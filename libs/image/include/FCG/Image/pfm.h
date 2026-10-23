
#ifndef __FCG_PFM_H__
#define __FCG_PFM_H__


//////
//
// Module documentation
//

/// \defgroup fcg_pfm Portable Float Map images
/// \ingroup fcg_modules
/// \brief Decodes RGB and grayscale Portable Float Map files into three-channel 32-bit floating-point pixels.
///
/// Use <tt>\ref fcg::PFMImageFormatHandler "PFMImageFormatHandler"</tt> directly or load through
/// <tt>\ref fcg::ImageLoader::global "ImageLoader::global"</tt>.
/// Both endian orders and LF/CRLF text headers are supported, with comments and blank lines between records.
/// The raster must consume the remaining input. Ordinary loading assumes missing color primaries as BT.709 and
/// transfer as linear, then assumes missing SDR white as 1 and derives missing headroom from the RGB peak.
/// <tt>\ref fcg::ImageLoaderOptions::conservative "ImageLoaderOptions::conservative"</tt> retains unidentified
/// interpretation without optional assumptions or scans.
///
/// This handler guesses BT.709 primaries and linear transfer when the corresponding requests are enabled. Range is
/// full. These format-specific fallbacks label the decoded samples without changing them.
///
/// \section fcg_pfm_usage Getting started
/// \snippet hdr_examples.cpp pfm_direct
///
/// \section fcg_pfm_workflows Common tasks
/// Register a handler instance on an independent <tt>\ref fcg::ImageLoader "ImageLoader"</tt> to choose its
/// formats.
/// Inspect floating samples with <tt>\ref fcg::Image::readColorFloat "Image::readColorFloat"</tt>.
/// This handler is included in every Image build.
/// <tt>\ref fcg::ImageFormatHandler::load "ImageFormatHandler::load"</tt> is inherited by this
/// handler and supplies missing color metadata. The registry offers
/// <tt>\ref fcg::ImageLoader::load "ImageLoader::load"</tt> as well:
/// \snippet hdr_examples.cpp guessing
/// To retain source metadata, choose <tt>\ref fcg::Image::uploadWithFallback "uploadWithFallback"</tt> or upload
/// stored values with <tt>\ref fcg::Image::uploadStoredSamples "uploadStoredSamples"</tt>. Conditional preservation
/// is available through <tt>\ref fcg::Image::uploadStoredSamplesIfUnknown "uploadStoredSamplesIfUnknown"</tt>.
/// \snippet hdr_examples.cpp upload_policies
/// If the producer supplies the actual source encoding, label it before color conversion:
/// \snippet hdr_examples.cpp hdr_encoding
///
/// \section fcg_pfm_errors Handling errors
/// Input errors are reported through <tt>\ref fcg::ImageError "ImageError"</tt>.
/// Recognized malformed or unsupported variants produce a decode failure. Allocation exceptions may propagate.
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

/// \addtogroup fcg_pfm
/// @{



//////
//
// Classes
//

/// \brief Decodes RGB and grayscale Portable Float Map files into three-channel 32-bit floating-point pixels.
///
/// Both endian orders and LF/CRLF text headers are supported, with comments and blank lines between records.
/// The raster must consume the remaining input. Ordinary loading assumes BT.709 primaries, linear transfer and
/// missing SDR white as 1, then derives missing headroom from the stored RGB peak. Range is format-defined as full.
/// \c ImageLoaderOptions::conservative retains unidentified interpretation without optional assumptions or scans.
/// Metadata completion retains decoded samples.
///
/// \note
///         Rows use the conventional bottom-to-top file order, including when a producer uses another
/// convention without identifying it in the header. Grayscale is replicated into RGB. Samples are multiplied by
/// the absolute header scale.
class FCG_IMAGE_EXPORT PFMImageFormatHandler final : public ImageFormatHandler
{
public:

	////
	// Object construction

	/// Creates a stateless CPU decoder.
	PFMImageFormatHandler() = default;


	////
	// Interface: ImageFormatHandler

	/// Lists the supported PFM extensions.
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


protected:

	////
	// Interface: ImageFormatHandler

	/// \brief Decodes a complete PFM image into independently owned floating-point storage.
	///
	/// \param bytes Encoded input, borrowed for this call.
	/// \param hint Optional extension; the signature determines whether this decoder can load the input.
	/// \param options Controls optional assumptions and metadata completion; all requests are enabled by default.
	///
	/// \return
	/// 	The image, \c ImageErrorCode::InvalidArgument for invalid arguments,
	/// 	\c ImageErrorCode::UnsupportedFormat for unrelated input, \c ImageErrorCode::DecodeFailure for
	/// 	malformed or unsupported variants, or \c ImageErrorCode::SDLFailure for surface setup failures.
	///
	/// 	hrows std::bad_alloc If a C++ allocation fails.
	[[nodiscard]] auto decode (
		std::span<const std::byte> bytes, std::string_view hint, ImageLoaderOptions options
	) const -> std::expected<Image, ImageError> override;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_PFM_H__

