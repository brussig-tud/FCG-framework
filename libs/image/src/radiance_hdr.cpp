
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

// GLM library
#include <glm/glm.hpp>

// stb_image library: all symbols and configuration remain in this translation unit.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_HDR
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

// Local includes
#include "FCG/Image/radiance_hdr.h"
#include "hdr_internal.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Validated Radiance dimensions and representable color interpretation.
struct HDRHeader
{
	/// Positive output width.
	int width;

	/// Positive output height.
	int height;

	/// Complete declared RGB/white xy coordinates, available for conversion even for custom gamuts.
	std::optional<std::array<float, 8>> primaries;

	/// Power-law exponent describing the stored RGB values.
	float gamma;

	/// Validated flat RGBE raster borrowed from the input; empty for scanline RLE.
	std::span<const std::byte> flatRaster;

	/// Original declared color interpretation and radiometric adjustments.
	std::string colorRecords;
};

/// Output encoding and the loading actions performed while normalizing decoded Radiance samples.
struct NormalizedHDR
{
	/// Encoding of the normalized samples.
	SDL_Colorspace colorspace;

	/// Guessed source components and adaptations performed before surface creation.
	fcg::ImageMetadataProvenance provenance;
};

/// Reject singular or numerically unstable color bases before inverting them.
[[nodiscard]] auto inverseColorMatrix (const glm::dmat3 &matrix) -> std::expected<glm::dmat3, fcg::ImageError>
{
	double magnitude = 0;
	for (int c = 0; c < 3; ++c)
		for (int r = 0; r < 3; ++r) {
			if (!std::isfinite(matrix[c][r]))
				return fcg::detail::imageDecodeError("Nonfinite Radiance color matrix");
			magnitude = std::max(magnitude, std::abs(matrix[c][r]));
		}
	if (magnitude == 0 || std::abs(glm::determinant(matrix / magnitude)) <= 1e-12)
		return fcg::detail::imageDecodeError("Singular Radiance color primaries");
	return glm::inverse(matrix);
}

/// Construct an RGB-to-XYZ matrix with unit white luminance, including XYZ-style primaries with zero y.
[[nodiscard]] auto rgbToXYZ (const std::array<float, 8> &xy) -> std::expected<glm::dmat3, fcg::ImageError>
{
	if (xy[6] <= 0 || xy[7] <= 0 || double(xy[6]) + xy[7] >= 1)
		return fcg::detail::imageDecodeError("Invalid Radiance white chromaticity");
	// Homogeneous chromaticities also represent the zero-y primaries used for an XYZ basis.
	glm::dmat3 basis;
	for (int c = 0; c < 3; ++c)
		basis[c] = glm::dvec3(xy[c * 2], xy[c * 2 + 1], 1. - xy[c * 2] - xy[c * 2 + 1]);
	const auto inverse = inverseColorMatrix(basis);
	if (!inverse)
		return std::unexpected(inverse.error());
	// Scale the primary columns to reproduce source white at unit luminance.
	const glm::dvec3 white(double(xy[6]) / xy[7], 1., (1. - xy[6] - xy[7]) / xy[7]);
	const auto weights = *inverse * white;
	for (int c = 0; c < 3; ++c)
		basis[c] *= weights[c];
	if (auto checked = inverseColorMatrix(basis); !checked)
		return std::unexpected(checked.error());
	return basis;
}

/// Adapt source white with Bradford cone responses and express linear RGB in BT.709/D65.
[[nodiscard]] auto radianceToBT709 (const glm::dmat3 &source) -> std::expected<glm::dmat3, fcg::ImageError>
{
	const auto target = rgbToXYZ({.64f, .33f, .30f, .60f, .15f, .06f, .3127f, .3290f});
	if (!target)
		return std::unexpected(target.error());
	// Apply independent cone-response ratios between the source white and D65.
	const glm::dmat3 bradford{{.8951, -.7502, .0389}, {.2664, 1.7135, -.0685}, {-.1614, .0367, 1.0296}};
	const auto sourceWhite = bradford * source * glm::dvec3(1.);
	const auto targetWhite = bradford * *target * glm::dvec3(1.);
	glm::dmat3 adaptation(1.);
	for (int c = 0; c < 3; ++c) {
		if (!std::isfinite(sourceWhite[c]) || sourceWhite[c] <= 0)
			return fcg::detail::imageDecodeError("Invalid Radiance white adaptation");
		adaptation[c][c] = targetWhite[c] / sourceWhite[c];
	}
	return glm::inverse(*target) * glm::inverse(bradford) * adaptation * bradford * source;
}

