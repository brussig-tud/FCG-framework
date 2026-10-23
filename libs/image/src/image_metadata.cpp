
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

// SDL3 library
#include <SDL3/SDL_properties.h>

// Local includes
#include "image_internal.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Stable copyable surface records in the same order as their public aggregate members.
constexpr std::array metadataNames{
	"fcg.image.metadata.primaries", "fcg.image.metadata.transfer", "fcg.image.metadata.range",
	"fcg.image.metadata.white", "fcg.image.metadata.headroom"
};

/// Access individual records without exposing numeric property indices to callers.
constexpr std::array metadataMembers{
	&fcg::ImageMetadataProvenance::colorPrimaries, &fcg::ImageMetadataProvenance::transferCharacteristics,
	&fcg::ImageMetadataProvenance::colorRange, &fcg::ImageMetadataProvenance::sdrWhitePoint,
	&fcg::ImageMetadataProvenance::hdrHeadroom
};

/// Encode optional brightness values by their exact bits, reserving -1 for absence.
[[nodiscard]] auto brightnessToken (SDL_PropertiesID properties, const char *name) -> Sint64 {
	return SDL_HasProperty(properties, name)
		? std::bit_cast<Uint32>(SDL_GetFloatProperty(properties, name, 0.f)) : Sint64(-1);
}

/// Bind provenance to the current individual values, independently of storage layout.
[[nodiscard]] auto metadataTokens (SDL_Surface &surface, SDL_PropertiesID properties) -> std::array<Sint64, 5>
{
	const auto encoding = SDL_GetSurfaceColorspace(&surface);
	return {SDL_COLORSPACEPRIMARIES(encoding), SDL_COLORSPACETRANSFER(encoding), SDL_COLORSPACERANGE(encoding),
		brightnessToken(properties, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT),
		brightnessToken(properties, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT)};
}

/// Measure native RGB through one read-only access session; alpha never participates in the peak.
template<SDL_PixelFormat Format>
[[nodiscard]] auto measurePeak (const fcg::Image &image) -> std::expected<double, fcg::ImageError>
{
	auto access = image.mapPixels<Format>();
	if (!access)
		return std::unexpected(access.error());
	// The session owns all borrowed rows and remains active until the complete scan has finished.
	auto view = access->view();
	double peak = 0.;
	for (const auto row : *view)
		for (int x = 0; x < row.size(); ++x)
		{
			const auto pixel = row.readUnchecked(x);
			for (const auto sample : {float(pixel.r), float(pixel.g), float(pixel.b)}) {
				if (!std::isfinite(sample))
					return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::InvalidArgument,
						"Headroom unavailable: RGB contains nonfinite samples"});
				peak = std::max(peak, double(sample));
			}
		}
	return peak;
}

