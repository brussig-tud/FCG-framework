
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
#include "sdl_image_capabilities.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Owner that closes an SDL stream; the bytes the stream reads from remain borrowed for the duration of the call.
using Stream = std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)>;

/// \brief
/// 	Runs SDL_image's signature probes on the stream in SDL_image's own order and returns the first matching type.
///
/// The stream is rewound before each probe. A failing probe is not an error, even if it leaves a message in
/// \c SDL_GetError.
///
/// \param stream
/// 	Seekable stream over the encoded bytes.
///
/// \return
/// 	The SDL_image type name of the first matching format, \c nullptr if no signature matched, or
/// 	\c fcg::ImageErrorCode::SDLFailure if rewinding the stream failed.
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

/// \brief
/// 	Checks whether a normalized hint is an extension of a format SDL_image knows.
///
/// A recognized extension permits a decode attempt even when the format cannot be probed in this build.
///
/// \param hint
/// 	Normalized extension hint.
///
/// \return Whether \c hint is one of the known extensions.
auto recognizedHint (std::string_view hint) -> bool {
	static constexpr std::string_view extensions[] {
		"avif", "cur", "ico", "bmp", "gif", "jpg", "jpeg", "jxl", "lbm", "iff", "pcx", "png",
		"pnm", "pbm", "pgm", "ppm", "svg", "tga", "tif", "tiff", "xcf", "xpm", "xv", "webp", "qoi"
	};
	return std::ranges::find(extensions, hint) != std::end(extensions);
}

/// Registers the handler with \c fcg::ImageLoader::global; \c linkSDLImageHandler keeps it in static links.
const fcg::FormatHandlerRegistration sdlImageRegistration {
	"sdl_image", std::make_unique<fcg::SDLImageFormatHandler>(), -100
};

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

// Private implementation helpers shared within the Image library.
namespace detail {

void linkSDLImageHandler () {
	// Referencing a TU-local object also ensures deferred dynamic initialization precedes this function.
	(void) &sdlImageRegistration;
}

// namespace detail
}



//////
//
// Class implementations
//

////
// SDLImageFormatHandler

auto SDLImageFormatHandler::fileFormats () const -> std::vector<ImageFileFormat>
{
	std::vector<ImageFileFormat> formats;
#if FCG_SDL_IMAGE_AVIF
	formats.push_back({"AVIF", {"avif"}});
#endif
#if FCG_SDL_IMAGE_BMP
	formats.push_back({"Bitmap", {"bmp"}});
	formats.push_back({"Icon", {"ico"}});
	formats.push_back({"Cursor", {"cur"}});
#endif
#if FCG_SDL_IMAGE_GIF
	formats.push_back({"GIF", {"gif"}});
#endif
#if FCG_SDL_IMAGE_JPG
	formats.push_back({"JPEG", {"jpg", "jpeg"}});
#endif
#if FCG_SDL_IMAGE_JXL
	formats.push_back({"JPEG XL", {"jxl"}});
#endif
#if FCG_SDL_IMAGE_LBM
	formats.push_back({"Interleaved bitmap", {"lbm", "iff"}});
#endif
#if FCG_SDL_IMAGE_PCX
	formats.push_back({"PCX", {"pcx"}});
#endif
#if FCG_SDL_IMAGE_PNG
	formats.push_back({"PNG", {"png"}});
#endif
#if FCG_SDL_IMAGE_PNM
	formats.push_back({"Portable anymap", {"pnm", "pbm", "pgm", "ppm"}});
#endif
#if FCG_SDL_IMAGE_QOI
	formats.push_back({"Quite OK Image", {"qoi"}});
#endif
#if FCG_SDL_IMAGE_SVG
	formats.push_back({"Scalable vector graphics", {"svg"}});
#endif
#if FCG_SDL_IMAGE_TGA
	formats.push_back({"Targa", {"tga"}});
#endif
#if FCG_SDL_IMAGE_TIF
	formats.push_back({"TIFF", {"tif", "tiff"}});
#endif
#if FCG_SDL_IMAGE_WEBP
	formats.push_back({"WebP", {"webp"}});
#endif
#if FCG_SDL_IMAGE_XCF
	formats.push_back({"GIMP", {"xcf"}});
#endif
#if FCG_SDL_IMAGE_XPM
	formats.push_back({"X PixMap", {"xpm"}});
#endif
#if FCG_SDL_IMAGE_XV
	formats.push_back({"XV thumbnail", {"xv"}});
#endif
	return formats;
}

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

auto SDLImageFormatHandler::load (
	std::span<const std::byte> bytes, std::string_view hint, ImageLoaderOptions options
) const -> std::expected<Image, ImageError>
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
	return finishLoading(std::move(image), options);
}



//////
//
// Module namespace close
//

// namespace fcg
}
