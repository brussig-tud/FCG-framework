
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <span>

// FCG Framework
#include <FCG/Image/image_loader.h>
#include <FCG/Image/features.h>


//////
//
// Functions
//

/// Only the generic loader is referenced: its archive link must retain the builtin static registrar.
auto main (int argc, char **argv) -> int
{
	const unsigned char pnm[]{'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 255, 0, 0};
	auto &loader = fcg::ImageLoader::global();
	auto image = loader.load(std::as_bytes(std::span(pnm)));
	if (!image || image->width() != 1 || image->height() != 1 || loader.handlerCount() != 3 + FCG_IMAGE_HAS_OPENEXR) {
		std::cerr << (image ? "Automatic registration failed" : image.error().message) << '\n';
		return 1;
	}
	const unsigned char hdr[]{'#', '?', 'R', 'G', 'B', 'E', '\n', 'F', 'O', 'R', 'M', 'A', 'T', '=',
		'3', '2', '-', 'b', 'i', 't', '_', 'r', 'l', 'e', '_', 'r', 'g', 'b', 'e', '\n', '\n',
		'-', 'Y', ' ', '1', ' ', '+', 'X', ' ', '1', '\n', 128, 64, 32, 130};
	if (!loader.load(std::as_bytes(std::span(hdr)), "tga"))
		return 1;
	const unsigned char pfm[]{'P', 'f', '\n', '1', ' ', '1', '\n', '-', '1', '\n', 0, 0, 128, 64};
	if (!loader.load(std::as_bytes(std::span(pfm)), ".HDR"))
		return 1;
#if FCG_IMAGE_HAS_OPENEXR
	if (argc != 2 || !loader.load(argv[1]))
		return 1;
#endif
	return 0;
}
