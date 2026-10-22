
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <utility>

// SDL3 library
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_surface.h>

// Local includes
#include "image_internal.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Surface property names shared by all annotation readers, writers and conversion copies.
constexpr std::array loadAdjustmentProperties{
	"fcg.image.load.color_primaries", "fcg.image.load.transfer_characteristics",
	"fcg.image.load.color_range", "fcg.image.load.colorspace"
};

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

auto filterImageLoadAdjustments (
	ImageLoadAdjustments adjustments, SDL_Colorspace recorded, SDL_Colorspace current
) -> ImageLoadAdjustments
{
	if (SDL_COLORSPACEPRIMARIES(recorded) != SDL_COLORSPACEPRIMARIES(current))
		adjustments.colorPrimaries = ImageLoadAdjustment::None;
	if (SDL_COLORSPACETRANSFER(recorded) != SDL_COLORSPACETRANSFER(current))
		adjustments.transferCharacteristics = ImageLoadAdjustment::None;
	if (SDL_COLORSPACERANGE(recorded) != SDL_COLORSPACERANGE(current))
		adjustments.colorRange = ImageLoadAdjustment::None;
	return adjustments;
}

auto readImageLoadAdjustments (SDL_Surface &surface) -> std::expected<ImageLoadAdjustments, ImageError>
{
	const auto properties = SDL_GetSurfaceProperties(&surface);
	if (!properties)
		return imageSDLError(ImageErrorCode::SDLFailure, "Reading image loading annotations");
	if (!SDL_HasProperty(properties, loadAdjustmentProperties[3]))
		return ImageLoadAdjustments{};
	const auto read = [properties] (const char *name) {
		const auto value = SDL_GetNumberProperty(properties, name, 0);
		return value >= 0 && value <= 3 ? static_cast<ImageLoadAdjustment>(value) : ImageLoadAdjustment::None;
	};
	const auto recorded = static_cast<SDL_Colorspace>(SDL_GetNumberProperty(properties, loadAdjustmentProperties[3], 0));
	return filterImageLoadAdjustments({read(loadAdjustmentProperties[0]), read(loadAdjustmentProperties[1]),
		read(loadAdjustmentProperties[2])}, recorded, SDL_GetSurfaceColorspace(&surface));
}

auto writeImageLoadAdjustments (SDL_Surface &surface, ImageLoadAdjustments adjustments)
	-> std::expected<void, ImageError>
{
	const auto properties = SDL_GetSurfaceProperties(&surface);
	if (!properties)
		return imageSDLError(ImageErrorCode::SDLFailure, "Writing image loading annotations");
	// Clear copied records too: explicitly converted components must not regain old annotations after SDL edits.
	if (adjustments.colorPrimaries == ImageLoadAdjustment::None
		&& adjustments.transferCharacteristics == ImageLoadAdjustment::None
		&& adjustments.colorRange == ImageLoadAdjustment::None) {
		for (const auto *name : loadAdjustmentProperties)
			if (SDL_HasProperty(properties, name) && !SDL_ClearProperty(properties, name))
				return imageSDLError(ImageErrorCode::SDLFailure, "Clearing image loading annotations");
		return {};
	}
	const std::array values{static_cast<Sint64>(adjustments.colorPrimaries),
		static_cast<Sint64>(adjustments.transferCharacteristics), static_cast<Sint64>(adjustments.colorRange),
		static_cast<Sint64>(SDL_GetSurfaceColorspace(&surface))};
	for (std::size_t i = 0; i < values.size(); ++i)
		if (!SDL_SetNumberProperty(properties, loadAdjustmentProperties[i], values[i]))
			return imageSDLError(ImageErrorCode::SDLFailure, "Writing image loading annotations");
	return {};
}

auto imageRGBColorspace (SDL_ColorPrimaries primaries, SDL_TransferCharacteristics transfer) -> SDL_Colorspace {
	return static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL,
		primaries, transfer, SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE));
}

auto missingImageEncoding (SDL_Colorspace colorspace) -> bool
{
	const auto primaries = SDL_COLORSPACEPRIMARIES(colorspace);
	const auto transfer = SDL_COLORSPACETRANSFER(colorspace);
	return SDL_COLORSPACETYPE(colorspace) == SDL_COLOR_TYPE_UNKNOWN
		|| SDL_COLORSPACERANGE(colorspace) == SDL_COLOR_RANGE_UNKNOWN
		|| primaries == SDL_COLOR_PRIMARIES_UNKNOWN || primaries == SDL_COLOR_PRIMARIES_UNSPECIFIED
		|| transfer == SDL_TRANSFER_CHARACTERISTICS_UNKNOWN || transfer == SDL_TRANSFER_CHARACTERISTICS_UNSPECIFIED;
}

