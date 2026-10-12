
#ifndef __FCG_IMAGE_INTERNAL_H__
#define __FCG_IMAGE_INTERNAL_H__


//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Local includes
#include "FCG/Image/image_loader.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Private implementation helpers shared by the file reader, the handler registry and the SDL_image backend.
namespace detail {



//////
//
// Structs and enums
//

/// The complete contents of an image file together with its extension.
struct EncodedImage
{
	/// Encoded file contents.
	std::vector<std::byte> bytes;

	/// \brief
	/// 	Extension of the file path encoded as UTF-8, including the leading dot (e.g. <tt>".PNG"</tt>), or empty if
	/// 	the path has no extension. Not yet normalized; pass it through \c imageHint before handing it to handlers.
	std::string hint;
};



//////
//
// Functions
//

/// \brief
/// 	Reads a file completely into memory.
///
/// \param path
/// 	Path of the file to read.
///
/// \return
/// 	The file contents and extension, \c ImageErrorCode::InvalidArgument if \c path is empty or contains a NUL
/// 	character or the file is empty, or \c ImageErrorCode::IOFailure if the file cannot be opened, sized or read.
auto readImageFile (const std::filesystem::path &path) -> std::expected<EncodedImage, ImageError>;

/// \brief
/// 	Validates encoded input and normalizes its extension hint.
///
/// \param bytes
/// 	Encoded image data; must not be empty.
/// \param hint
/// 	Extension hint, optionally with a leading dot, in any ASCII letter case.
///
/// \return
/// 	The hint without its leading dot and with ASCII letters lowercased, or \c ImageErrorCode::InvalidArgument if
/// 	\c bytes is empty or \c hint contains a NUL character.
auto imageHint (std::span<const std::byte> bytes, std::string_view hint) -> std::expected<std::string, ImageError>;

/// \brief
/// 	Creates an error whose message is \c operation followed by the current \c SDL_GetError text.
///
/// Call immediately after the failing SDL call, before any cleanup calls can overwrite SDL's thread-local message.
///
/// \param code
/// 	Error category to report.
/// \param operation
/// 	Short description of the failed operation, used as the message prefix.
///
/// \return The error, ready to be returned from a function producing \c std::expected.
auto imageSDLError (ImageErrorCode code, const char *operation) -> std::unexpected<ImageError>;

/// \brief
/// 	Link anchor for the translation unit that registers the shipped SDL_image handler.
///
/// \c ImageLoader::global calls this function, which keeps that translation unit, and with it the registration,
/// from being dropped when the library is linked statically.
void linkSDLImageHandler ();



//////
//
// Namespaces close
//

// namespace detail
}

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_INTERNAL_H__
