
//////
//
// Includes
//

// C++ STL
#include <span>

// FCG Framework
#include <FCG/Image/image_loader.h>
#include <FCG/Image/features.h>


//////
//
// Functions
//

/// Verify the custom format without referencing any symbol from the registrar translation unit.
auto main () -> int
{
	const char bytes[]{'T', 'E', 'S', 'T'};
	auto &loader = fcg::ImageLoader::global();
	auto image = loader.load(std::as_bytes(std::span(bytes)));
	return image && image->width() == 7 && image->height() == 3 && loader.handlerCount() == 4 + FCG_IMAGE_HAS_OPENEXR ? 0 : 1;
}
