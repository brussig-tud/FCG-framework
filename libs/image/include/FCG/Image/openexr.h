
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
/// the encoding; unidentified primaries remain unspecified. Enable <tt>FCG_IMAGE_OPENEXR</tt> before
/// configuring the framework.
///
/// \section fcg_openexr_usage Getting started
/// \snippet hdr_examples.cpp exr_direct
///
/// \section fcg_openexr_workflows Common tasks
/// Register a handler instance on an independent <tt>\ref fcg::ImageLoader "ImageLoader"</tt> to choose its
/// formats.
/// Inspect floating samples with <tt>\ref fcg::Image::readColorFloat "Image::readColorFloat"</tt>.
/// The class is available only when <tt>FCG_IMAGE_HAS_OPENEXR</tt> is one.
/// A source whose encoding is unspecified needs a known interpretation before color conversion or GPU upload.
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
/// the encoding; unidentified primaries remain unspecified. Enable <tt>FCG_IMAGE_OPENEXR</tt> before
/// configuring the framework.
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

	/// Exposes the inherited filesystem loading overload.
	using ImageFormatHandler::load;

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

	/// \brief Decodes a complete OpenEXR image into independently owned floating-point storage.
	///
	/// \param bytes Encoded input, borrowed for this call.
	/// \param hint Optional extension; the signature determines whether this decoder can load the input.
	///
	/// \return
	/// 	The image, \c ImageErrorCode::InvalidArgument for invalid arguments,
	/// 	\c ImageErrorCode::UnsupportedFormat for unrelated input, \c ImageErrorCode::DecodeFailure for
	/// 	malformed or unsupported variants, or \c ImageErrorCode::SDLFailure for surface setup failures.
	///
	/// 	hrows std::bad_alloc If a C++ allocation fails.
	[[nodiscard]] auto load (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<Image, ImageError> override;
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

