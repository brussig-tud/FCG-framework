
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

// SDL3 library
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>

// Local includes
#include "FCG/Image/sdl_image.h"
#include "image_internal.h"


//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Scoped stream owner; the bytes themselves remain borrowed during the synchronous operation.
using Stream = std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)>;

/// Probe one signature and reset between attempts. False probes are normal, even when they leave an SDL diagnostic.
auto detectedType (SDL_IOStream *stream) -> std::expected<const char*, fcg::ImageError>
{
	struct Probe {
		const char *type;
		bool(SDLCALL *is)(SDL_IOStream *);
	};

	// Match the still-image formats supported by IMG_LoadTyped_IO; ANI has only an animation loader.
	static constexpr Probe probes[] {
		{"AVIF", IMG_isAVIF}, {"CUR", IMG_isCUR}, {"ICO", IMG_isICO}, {"BMP", IMG_isBMP},
		{"GIF", IMG_isGIF}, {"JPG", IMG_isJPG}, {"JXL", IMG_isJXL}, {"LBM", IMG_isLBM},
		{"PCX", IMG_isPCX}, {"PNG", IMG_isPNG}, {"PNM", IMG_isPNM}, {"SVG", IMG_isSVG},
		{"TIF", IMG_isTIF}, {"XCF", IMG_isXCF}, {"XPM", IMG_isXPM}, {"XV", IMG_isXV},
		{"WEBP", IMG_isWEBP}, {"QOI", IMG_isQOI}
	};

	for (const auto &probe : probes) {
		if (SDL_SeekIO(stream, 0, SDL_IO_SEEK_SET) < 0)
			return fcg::detail::imageSDLError(fcg::ImageErrorCode::SDLFailure, "Seeking image probe stream");
		if (probe.is(stream))
			return probe.type;
	}
	return nullptr;
}

/// Recognized extensions permit a decode attempt, even when an optional codec cannot probe in this build.
auto recognizedHint (std::string_view hint) -> bool {
	static constexpr std::string_view extensions[] {
		"avif", "cur", "ico", "bmp", "gif", "jpg", "jpeg", "jxl", "lbm", "iff", "pcx", "png",
		"pnm", "pbm", "pgm", "ppm", "svg", "tga", "tif", "tiff", "xcf", "xpm", "xv", "webp", "qoi"
	};
	return std::ranges::find(extensions, hint) != std::end(extensions);
}

/// Static registration is retained in archive links by linkSDLImageHandler below.
const fcg::FormatHandlerRegistration sdlImageRegistration {
	"sdl_image", std::make_unique<fcg::SDLImageFormatHandler>(), -100
};

// Anonymous namespace end
}



//////
//
// Module namespace open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// SDLImageFormatHandler

auto SDLImageFormatHandler::accepts (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<bool, ImageError>
{
	auto normalized = detail::imageHint(bytes, hint);
	if (!normalized)
		return std::unexpected(std::move(normalized.error()));
	Stream stream(SDL_IOFromConstMem(bytes.data(), bytes.size()), SDL_CloseIO);
	if (!stream)
		return detail::imageSDLError(ImageErrorCode::SDLFailure, "Creating image probe stream");
	auto type = detectedType(stream.get());
	if (!type)
		return std::unexpected(std::move(type.error()));
	return *type != nullptr || recognizedHint(*normalized);
}

auto SDLImageFormatHandler::load (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<Image, ImageError>
{
	auto normalized = detail::imageHint(bytes, hint);
	if (!normalized)
		return std::unexpected(std::move(normalized.error()));
	Stream stream(SDL_IOFromConstMem(bytes.data(), bytes.size()), SDL_CloseIO);
	if (!stream)
		return detail::imageSDLError(ImageErrorCode::SDLFailure, "Creating image decode stream");
	// Prefer signatures over hints: upstream would otherwise try a misleading TGA hint before all magic formats.
	auto detected = detectedType(stream.get());
	if (!detected)
		return std::unexpected(std::move(detected.error()));
	if (SDL_SeekIO(stream.get(), 0, SDL_IO_SEEK_SET) < 0)
		return detail::imageSDLError(ImageErrorCode::SDLFailure, "Rewinding image decode stream");
	const char *type = *detected ? *detected : (normalized->empty() ? nullptr : normalized->c_str());
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface {
		IMG_LoadTyped_IO(stream.get(), false, type), SDL_DestroySurface
	};
	if (!surface)
		return detail::imageSDLError(ImageErrorCode::DecodeFailure, "Decoding image with SDL_image");
	auto image = Image::adopt(surface.get());
	if (image)
		surface.release();
	return image;
}



//////
//
// Functions
//

// Private interface namespace
namespace detail {

void linkSDLImageHandler () {
	// Referencing a TU-local object also ensures deferred dynamic initialization precedes this function.
	(void) &sdlImageRegistration;
}

// namespace detail
}



//////
//
// Namespaces close
//

// namespace fcg
}
