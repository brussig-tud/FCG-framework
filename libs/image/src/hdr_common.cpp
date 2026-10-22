
//////
//
// Includes
//

// C++ STL
#include <charconv>
#include <limits>
#include <utility>

// SDL3 library
#include <SDL3/SDL_properties.h>

// Local includes
#include "hdr_internal.h"



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

auto trimImageHeader (std::string_view text) -> std::string_view {
	const auto first = text.find_first_not_of(" \t\r");
	return first == text.npos ? std::string_view{} : text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

auto imageHeaderLine (std::string_view &input) -> std::optional<std::string_view> {
	const auto end = input.find('\n');
	if (end == input.npos)
		return std::nullopt;
	const auto line = input.substr(0, end);
	input.remove_prefix(end + 1);
	return line;
}

auto imageHeaderToken (std::string_view &line) -> std::string_view
{
	line = trimImageHeader(line);
	const auto end = line.find_first_of(" \t\r");
	const auto token = line.substr(0, end);
	line.remove_prefix(token.size());
	return token;
}

auto imageDimension (std::string_view token) -> std::optional<int>
{
	if (token.empty())
		return std::nullopt;
	int value = 0;
	const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
	if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || value <= 0)
		return std::nullopt;
	return value;
}

auto unidentifiedImageColorspace (bool linear) -> SDL_Colorspace {
	return static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(
		SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL, SDL_COLOR_PRIMARIES_UNSPECIFIED,
		linear ? SDL_TRANSFER_CHARACTERISTICS_LINEAR : SDL_TRANSFER_CHARACTERISTICS_UNSPECIFIED,
		SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE));
}

auto floatImageSurface (std::int64_t width, std::int64_t height, bool alpha, SDL_Colorspace colorspace)
	-> std::expected<ImageSurface, ImageError>
{
	const auto pixelBytes = alpha ? 16 : 12;
	if (width <= 0 || height <= 0 || width > (std::numeric_limits<int>::max() - 3) / pixelBytes
		|| height > std::numeric_limits<int>::max()
		|| static_cast<std::uint64_t>(height) > std::numeric_limits<std::size_t>::max() / (width * pixelBytes))
		return imageDecodeError("Image dimensions or storage size exceed supported limits");
	ImageSurface surface(SDL_CreateSurface(static_cast<int>(width), static_cast<int>(height),
		alpha ? SDL_PIXELFORMAT_RGBA128_FLOAT : SDL_PIXELFORMAT_RGB96_FLOAT), SDL_DestroySurface);
	if (!surface || !SDL_SetSurfaceColorspace(surface.get(), colorspace)
		|| !SDL_SetStringProperty(SDL_GetSurfaceProperties(surface.get()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "none"))
		return imageSDLError(ImageErrorCode::SDLFailure, "Creating floating-point image surface");
	return surface;
}

auto adoptImageSurface (ImageSurface surface) -> std::expected<Image, ImageError> {
	auto image = Image::adopt(surface.get());
	if (image)
		surface.release();
	return image;
}

auto imageDecodeError (std::string message) -> std::unexpected<ImageError> {
	return std::unexpected(ImageError{ImageErrorCode::DecodeFailure, std::move(message)});
}



//////
//
// Module namespace close
//

// namespace fcg::detail
}
