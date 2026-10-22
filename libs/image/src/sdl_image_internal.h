
#ifndef __FCG_SDL_IMAGE_INTERNAL_H__
#define __FCG_SDL_IMAGE_INTERNAL_H__


//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <span>
#include <string_view>

// SDL3 library
#include <SDL3/SDL_pixels.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Private helpers for the SDL_image decoder.
namespace detail {



//////
//
// Functions
//

/// Choose candidates for missing components from the decoded format, source signature and remaining metadata.
[[nodiscard]] inline auto guessedSDLImageEncoding (
	std::span<const std::byte> bytes, std::string_view type, SDL_PixelFormat format, SDL_Colorspace encoding
) -> SDL_Colorspace
{
	const auto transfer = SDL_COLORSPACETRANSFER(encoding);
	const auto primaries = transfer == SDL_TRANSFER_CHARACTERISTICS_PQ || transfer == SDL_TRANSFER_CHARACTERISTICS_HLG
		? SDL_COLOR_PRIMARIES_BT2020 : SDL_COLOR_PRIMARIES_BT709;
	auto curve = SDL_ISPIXELFORMAT_FLOAT(format)
		? SDL_TRANSFER_CHARACTERISTICS_LINEAR : SDL_TRANSFER_CHARACTERISTICS_SRGB;
	// Inspect only the already selected format; filename hints cannot override a detected signature.
	const std::string_view source(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	if ((type == "QOI" || type == "qoi") && source.size() >= 14 && source.starts_with("qoif")) {
		if (bytes[13] == std::byte{0})
			curve = SDL_TRANSFER_CHARACTERISTICS_SRGB;
		else if (bytes[13] == std::byte{1})
			curve = SDL_TRANSFER_CHARACTERISTICS_LINEAR;
	} else if ((type == "PNM" || type == "pnm" || type == "pgm" || type == "ppm")
		&& source.size() >= 2 && source[0] == 'P' && std::string_view("2356").contains(source[1]))
		curve = SDL_TRANSFER_CHARACTERISTICS_BT709;
	return static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL,
		primaries, curve, SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE));
}



//////
//
// Namespaces close
//

// namespace detail
}

// namespace fcg
}


#endif // ifndef __FCG_SDL_IMAGE_INTERNAL_H__