/// Resolve permitted source guesses and normalize known unsupported encodings before allocating a surface.
[[nodiscard]] auto normalizeHDR (
	std::span<float> pixels, const HDRHeader &header, fcg::ImageLoaderOptions options
) -> std::expected<NormalizedHDR, fcg::ImageError>
{
	using namespace fcg::detail;
	fcg::ImageMetadataProvenance provenance;
	auto primaries = header.primaries;
	// Radiance's conventional green primary and equal-energy white differ from Rec.709/D65.
	if (!primaries && options.inferColorPrimaries) {
		primaries = std::array{.64f, .33f, .29f, .60f, .15f, .06f, 1.f / 3, 1.f / 3};
		provenance.colorPrimaries = {fcg::ImageMetadataOrigin::Assumed, false, true,
			"Missing Radiance primaries interpreted using the conventional RGB/equal-energy-white gamut"};
	}
	if (header.primaries)
		provenance.colorPrimaries = {fcg::ImageMetadataOrigin::Declared, false, false, "Radiance PRIMARIES record"};
	provenance.transferCharacteristics = {fcg::ImageMetadataOrigin::FormatDefined, false, false,
		"Radiance RGBE uses linear light after decoding the declared gamma"};
	provenance.colorRange = {fcg::ImageMetadataOrigin::FormatDefined, false, false, "Radiance stores full-range RGB"};
	auto encoding = imageRGBColorspace(primaries ? imageColorPrimaries(*primaries) : SDL_COLOR_PRIMARIES_UNSPECIFIED,
		SDL_TRANSFER_CHARACTERISTICS_LINEAR);
	glm::dmat3 transform(1.);
	bool convertGamut = false;
	if (primaries)
	{
		const auto source = rgbToXYZ(*primaries);
		if (!source)
			return std::unexpected(source.error());
		if (!supportedImageEncoding(encoding))
		{
			const auto conversion = radianceToBT709(*source);
			if (!conversion)
				return std::unexpected(conversion.error());
			transform = *conversion;
			convertGamut = true;
			encoding = SDL_COLORSPACE_SRGB_LINEAR;
			provenance.colorPrimaries = {fcg::ImageMetadataOrigin::Assigned, true,
				provenance.colorPrimaries.dependsOnAssumptions,
				"Radiance source gamut adapted to linear BT.709/D65 using Bradford white adaptation"};
		}
	}
	if (!convertGamut && header.gamma == 1.f)
		return NormalizedHDR{encoding, provenance};
	if (header.gamma != 1.f)
		provenance.transferCharacteristics = {fcg::ImageMetadataOrigin::Assigned, true, false,
			"Declared Radiance GAMMA decoded to linear light"};
	// Decode the transfer before mixing channels; exposure and COLORCORR remain stored in the values.
	for (std::size_t i = 0; i < pixels.size(); i += 3)
	{
		glm::dvec3 rgb(pixels[i], pixels[i + 1], pixels[i + 2]);
		if (header.gamma != 1.f)
			for (int c = 0; c < 3; ++c)
				rgb[c] = std::pow(rgb[c], header.gamma);
		if (convertGamut)
			rgb = transform * rgb;
		for (int c = 0; c < 3; ++c) {
			if (!std::isfinite(rgb[c]) || std::abs(rgb[c]) > std::numeric_limits<float>::max())
				return imageDecodeError("Radiance color conversion exceeds finite binary32 storage");
			pixels[i + c] = static_cast<float>(rgb[c]);
		}
	}
	return NormalizedHDR{encoding, provenance};
}

/// Recognize only the identifiers stb's HDR decoder supports.
[[nodiscard]] auto isRadiance (std::span<const std::byte> bytes) -> bool {
	const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	return text.starts_with("#?RADIANCE\n") || text.starts_with("#?RGBE\n");
}

