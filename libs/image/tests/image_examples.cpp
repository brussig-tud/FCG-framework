
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <optional>
#include <utility>

// SDL3 library
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_error.h>

// Local includes
//! [includes]
#include <FCG/Image/image_loader.h>
#include <FCG/Image/sdl_image.h>
//! [includes]



//////
//
// Module namespace open
//

namespace image_examples {

//! [file]
auto fromDisk (const std::filesystem::path &path) -> std::expected<fcg::Image, fcg::ImageError> {
	return fcg::ImageLoader::global().load(path);
}
//! [file]

//! [memory]
auto fromMemory (std::span<const std::byte> encoded) -> std::expected<fcg::Image, fcg::ImageError> {
	// The hint is useful for TGA; PNG/JPEG and other signature formats do not require it.
	return fcg::ImageLoader::global().load(encoded, "tga");
}
//! [memory]

//! [surface]
auto convertToRGBA (fcg::Image &image) -> std::expected<fcg::Image, fcg::ImageError> {
	// SDL_ConvertSurface handles native pitch, palette, and transparency. This is an explicit client operation.
	auto *surface = SDL_ConvertSurface(image.handle(), SDL_PIXELFORMAT_RGBA32);
	if (!surface)
		return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, SDL_GetError()});
	auto converted = fcg::Image::adopt(surface);
	if (!converted)
		SDL_DestroySurface(surface);
	return converted;
}
//! [surface]

//! [optional]
auto replaceImage (std::optional<fcg::Image> &current, const std::filesystem::path &path)
	-> std::expected<void, fcg::ImageError>
{
	auto next = fcg::ImageLoader::global().load(path);
	if (!next)
		return std::unexpected(std::move(next.error()));
	current = std::move(*next);
	return {};
}
//! [optional]

//! [independent]
auto localLoad (std::span<const std::byte> bytes) -> std::expected<fcg::Image, fcg::ImageError> {
	fcg::ImageLoader loader; // Empty until explicitly populated.
	auto added = loader.registerHandler("sdl_image", std::make_unique<fcg::SDLImageFormatHandler>());
	if (!added)
		return std::unexpected(std::move(added.error()));
	return loader.load(bytes); // The returned image does not borrow the local decoder.
}
//! [independent]

//! [direct]
auto directLoad (const std::filesystem::path &path) -> std::expected<fcg::Image, fcg::ImageError> {
	fcg::SDLImageFormatHandler decoder;
	return decoder.load(path);
}
//! [direct]

//! [errors]
auto describeFailure (std::span<const std::byte> bytes) -> std::string
{
	auto result = fcg::ImageLoader::global().load(bytes);
	if (result)
		return {};
	if (result.error().code == fcg::ImageErrorCode::UnsupportedFormat)
		return "Install an accepting image handler";
	return result.error().message; // Owned text; the client decides how to report it.
}
//! [errors]

//! [registration]
// A client may define its own ImageFormatHandler. This example delegates to the built-in <em>SDL_image</em>-based
// handler; an actual custom format would implement accepts()/load() with its own detection and decoding logic.
class ExampleHandler final : public fcg::ImageFormatHandler
{
public:

	////
	// Methods

	auto accepts (std::span<const std::byte> bytes, std::string_view hint) const
		-> std::expected<bool, fcg::ImageError> override
	{
		return builtinHandler.accepts(bytes, hint);
	}
	auto load (std::span<const std::byte> bytes, std::string_view hint) const
		-> std::expected<fcg::Image, fcg::ImageError> override
	{
		return builtinHandler.load(bytes, hint);
	}


private:

	////
	// Fields

	/// We just wrap the built-in <em>SDL_image</em>-based handler.
	fcg::SDLImageFormatHandler builtinHandler;
};

// Outside every function, in the defining translation unit. Priority zero precedes the builtin's -100.
const fcg::FormatHandlerRegistration exampleRegistration{"example_decoder", std::make_unique<ExampleHandler>()};
//! [registration]




//////
//
// Module namespace close
//

// namespace image_examples
}
