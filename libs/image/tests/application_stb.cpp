
//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <limits>
#include <span>

// stb_image library: intentionally external application-owned symbols.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>



//////
//
// Functions
//

/// Exercise a separate stb copy, changing state that must not affect FCG's private HDR decoder.
[[nodiscard]] auto applicationSTBDecode (std::span<const std::byte> bytes) -> bool
{
	if (bytes.size() > std::numeric_limits<int>::max())
		return false;
	stbi_set_flip_vertically_on_load(1);
	int width, height, channels;
	auto *pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()),
		&width, &height, &channels, 0);
	const bool success = pixels && width > 0 && height > 0;
	stbi_image_free(pixels);
	return success;
}