/// Validate every byte consumed by stb, including flat reads whose short-read status it does not check.
[[nodiscard]] auto validateHDR (std::span<const std::byte> bytes) -> std::expected<HDRHeader, fcg::ImageError>
{
	using namespace fcg::detail;
	std::string_view input(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	(void) imageHeaderLine(input);
	bool format = false;
	std::string colorRecords;
	std::optional<std::array<float, 8>> declaredPrimaries;
	std::optional<float> declaredGamma;
	const auto parseValues = [] (std::string_view text, auto &values)
	{
		for (auto &value : values)
		{
			auto token = imageHeaderToken(text);
			if (token.starts_with('+'))
				token.remove_prefix(1);
			if (token.empty())
				return false;
			const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
			if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || !std::isfinite(value))
				return false;
		}
		return trimImageHeader(text).empty();
	};
	// Parse only bounded records that stb will interpret identically.
	for (;;)
	{
		const auto line = imageHeaderLine(input);
		if (!line || line->size() >= 1023)
			return imageDecodeError("Missing or oversized Radiance HDR header record");
		if (line->empty())
			break;
		if (line->starts_with("FORMAT=")) {
			if (format || *line != "FORMAT=32-bit_rle_rgbe")
				return imageDecodeError("Only RGBE Radiance HDR is supported");
			format = true;
		}
		if (line->starts_with("PRIMARIES="))
		{
			std::array<float, 8> values{};
			if (!parseValues(line->substr(10), values))
				return imageDecodeError("Invalid Radiance HDR primaries");
			if (declaredPrimaries && *declaredPrimaries != values)
				return imageDecodeError("Conflicting Radiance HDR primaries");
			declaredPrimaries = values;
			colorRecords += std::string(*line) + '\n';
		}
		if (line->starts_with("GAMMA="))
		{
			std::array<float, 1> gamma{};
			if (!parseValues(line->substr(6), gamma) || gamma[0] <= 0)
				return imageDecodeError("Invalid Radiance HDR gamma");
			if (declaredGamma && *declaredGamma != gamma[0])
				return imageDecodeError("Conflicting Radiance HDR gamma");
			declaredGamma = gamma[0];
			colorRecords += std::string(*line) + '\n';
		}
		if (line->starts_with("EXPOSURE=") || line->starts_with("COLORCORR="))
			colorRecords += std::string(*line) + '\n';
	}
	const auto resolution = imageHeaderLine(input);
	// Restrict layout and allocation arithmetic before inspecting scanline packets.
	if (!format || !resolution || resolution->size() >= 1023)
		return imageDecodeError("Missing Radiance HDR format or resolution");
	auto layout = *resolution;
	if (imageHeaderToken(layout) != "-Y")
		return imageDecodeError("Radiance HDR requires -Y height +X width orientation");
	const auto height = imageDimension(imageHeaderToken(layout));
	if (imageHeaderToken(layout) != "+X")
		return imageDecodeError("Radiance HDR requires -Y height +X width orientation");
	const auto width = imageDimension(imageHeaderToken(layout));
	if (!width || !height || !trimImageHeader(layout).empty()
		|| std::uint64_t(*width) * *height > std::numeric_limits<int>::max() / (3 * sizeof(float)))
		return imageDecodeError("Invalid or unsupported Radiance HDR dimensions");
	const auto byte = [&input] (std::size_t index) { return static_cast<unsigned char>(input[index]); };
	const bool rle = *width >= 8 && *width < 32768 && input.size() >= 4
		&& byte(0) == 2 && byte(1) == 2 && !(byte(2) & 128);
	const auto flatRaster = rle ? std::span<const std::byte>{} : bytes.last(input.size());
	// Count encoded packet bytes without reconstructing pixels; stb performs the actual decode.
	if (rle)
		for (int y = 0; y < *height; ++y)
		{
			if (input.size() < 4 || byte(0) != 2 || byte(1) != 2 || (byte(2) * 256 + byte(3)) != *width)
				return imageDecodeError("Invalid or truncated Radiance HDR scanline");
			input.remove_prefix(4);
			for (int c = 0; c < 4; ++c)
				for (int remaining = *width; remaining > 0;)
				{
					if (input.empty())
						return imageDecodeError("Truncated Radiance HDR RLE packet");
					const auto packet = byte(0);
					input.remove_prefix(1);
					const int count = packet > 128 ? packet - 128 : packet;
					const auto encoded = packet > 128 ? 1u : packet;
					if (!count || count > remaining || encoded > input.size())
						return imageDecodeError("Invalid or truncated Radiance HDR RLE packet");
					remaining -= count;
					input.remove_prefix(encoded);
				}
		}
	else
	{
		if (std::uint64_t(*width) * *height * 4 != input.size())
			return imageDecodeError("Radiance HDR flat raster length does not match its dimensions");
		for (std::size_t i = 0; i < input.size(); i += 4)
			if (byte(i) == 1 && byte(i + 1) == 1 && byte(i + 2) == 1)
				return imageDecodeError("Legacy Radiance HDR repeat markers are unsupported");
		input = {};
	}
	if (!input.empty())
		return imageDecodeError("Unexpected trailing Radiance HDR raster data");
	return HDRHeader{*width, *height, declaredPrimaries, declaredGamma.value_or(1.f), flatRaster, std::move(colorRecords)};
}