auto fillImageEncoding (SDL_Colorspace source, SDL_Colorspace fallback) -> SDL_Colorspace
{
	auto primaries = SDL_COLORSPACEPRIMARIES(source);
	auto transfer = SDL_COLORSPACETRANSFER(source);
	auto range = SDL_COLORSPACERANGE(source);
	auto type = SDL_COLORSPACETYPE(source);
	if (primaries == SDL_COLOR_PRIMARIES_UNKNOWN || primaries == SDL_COLOR_PRIMARIES_UNSPECIFIED)
		primaries = SDL_COLORSPACEPRIMARIES(fallback);
	if (transfer == SDL_TRANSFER_CHARACTERISTICS_UNKNOWN || transfer == SDL_TRANSFER_CHARACTERISTICS_UNSPECIFIED)
		transfer = SDL_COLORSPACETRANSFER(fallback);
	if (range == SDL_COLOR_RANGE_UNKNOWN)
		range = SDL_COLORSPACERANGE(fallback);
	if (type == SDL_COLOR_TYPE_UNKNOWN)
		type = SDL_COLORSPACETYPE(fallback);
	return static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(type, range, primaries, transfer,
		SDL_COLORSPACEMATRIX(source), SDL_COLORSPACECHROMA(source)));
}

auto supportedImageEncoding (SDL_Colorspace colorspace) -> bool
{
	const auto primaries = SDL_COLORSPACEPRIMARIES(colorspace);
	const auto transfer = SDL_COLORSPACETRANSFER(colorspace);
	return SDL_COLORSPACETYPE(colorspace) == SDL_COLOR_TYPE_RGB
		&& SDL_COLORSPACERANGE(colorspace) == SDL_COLOR_RANGE_FULL
		&& SDL_COLORSPACEMATRIX(colorspace) == SDL_MATRIX_COEFFICIENTS_IDENTITY
		&& SDL_COLORSPACECHROMA(colorspace) == SDL_CHROMA_LOCATION_NONE
		&& (primaries == SDL_COLOR_PRIMARIES_BT709 || primaries == SDL_COLOR_PRIMARIES_BT601
			|| primaries == SDL_COLOR_PRIMARIES_SMPTE240 || primaries == SDL_COLOR_PRIMARIES_BT2020
			|| primaries == SDL_COLOR_PRIMARIES_SMPTE431 || primaries == SDL_COLOR_PRIMARIES_SMPTE432)
		&& (transfer == SDL_TRANSFER_CHARACTERISTICS_LINEAR || transfer == SDL_TRANSFER_CHARACTERISTICS_SRGB
			|| transfer == SDL_TRANSFER_CHARACTERISTICS_PQ);
}

auto imageColorPrimaries (const std::array<float, 8> &chromaticities) -> SDL_ColorPrimaries
{
	// CICP primary coordinates and their reference white points, in RGB/white xy order.
	static constexpr std::array<std::pair<SDL_ColorPrimaries, std::array<float, 8>>, 10> standards{{
		{SDL_COLOR_PRIMARIES_BT709, {.64f, .33f, .30f, .60f, .15f, .06f, .3127f, .3290f}},
		{SDL_COLOR_PRIMARIES_BT470M, {.67f, .33f, .21f, .71f, .14f, .08f, .310f, .316f}},
		{SDL_COLOR_PRIMARIES_BT470BG, {.64f, .33f, .29f, .60f, .15f, .06f, .3127f, .3290f}},
		{SDL_COLOR_PRIMARIES_BT601, {.63f, .34f, .31f, .595f, .155f, .07f, .3127f, .3290f}},
		{SDL_COLOR_PRIMARIES_GENERIC_FILM, {.681f, .319f, .243f, .692f, .145f, .049f, .310f, .316f}},
		{SDL_COLOR_PRIMARIES_BT2020, {.708f, .292f, .170f, .797f, .131f, .046f, .3127f, .3290f}},
		{SDL_COLOR_PRIMARIES_XYZ, {1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f / 3, 1.f / 3}},
		{SDL_COLOR_PRIMARIES_SMPTE431, {.68f, .32f, .265f, .69f, .15f, .06f, .314f, .351f}},
		{SDL_COLOR_PRIMARIES_SMPTE432, {.68f, .32f, .265f, .69f, .15f, .06f, .3127f, .3290f}},
		{SDL_COLOR_PRIMARIES_EBU3213, {.63f, .34f, .295f, .605f, .155f, .077f, .3127f, .3290f}}
	}};
	for (const auto &[primaries, expected] : standards)
	{
		bool match = true;
		for (std::size_t i = 0; i < expected.size(); ++i)
			match &= std::isfinite(chromaticities[i]) && std::abs(chromaticities[i] - expected[i]) <= .0001f;
		if (match)
			return primaries;
	}
	return SDL_COLOR_PRIMARIES_CUSTOM;
}



//////
//
// Module namespace close
//

// namespace fcg::detail
}
