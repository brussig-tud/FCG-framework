
#ifndef __FCG_RADIANCE_HDR_H__
#define __FCG_RADIANCE_HDR_H__


//////
//
// Module documentation
//

/// \defgroup fcg_radiance_hdr Radiance HDR images
/// \ingroup fcg_modules
/// \brief Decodes RGBE Radiance images into three-channel 32-bit floating-point pixels.
///
/// Use <tt>\ref fcg::RadianceHDRFormatHandler "RadianceHDRFormatHandler"</tt> directly or load through
/// <tt>\ref fcg::ImageLoader::global "ImageLoader::global"</tt>.
/// Returned samples have linear transfer. A declared <tt>GAMMA</tt> power function is decoded while loading.
/// Explicit <tt>PRIMARIES</tt> coordinates establish the source gamut and white point. Gamuts supported by
/// <tt>\ref fcg::Image::upload "Image::upload"</tt> are retained; other numerically defined gamuts are converted to
/// linear BT.709/D65 with Bradford white-point adaptation. Returned metadata describes these output samples.
/// Without declared primaries, they remain unspecified unless guessing is requested. Primary guessing uses the
/// conventional Radiance gamut: red <tt>(.64, .33)</tt>, green <tt>(.29, .60)</tt>, blue <tt>(.15, .06)</tt> and
/// equal-energy white <tt>(1/3, 1/3)</tt>, then converts it to linear BT.709/D65. Range is full.
/// <tt>\ref fcg::ImagePixelInfo::loadAdjustments "ImagePixelInfo::loadAdjustments"</tt> marks inferred and adapted
/// primaries as <tt>\ref fcg::ImageLoadAdjustment::GuessedAndAdapted "GuessedAndAdapted"</tt>. Adapting declared
/// primaries or decoding non-unit gamma is marked <tt>\ref fcg::ImageLoadAdjustment::Adapted "Adapted"</tt> on the
/// corresponding property.
///
/// \section fcg_radiance_hdr_usage Getting started
/// \snippet hdr_examples.cpp radiance_direct
///
/// \section fcg_radiance_hdr_workflows Common tasks
/// Register a handler instance on an independent <tt>\ref fcg::ImageLoader "ImageLoader"</tt> to choose its
/// formats.
/// Inspect floating samples with <tt>\ref fcg::Image::readColorFloat "Image::readColorFloat"</tt>.
/// This handler is included in every Image build.
/// <tt>\ref fcg::ImageFormatHandler::loadAndGuess "ImageFormatHandler::loadAndGuess"</tt> is inherited by this
/// handler and supplies missing color metadata. The registry offers
/// <tt>\ref fcg::ImageLoader::loadAndGuess "ImageLoader::loadAndGuess"</tt> as well:
/// \snippet hdr_examples.cpp guessing
/// Color conversion preserves unclamped floating-point values, including HDR and negative out-of-gamut components.
/// Exposure and color-correction adjustments already present in the samples are retained. Upload preservation
/// policies preserve the returned image's numeric samples, which can differ from those stored in the file:
/// \snippet hdr_examples.cpp upload_policies
///
/// \section fcg_radiance_hdr_errors Handling errors
/// Input errors are reported through <tt>\ref fcg::ImageError "ImageError"</tt>.
/// Recognized malformed or unsupported variants, singular color definitions and conversion results outside finite
/// binary32 storage produce a decode failure. Allocation exceptions may propagate.
/// No SDL video initialization is required. Calls sharing one handler must be externally serialized.
/// Images own their pixels and remain valid after the encoded input and decoder are destroyed.



//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Local includes
#include "FCG/Image/image_loader.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_radiance_hdr
/// @{



//////
//
// Classes
//

/// \brief Decodes RGBE Radiance images into three-channel 32-bit floating-point pixels.
///
/// Returned samples are linear, full-range floating RGB. Declared numeric gamma is decoded. Source gamuts that
/// color-managed upload supports are retained; other numerically defined gamuts are converted to linear BT.709/D65
/// with Bradford white-point adaptation. Conversion retains unclamped HDR and negative components. Returned metadata
/// describes the output encoding; original color records remain available through the surface properties under
/// <tt>fcg.image.source.color_metadata</tt>.
///
/// Missing primaries remain unspecified unless \c ImageLoaderOptions::guessColorPrimaries is enabled, including
/// through the inherited \c loadAndGuess. This request selects the conventional Radiance gamut and equal-energy white,
/// then converts to linear BT.709/D65. Declared primaries take precedence. Missing transfer is interpreted as linear.
///
/// \note
///         The supported layouts are flat RGBE and scanline RLE in <tt>-Y height +X width</tt> order. XYZE,
/// other orientations and legacy repeat markers are rejected. Exposure and color-correction records are retained
/// without applying additional radiometric adjustments.
class FCG_IMAGE_EXPORT RadianceHDRFormatHandler final : public ImageFormatHandler
{
public:

	////
	// Object construction

	/// Creates a stateless CPU decoder.
	RadianceHDRFormatHandler() = default;


	////
	// Interface: ImageFormatHandler

	/// Exposes the inherited filesystem loading overload.
	using ImageFormatHandler::load;

	/// Lists the supported Radiance HDR extensions.
	///
	/// \return Owning format records for file-dialog filters.
	[[nodiscard]] auto fileFormats () const -> std::vector<ImageFileFormat> override;

	/// \brief Tests the format signature independently of the extension hint.
	///
	/// \param bytes Encoded input, borrowed for this call.
	/// \param hint Optional extension; case and a leading dot are ignored.
	///
	/// \return
	/// 	Whether the signature matches, or \c ImageErrorCode::InvalidArgument for empty bytes or a NUL in the hint.
	[[nodiscard]] auto accepts (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<bool, ImageError> override;

	/// \brief Decodes a complete Radiance HDR image into independently owned floating-point storage.
	///
	/// \param bytes Encoded input, borrowed for this call.
	/// \param hint Optional extension; the signature determines whether this decoder can load the input.
	/// \param options Requests to infer missing RGB metadata; \c loadAndGuess enables every request.
	///
	/// \return
	/// 	The image, \c ImageErrorCode::InvalidArgument for invalid arguments,
	/// 	\c ImageErrorCode::UnsupportedFormat for unrelated input, \c ImageErrorCode::DecodeFailure for
	/// 	malformed or unsupported variants, invalid color definitions or unrepresentable conversion results,
	/// 	or \c ImageErrorCode::SDLFailure for surface setup failures.
	///
	/// 	hrows std::bad_alloc If a C++ allocation fails.
	[[nodiscard]] auto load (
		std::span<const std::byte> bytes, std::string_view hint={}, ImageLoaderOptions options={}
	) const -> std::expected<Image, ImageError> override;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_RADIANCE_HDR_H__

