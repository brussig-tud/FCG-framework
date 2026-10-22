
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

/// \brief Load an image while assuming every missing RGB encoding component.
///
/// \param path The encoded image file.
///
/// \return The owned image with best-effort metadata, or a decoding error.
//! [guessing]
[[nodiscard]] auto loadForTexture (const std::filesystem::path &path) -> std::expected<fcg::Image, fcg::ImageError> {
	return fcg::ImageLoader::global().loadAndGuess(path);
}

/// \brief Guess only primaries when the file already establishes its transfer function.
///
/// \param path The encoded image file.
///
/// \return The owned image with identified or assumed primaries, or a decoding error.
[[nodiscard]] auto loadWithGuessedPrimaries (const std::filesystem::path &path)
	-> std::expected<fcg::Image, fcg::ImageError> {
	return fcg::ImageLoader::global().load(path, {.guessColorPrimaries=true});
}

/// \brief Use the inherited guessing alias on a concrete handler.
///
/// \param bytes Encoded PFM data borrowed during the call.
///
/// \return The owned decoded image, or a decoding error.
[[nodiscard]] auto loadPFMForTexture (std::span<const std::byte> bytes) -> std::expected<fcg::Image, fcg::ImageError> {
	fcg::PFMImageFormatHandler decoder;
	return decoder.loadAndGuess(bytes);
}
//! [guessing]

/// \brief Supply missing metadata only during upload, leaving the CPU image untouched.
///
/// \param image The decoded image.
/// \param device The device, which must outlive the returned texture.
///
/// \return The owning texture, or an upload error.
//! [upload_policies]
[[nodiscard]] auto uploadWithAssumedEncoding (const fcg::Image &image, fcg::Device &device)
	-> std::expected<fcg::Texture, fcg::ImageError> {
	return image.uploadWithFallback(device, SDL_COLORSPACE_SRGB_LINEAR);
}

/// \brief Upload numeric samples without gamma or gamut conversion, including integer data textures.
///
/// \param image The decoded image.
/// \param device The device, which must outlive the returned texture.
///
/// \return The owning texture containing normalized integer or floating samples, or an upload error.
[[nodiscard]] auto uploadData (const fcg::Image &image, fcg::Device &device)
	-> std::expected<fcg::Texture, fcg::ImageError> {
	return image.uploadStoredSamples(device);
}

/// \brief Preserve unidentified samples first, permitting color conversion with an explicit fallback when unavailable.
///
/// \param image The decoded image.
/// \param device The device, which must outlive the returned texture.
///
/// \return The owning texture, or an upload error.
[[nodiscard]] auto uploadWithPolicies (const fcg::Image &image, fcg::Device &device)
	-> std::expected<fcg::Texture, fcg::ImageError>
{
	return image.upload(device, {
		.samplePolicy=fcg::ImageUploadSamplePolicy::PreserveSamplesIfUnknown,
		.unidentifiedSourceColorspace=SDL_COLORSPACE_SRGB_LINEAR
	});
}
//! [upload_policies]

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
