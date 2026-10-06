//////
//
// Includes
//

// FCG Framework
#include <FCG/image_loader.h>


//////
//
// Functions
//

/// Decode a real fixture after a parent opts in to an external codec.
auto main (int argc, char **argv) -> int
{
	if (argc != 2)
		return 1;
	auto image = fcg::ImageLoader::global().load(std::filesystem::path(argv[1]));
	return image && image->width() == 3 && image->height() == 2 ? 0 : 1;
}
