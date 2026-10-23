
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <utility>

// SDL3 library
#include <SDL3/SDL_pixels.h>

// Local includes
#include "image_internal.h"



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
