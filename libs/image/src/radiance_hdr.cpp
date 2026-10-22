
//////
//
// Includes
//

// C++ STL
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

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

	/// Source encoding; missing components remain unspecified and unmatched declarations are custom.
	SDL_Colorspace colorspace;

	/// Original declared color interpretation and radiometric adjustments, retained without changing samples.
	std::string colorRecords;
};

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
	SDL_Colorspace colorspace = unidentifiedImageColorspace(true);
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
			colorspace = imageRGBColorspace(imageColorPrimaries(values), SDL_COLORSPACETRANSFER(colorspace));
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
			const auto transfer = gamma[0] == 1.f ? SDL_TRANSFER_CHARACTERISTICS_LINEAR
				: gamma[0] == 2.2f ? SDL_TRANSFER_CHARACTERISTICS_GAMMA22
				: gamma[0] == 2.8f ? SDL_TRANSFER_CHARACTERISTICS_GAMMA28 : SDL_TRANSFER_CHARACTERISTICS_CUSTOM;
			colorspace = imageRGBColorspace(SDL_COLORSPACEPRIMARIES(colorspace), transfer);
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
	return HDRHeader{*width, *height, colorspace, std::move(colorRecords)};
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

auto RadianceHDRFormatHandler::load (
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
	int width = 0, height = 0, channels = 0;
	std::unique_ptr<float, decltype(&stbi_image_free)> pixels(stbi_loadf_from_memory(
		reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &width, &height, &channels, 3),
		stbi_image_free);
	if (!pixels)
		return detail::imageDecodeError(std::string("Decoding Radiance HDR: ")
			+ (stbi_failure_reason() ? stbi_failure_reason() : "stb_image failed"));
	if (width != header->width || height != header->height)
		return detail::imageDecodeError("Radiance HDR dimensions disagree with its header");
	// Copy decoder-owned storage into a surface that can outlive the stb allocation.
	auto surface = detail::floatImageSurface(width, height, false, header->colorspace);
	if (!surface)
		return std::unexpected(std::move(surface.error()));
	if (!header->colorRecords.empty() && !SDL_SetStringProperty(SDL_GetSurfaceProperties(surface->get()),
		"fcg.image.source.color_metadata", header->colorRecords.c_str()))
		return detail::imageSDLError(ImageErrorCode::SDLFailure, "Retaining Radiance color metadata");
	for (int y = 0; y < height; ++y)
		std::memcpy(static_cast<std::byte*>((*surface)->pixels) + std::size_t(y) * (*surface)->pitch,
			pixels.get() + std::size_t(y) * width * 3, std::size_t(width) * 3 * sizeof(float));
	return finishLoading(detail::adoptImageSurface(std::move(*surface)), options);
}



//////
//
// Module namespace close
//

// namespace fcg
}
