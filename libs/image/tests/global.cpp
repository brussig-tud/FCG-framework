//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <span>

// FCG Framework
#include <FCG/Image/image_loader.h>


//////
//
// Functions
//

/// Only the generic loader is referenced: its archive link must retain the builtin static registrar.
auto main () -> int
{
	const unsigned char pnm[]{'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 255, 0, 0};
	auto &loader = fcg::ImageLoader::global();
	auto image = loader.load(std::as_bytes(std::span(pnm)));
	if (!image || image->width() != 1 || image->height() != 1 || loader.handlerCount() != 1) {
		std::cerr << (image ? "Automatic registration failed" : image.error().message) << '\n';
		return 1;
	}
	return 0;
}