/// Dispatch every native binary16/binary32 RGB order through the checked read-only scanner.
[[nodiscard]] auto measurePeak (const fcg::Image &image) -> std::expected<double, fcg::ImageError>
{
	switch (image.format())
	{
		case SDL_PIXELFORMAT_RGB48_FLOAT: return measurePeak<SDL_PIXELFORMAT_RGB48_FLOAT>(image);
		case SDL_PIXELFORMAT_BGR48_FLOAT: return measurePeak<SDL_PIXELFORMAT_BGR48_FLOAT>(image);
		case SDL_PIXELFORMAT_RGBA64_FLOAT: return measurePeak<SDL_PIXELFORMAT_RGBA64_FLOAT>(image);
		case SDL_PIXELFORMAT_ARGB64_FLOAT: return measurePeak<SDL_PIXELFORMAT_ARGB64_FLOAT>(image);
		case SDL_PIXELFORMAT_BGRA64_FLOAT: return measurePeak<SDL_PIXELFORMAT_BGRA64_FLOAT>(image);
		case SDL_PIXELFORMAT_ABGR64_FLOAT: return measurePeak<SDL_PIXELFORMAT_ABGR64_FLOAT>(image);
		case SDL_PIXELFORMAT_RGB96_FLOAT: return measurePeak<SDL_PIXELFORMAT_RGB96_FLOAT>(image);
		case SDL_PIXELFORMAT_BGR96_FLOAT: return measurePeak<SDL_PIXELFORMAT_BGR96_FLOAT>(image);
		case SDL_PIXELFORMAT_RGBA128_FLOAT: return measurePeak<SDL_PIXELFORMAT_RGBA128_FLOAT>(image);
		case SDL_PIXELFORMAT_ARGB128_FLOAT: return measurePeak<SDL_PIXELFORMAT_ARGB128_FLOAT>(image);
		case SDL_PIXELFORMAT_BGRA128_FLOAT: return measurePeak<SDL_PIXELFORMAT_BGRA128_FLOAT>(image);
		case SDL_PIXELFORMAT_ABGR128_FLOAT: return measurePeak<SDL_PIXELFORMAT_ABGR128_FLOAT>(image);
		default: return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::UnsupportedFormat,
			"Headroom measurement requires native floating-point RGB storage"});
	}
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// Private image-library helpers.
namespace fcg::detail {



//////
//
// Function implementations
//

auto readImageProvenance (SDL_Surface &surface) -> std::expected<ImageMetadataProvenance, ImageError>
{
	const auto properties = SDL_GetSurfaceProperties(&surface);
	if (!properties)
		return imageSDLError(ImageErrorCode::SDLFailure, "Reading image provenance");
	const auto tokens = metadataTokens(surface, properties);
	ImageMetadataProvenance result;
	for (std::size_t i = 0; i < metadataNames.size(); ++i)
	{
		const std::string base(metadataNames[i]);
		if (!SDL_HasProperty(properties, (base + ".record").c_str())
			|| SDL_GetNumberProperty(properties, (base + ".value").c_str(), -2) != tokens[i])
			continue;
		const auto packed = SDL_GetNumberProperty(properties, (base + ".record").c_str(), 0);
		if ((packed & 7) > static_cast<Sint64>(ImageMetadataOrigin::Assigned) || (packed & ~31) != 0)
			continue;
		auto &record = result.*metadataMembers[i];
		record.origin = static_cast<ImageMetadataOrigin>(packed & 7);
		record.adapted = (packed & 8) != 0;
		record.dependsOnAssumptions = (packed & 16) != 0;
		record.explanation = SDL_GetStringProperty(properties, (base + ".explanation").c_str(), "");
	}
	return result;
}

auto writeImageProvenance (SDL_Surface &surface, const ImageMetadataProvenance &provenance)
	-> std::expected<void, ImageError>
{
	const auto properties = SDL_GetSurfaceProperties(&surface);
	if (!properties)
		return imageSDLError(ImageErrorCode::SDLFailure, "Writing image provenance");
	const auto tokens = metadataTokens(surface, properties);
	for (std::size_t i = 0; i < metadataNames.size(); ++i)
	{
		const std::string base(metadataNames[i]);
		const auto &record = provenance.*metadataMembers[i];
		if (record.origin == ImageMetadataOrigin::Unknown && !record.adapted
			&& !record.dependsOnAssumptions && record.explanation.empty())
		{
			for (const auto *suffix : {".record", ".value", ".explanation"})
				if (!SDL_ClearProperty(properties, (base + suffix).c_str()))
					return imageSDLError(ImageErrorCode::SDLFailure, "Clearing image provenance");
			continue;
		}
		const auto packed = static_cast<Sint64>(record.origin) | (record.adapted ? 8 : 0)
			| (record.dependsOnAssumptions || record.origin == ImageMetadataOrigin::Assumed ? 16 : 0);
		if (!SDL_SetNumberProperty(properties, (base + ".value").c_str(), tokens[i])
			|| !SDL_SetNumberProperty(properties, (base + ".record").c_str(), packed)
			|| !SDL_SetStringProperty(properties, (base + ".explanation").c_str(), record.explanation.c_str()))
			return imageSDLError(ImageErrorCode::SDLFailure, "Writing image provenance");
	}
	return {};
}

auto invalidateImageMetadata (SDL_Surface &surface, ImageSurfaceChanges changes) -> std::expected<void, ImageError>
{
	const auto mask = static_cast<unsigned>(changes);
	if (mask == 0 || (mask & ~3u) != 0)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Invalid surface change categories"});
	auto provenance = readImageProvenance(surface);
	if (!provenance)
		return std::unexpected(provenance.error());
	if (mask & static_cast<unsigned>(ImageSurfaceChanges::ColorMetadata))
		*provenance = {};
	provenance->hdrHeadroom = {};
	if (!SDL_ClearProperty(SDL_GetSurfaceProperties(&surface), SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT))
		return imageSDLError(ImageErrorCode::SDLFailure, "Invalidating image headroom");
	return writeImageProvenance(surface, *provenance);
}

auto imageUsesAssumptions (const ImageMetadataProvenance &provenance) -> bool {
	return std::ranges::any_of(std::span(metadataMembers).first(4), [&provenance](auto member) {
		const auto &record = provenance.*member;
		return record.origin == ImageMetadataOrigin::Assumed || record.dependsOnAssumptions;
	});
}

auto refreshImageMetadata (Image &image, ImageHDRMetadataOptions options, bool replaceHeadroom)
	-> std::expected<ImageMetadataRefreshResult, ImageError>
{
	const auto info = image.pixelInfo();
	if (!info)
		return std::unexpected(info.error());
	ImageMetadataRefreshResult result;
	auto provenance = info->provenance;
	auto *surface = image.handle();
	const auto properties = SDL_GetSurfaceProperties(surface);
	const auto type = SDL_PIXELTYPE(surface->format);
	const bool eligible = (type == SDL_PIXELTYPE_ARRAYF16 || type == SDL_PIXELTYPE_ARRAYF32)
		&& nativePixelFormat(surface->format) && SDL_COLORSPACETYPE(info->colorspace) == SDL_COLOR_TYPE_RGB
		&& SDL_COLORSPACERANGE(info->colorspace) == SDL_COLOR_RANGE_FULL
		&& SDL_COLORSPACETRANSFER(info->colorspace) == SDL_TRANSFER_CHARACTERISTICS_LINEAR
		&& SDL_COLORSPACEMATRIX(info->colorspace) == SDL_MATRIX_COEFFICIENTS_IDENTITY
		&& SDL_COLORSPACECHROMA(info->colorspace) == SDL_CHROMA_LOCATION_NONE;
	if (!eligible)
		result.diagnostic = "Brightness completion requires full-range linear floating-point RGB";

	// Interpret missing relative-linear white independently of whether a peak can be measured.
	auto white = info->sdrWhitePoint;
	if (eligible && !white && options.assumeSDRWhitePoint)
	{
		white = 1.f;
		if (!SDL_SetFloatProperty(properties, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, *white))
			return imageSDLError(ImageErrorCode::SDLFailure, "Assigning assumed SDR white level");
		provenance.sdrWhitePoint = {ImageMetadataOrigin::Assumed, false, true,
			"Missing diffuse-white level interpreted as 1.0 in relative linear RGB"};
		result.whitePointAssumed = true;
	}
	const bool measure = options.deriveHDRHeadroom && (replaceHeadroom || !info->hdrHeadroom);
	if (!measure) {
		if (eligible || white || !options.assumeSDRWhitePoint)
			result.diagnostic.clear();
	}
	else if (eligible)
	{
		if (!white || !std::isfinite(*white) || *white <= 0.f)
			result.diagnostic = "Headroom unavailable: SDR white level must be present, finite and positive";
		else
		{
			const auto peak = measurePeak(std::as_const(image));
			if (!peak && peak.error().code != ImageErrorCode::InvalidArgument)
				return std::unexpected(peak.error());
			if (!peak)
				result.diagnostic = peak.error().message;
			else
			{
				const auto headroom = std::max(1., *peak / double(*white));
				if (!std::isfinite(headroom) || headroom > std::numeric_limits<float>::max())
					result.diagnostic = "Headroom unavailable: the peak-to-white ratio exceeds finite binary32 range";
				else
				{
					if (!SDL_SetFloatProperty(properties, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, float(headroom)))
						return imageSDLError(ImageErrorCode::SDLFailure, "Assigning measured image headroom");
					provenance.hdrHeadroom = {};
					provenance.hdrHeadroom = {ImageMetadataOrigin::Derived, false, imageUsesAssumptions(provenance),
						"Maximum stored linear RGB component divided by SDR white, with a minimum headroom of 1"};
					result.headroomDerived = true;
				}
			}
		}
	}

	// Drop a stale computed value when an explicit remeasurement cannot establish a replacement.
	if (measure && !result.headroomDerived && info->provenance.hdrHeadroom.origin == ImageMetadataOrigin::Derived) {
		if (!SDL_ClearProperty(properties, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT))
			return imageSDLError(ImageErrorCode::SDLFailure, "Clearing unavailable derived headroom");
		provenance.hdrHeadroom = {};
	}
	if (auto recorded = writeImageProvenance(*surface, provenance); !recorded)
		return std::unexpected(recorded.error());
	return result;
}

auto convertedImageMetadata (SDL_Surface &surface, const ImagePixelInfo &source, bool preserveRGB)
	-> std::expected<void, ImageError>
{
	auto provenance = source.provenance;
	const auto target = SDL_GetSurfaceColorspace(&surface);
	const auto assigned = ImagePropertyProvenance{ImageMetadataOrigin::Assigned, true,
		imageUsesAssumptions(source.provenance), "Destination interpretation selected by explicit image conversion"};
	if (SDL_COLORSPACEPRIMARIES(source.colorspace) != SDL_COLORSPACEPRIMARIES(target))
		provenance.colorPrimaries = assigned;
	if (SDL_COLORSPACETRANSFER(source.colorspace) != SDL_COLORSPACETRANSFER(target))
		provenance.transferCharacteristics = assigned;
	if (SDL_COLORSPACERANGE(source.colorspace) != SDL_COLORSPACERANGE(target))
		provenance.colorRange = assigned;
	if (source.colorspace != target)
	{
		// SDL converts into the destination's documented default white units; expose that known output choice.
		provenance.sdrWhitePoint = {};
		const auto transfer = SDL_COLORSPACETRANSFER(target);
		if (transfer == SDL_TRANSFER_CHARACTERISTICS_LINEAR || transfer == SDL_TRANSFER_CHARACTERISTICS_PQ)
		{
			const bool sourcePQ = SDL_COLORSPACETRANSFER(source.colorspace) == SDL_TRANSFER_CHARACTERISTICS_PQ;
			const bool targetPQ = transfer == SDL_TRANSFER_CHARACTERISTICS_PQ;
			const float white = targetPQ ? 203.f : 1.f;
			if (!SDL_SetFloatProperty(SDL_GetSurfaceProperties(&surface), SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, white))
				return imageSDLError(ImageErrorCode::SDLFailure, "Recording destination SDR white units");
			provenance.sdrWhitePoint = {ImageMetadataOrigin::Assigned,
				sourcePQ != targetPQ || source.sdrWhitePoint.value_or(sourcePQ ? 203.f : 1.f) != white,
				imageUsesAssumptions(source.provenance), "Destination default SDR white units selected by image conversion"};
		}
	}
	if (!preserveRGB) {
		if (!SDL_ClearProperty(SDL_GetSurfaceProperties(&surface), SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT))
			return imageSDLError(ImageErrorCode::SDLFailure, "Clearing converted image headroom");
		provenance.hdrHeadroom = {};
	}
	return writeImageProvenance(surface, provenance);
}



//////
//
// Module namespace close
//

// namespace fcg::detail
}
