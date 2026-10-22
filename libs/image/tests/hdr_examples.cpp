
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// SDL3 library
#include <SDL3/SDL_error.h>

// FCG Framework
#include <FCG/Image/radiance_hdr.h>
#include <FCG/Image/pfm.h>
#include <FCG/Image/openexr.h>



//////
//
// Module namespace open
//

// Compiled examples for the HDR format-handler guides.
namespace hdr_examples {



//////
//
// Functions
//

/// Load Radiance HDR from disk with its dedicated handler.
//! [radiance_direct]
[[nodiscard]] auto loadRadiance (const std::filesystem::path &path) -> std::expected<fcg::Image, fcg::ImageError> {
	fcg::RadianceHDRFormatHandler decoder;
	return decoder.load(path);
}
//! [radiance_direct]

/// Load Portable Float Map from disk with its dedicated handler.
//! [pfm_direct]
[[nodiscard]] auto loadPFM (const std::filesystem::path &path) -> std::expected<fcg::Image, fcg::ImageError> {
	fcg::PFMImageFormatHandler decoder;
	return decoder.load(path);
}
//! [pfm_direct]

/// Compile direct EXR use only when the linked Image library provides it.
//! [exr_direct]
#if FCG_IMAGE_HAS_OPENEXR
[[nodiscard]] auto loadEXR (const std::filesystem::path &path) -> std::expected<fcg::Image, fcg::ImageError> {
	fcg::OpenEXRImageFormatHandler decoder;
	return decoder.load(path);
}
#endif
//! [exr_direct]

/// Assign externally known source encoding; this labels samples without transforming them.
//! [hdr_encoding]
[[nodiscard]] auto identifyEncoding (fcg::Image &image, SDL_Colorspace knownSourceEncoding)
	-> std::expected<void, fcg::ImageError>
{
	// The caller knows the actual source encoding, for example from the asset's producer.
	if (!image.handle() || !SDL_SetSurfaceColorspace(image.handle(), knownSourceEncoding))
		return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, SDL_GetError()});
	return {};
}
//! [hdr_encoding]

/// Generic loading retains floating HDR values when inspecting a color.
//! [hdr_pixel]
[[nodiscard]] auto hdrPixel (std::span<const std::byte> bytes) -> std::expected<SDL_FColor, fcg::ImageError> {
	auto image = fcg::ImageLoader::global().load(bytes);
	if (!image)
		return std::unexpected(image.error());
	return image->readColorFloat(0, 0); // Color components may exceed one.
}
//! [hdr_pixel]



//////
//
// Module namespace close
//

// namespace hdr_examples
}