/// Keep the HDR handler in ordinary static archive links.
const fcg::FormatHandlerRegistration hdrRegistration{"radiance_hdr", std::make_unique<fcg::RadianceHDRFormatHandler>(), -90};

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Function implementations
//

// Private implementation helpers.
namespace detail {

void linkRadianceHDRHandler () {
	(void) &hdrRegistration;
}

// namespace detail
}



//////
//
// Class implementations
//

////
// RadianceHDRFormatHandler

auto RadianceHDRFormatHandler::fileFormats () const -> std::vector<ImageFileFormat> {
	return {{"Radiance HDR", {"hdr", "rgbe"}}};
}

auto RadianceHDRFormatHandler::accepts (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<bool, ImageError> {
	if (auto normalized = detail::imageHint(bytes, hint); !normalized)
		return std::unexpected(std::move(normalized.error()));
	return isRadiance(bytes);
}

auto RadianceHDRFormatHandler::decode (
	std::span<const std::byte> bytes, std::string_view hint, ImageLoaderOptions options
) const -> std::expected<Image, ImageError>
{
	if (auto accepted = accepts(bytes, hint); !accepted || !*accepted)
		return accepted ? std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Not a Radiance HDR image"})
			: std::unexpected(std::move(accepted.error()));
	if (bytes.size() > std::numeric_limits<int>::max())
		return detail::imageDecodeError("Radiance HDR input exceeds stb_image's supported byte count");
	// Reject truncation before calling stb's permissive HDR reader.
	const auto header = validateHDR(bytes);
	if (!header)
		return std::unexpected(header.error());
	const auto count = std::size_t(header->width) * header->height * 3;
	std::vector<float> flatPixels;
	std::unique_ptr<float, decltype(&stbi_image_free)> rlePixels(nullptr, stbi_image_free);
	std::span<float> pixels;
	if (!header->flatRaster.empty())
	{
		// Avoid stb's cross-loop flat fallback, which can omit rows in optimized builds.
		flatPixels.resize(count);
		for (std::size_t i = 0; i < count / 3; ++i)
		{
			const auto rgbe = header->flatRaster.subspan(i * 4, 4);
			const auto exponent = std::to_integer<int>(rgbe[3]);
			const float factor = exponent ? std::ldexp(1.f, exponent - 136) : 0.f;
			for (unsigned c = 0; c < 3; ++c)
				flatPixels[i * 3 + c] = std::to_integer<unsigned>(rgbe[c]) * factor;
		}
		pixels = flatPixels;
	}
	else
	{
		int width = 0, height = 0, channels = 0;
		rlePixels.reset(stbi_loadf_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
			static_cast<int>(bytes.size()), &width, &height, &channels, 3));
		if (!rlePixels)
			return detail::imageDecodeError(std::string("Decoding Radiance HDR: ")
				+ (stbi_failure_reason() ? stbi_failure_reason() : "stb_image failed"));
		if (width != header->width || height != header->height)
			return detail::imageDecodeError("Radiance HDR dimensions disagree with its header");
		pixels = {rlePixels.get(), count};
	}
	const auto normalized = normalizeHDR(pixels, *header, options);
	if (!normalized)
		return std::unexpected(normalized.error());
	// Copy decoder-owned storage into a surface that can outlive the stb allocation.
	auto surface = detail::floatImageSurface(header->width, header->height, false, normalized->colorspace);
	if (!surface)
		return std::unexpected(std::move(surface.error()));
	if (!header->colorRecords.empty() && !SDL_SetStringProperty(SDL_GetSurfaceProperties(surface->get()),
		"fcg.image.source.color_metadata", header->colorRecords.c_str()))
		return detail::imageSDLError(ImageErrorCode::SDLFailure, "Retaining Radiance color metadata");
	for (int y = 0; y < header->height; ++y)
		std::memcpy(static_cast<std::byte*>((*surface)->pixels) + std::size_t(y) * (*surface)->pitch,
			pixels.data() + std::size_t(y) * header->width * 3, std::size_t(header->width) * 3 * sizeof(float));
	return resolveMetadata(detail::adoptImageSurface(std::move(*surface)), options, {}, normalized->provenance);
}



//////
//
// Module namespace close
//

// namespace fcg
}
