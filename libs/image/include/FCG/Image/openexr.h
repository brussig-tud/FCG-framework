
#ifndef __FCG_OPENEXR_H__
#define __FCG_OPENEXR_H__


//////
//
// Module documentation
//

/// \defgroup fcg_openexr OpenEXR images
/// \ingroup fcg_modules
/// \brief Decodes single-part flat OpenEXR color images into RGB or RGBA 32-bit floating-point pixels.
///
/// Use <tt>\ref fcg::OpenEXRImageFormatHandler "OpenEXRImageFormatHandler"</tt> directly or load through
/// <tt>\ref fcg::ImageLoader::global "ImageLoader::global"</tt>.
/// The returned extent is the data window, rebased to pixel <tt>(0, 0)</tt>. Recognized color metadata supplies
/// the encoding; unidentified components remain unspecified unless loader inference requests are enabled.
///
/// \section fcg_openexr_usage Getting started
/// Enable OpenEXR before adding the framework to your CMake project:
/// \code{.cmake}
/// set(FCG_IMAGE_OPENEXR ON CACHE BOOL "Enable OpenEXR decoding")
/// \endcode
/// Include <tt>\#include &lt;FCG/Image/features.h&gt;</tt> and test the numeric macro
/// <tt>FCG_IMAGE_HAS_OPENEXR</tt> when your code must also compile without OpenEXR support:
/// \snippet hdr_examples.cpp exr_direct
///
/// \section fcg_openexr_workflows Common tasks
/// \subsection fcg_openexr_workflows_encoding Interpreting color metadata
/// An ACES image-container declaration takes precedence over the color identifier. An explicit <tt>colorInteropID</tt>
/// other than empty or <tt>unknown</tt> takes precedence over <tt>chromaticities</tt>. Recognized identifiers establish
/// each representable component; for example, linear Rec.2020 and linear P3-D65 can be uploaded with color conversion.
/// With an absent, empty or <tt>unknown</tt> identifier, explicit chromaticities establish primaries. Missing
/// primaries and transfer are assumed as BT.709 and linear by default; full range is format-defined. Inspect
/// <tt>\ref fcg::ImagePixelInfo::provenance "ImagePixelInfo::provenance"</tt> to distinguish declared values from
/// assumptions. Use <tt>\ref fcg::ImageLoaderOptions::conservative "ImageLoaderOptions::conservative"</tt> to
/// retain unidentified components without optional assumptions or pixel scans. Label completion retains samples.
/// Linear float output also receives an assumed missing SDR white level of 1 and peak-derived missing headroom.
/// The source attributes <tt>whiteLuminance</tt> and <tt>sampToNits</tt> describe radiometric calibration; they are
/// retained as source information rather than used as diffuse-white levels.
///
/// ACES and Adobe RGB gamuts, the <tt>data</tt> identifier and unrecognized identifiers retain custom components.
/// Recognized gamma and HLG transfers can also be unsupported for color conversion. Missing-metadata assumptions retain
/// these declarations. Use <tt>\ref fcg::Image::uploadStoredSamples "Image::uploadStoredSamples"</tt> to upload their
/// numeric samples; <tt>\ref fcg::Image::uploadStoredSamplesIfUnknown "uploadStoredSamplesIfUnknown"</tt> selects
/// preservation only when an encoding component is missing.
///
/// \subsection fcg_openexr_workflows_loading Choosing a loading and upload policy
/// Register a handler instance on an independent <tt>\ref fcg::ImageLoader "ImageLoader"</tt> to choose its
/// formats.
/// Inspect floating samples with <tt>\ref fcg::Image::readColorFloat "Image::readColorFloat"</tt>.
/// The class is available only when <tt>FCG_IMAGE_HAS_OPENEXR</tt> is one.
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
/// \section fcg_openexr_errors Handling errors
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
#include "FCG/Image/features.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_openexr
/// @{

#if FCG_IMAGE_HAS_OPENEXR



//////
//
// Classes
//

/// \brief Decodes single-part flat OpenEXR color images into RGB or RGBA 32-bit floating-point pixels.
///
/// The returned extent is the data window, rebased to pixel <tt>(0, 0)</tt>. Recognized color metadata supplies
/// the encoding; unidentified components remain unspecified unless loader inference requests are enabled. The inherited
/// \c load supplies missing metadata. Identified custom components, including ACES gamuts and the
/// <tt>data</tt> identifier, are retained. Enable <tt>FCG_IMAGE_OPENEXR</tt> before configuring the framework.
///
/// \note
///         Full-resolution unlayered RGB or Y, with optional A, can use HALF or FLOAT channels. Tiled images
/// use their highest-resolution level. Deep, multipart, layered-only, selected UINT and selected subsampled
/// channels are rejected. Stored associated alpha is retained.
class FCG_IMAGE_EXPORT OpenEXRImageFormatHandler final : public ImageFormatHandler
{
public:

	////
	// Object construction

	/// Creates a stateless CPU decoder.
	OpenEXRImageFormatHandler() = default;


	////
	// Interface: ImageFormatHandler

	/// Lists the supported OpenEXR extensions.
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

	/// \brief Decodes a complete OpenEXR image into independently owned floating-point storage.
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

#endif // FCG_IMAGE_HAS_OPENEXR



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_OPENEXR_H__

