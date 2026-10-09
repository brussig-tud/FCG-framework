
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cstdlib>
#include <filesystem>

// FCG Framework
#include <FCG/Image/image_loader.h>



//////
//
// Functions
//

/// Decode a real fixture after a parent opts in to an external codec.
auto main (int argc, char **argv) -> int
{
	if (argc != 2 && argc != 4)
		return 1;
	auto image = fcg::ImageLoader::global().load(std::filesystem::path(argv[1]));
	const auto width = argc == 4 ? std::atoi(argv[2]) : 3;
	const auto height = argc == 4 ? std::atoi(argv[3]) : 2;
	if (argc == 4 && !std::ranges::any_of(fcg::ImageLoader::global().fileFormats(),
		[](const auto &format) { return format.name == "AVIF"; }))
		return 1;
	return image && image->width() == width && image->height() == height ? 0 : 1;
}
