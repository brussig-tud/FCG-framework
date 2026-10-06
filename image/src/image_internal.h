#ifndef __FCG_IMAGE_INTERNAL_H__
#define __FCG_IMAGE_INTERNAL_H__


//////
//
// Includes
//

// Local includes
#include "FCG/image_loader.h"


//////
//
// Namespaces open
//

// Private implementation helpers shared by the reader, registry, and backend.
namespace fcg::detail {

/// Owned file bytes and an advisory extension hint.
struct EncodedImage
{
	/// Encoded file contents.
	std::vector<std::byte> bytes;

	/// Native path's extension encoded as UTF-8.
	std::string hint;
};

/// Read one file completely without exposing SDL IO abstractions in the handler interface.
auto readImageFile (const std::filesystem::path &path) -> std::expected<EncodedImage, ImageError>;

/// Validate nonempty input and normalize ASCII hint case and an optional leading dot.
auto imageHint (std::span<const std::byte> bytes, std::string_view hint) -> std::expected<std::string, ImageError>;

/// Capture SDL's thread-local message immediately, before resource cleanup.
auto imageSDLError (ImageErrorCode code, const char *operation) -> std::unexpected<ImageError>;

/// Keep the shipped registrar's translation unit reachable from the singleton in ordinary static links.
void linkSDLImageHandler ();

// namespace fcg::detail
}


#endif // ifndef __FCG_IMAGE_INTERNAL_H__
