
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
/// The RGB transfer is linear. Primaries are unspecified unless an explicit recognized <tt>PRIMARIES</tt>
/// record identifies them.
///
/// \section fcg_radiance_hdr_usage Getting started
/// \snippet hdr_examples.cpp radiance_direct
///
/// \section fcg_radiance_hdr_workflows Common tasks
/// Register a handler instance on an independent <tt>\ref fcg::ImageLoader "ImageLoader"</tt> to choose its
/// formats.
/// Inspect floating samples with <tt>\ref fcg::Image::readColorFloat "Image::readColorFloat"</tt>.
/// This handler is included in every Image build.
/// A source whose encoding is unspecified needs a known interpretation before color conversion or GPU upload.
/// \snippet hdr_examples.cpp hdr_encoding
///
/// \section fcg_radiance_hdr_errors Handling errors
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

/// \addtogroup fcg_radiance_hdr
/// @{



//////
//
// Classes
//

/// \brief Decodes RGBE Radiance images into three-channel 32-bit floating-point pixels.
///
/// The RGB transfer is linear. Primaries are unspecified unless an explicit recognized <tt>PRIMARIES</tt>
/// record identifies them.
///
/// \note
///         The supported layouts are flat RGBE and scanline RLE in <tt>-Y height +X width</tt> order. XYZE,
/// other orientations and legacy repeat markers are rejected. Stored values are retained without applying
/// exposure or color correction.
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



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_RADIANCE_HDR_H__

