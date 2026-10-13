
//////
//
// Includes
//

// C++ STL
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

// FCG Framework
#include <FCG/Image/image.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Measure repeated passes; callers allocate storage and acquire access before timing.
template<class Kernel>
[[nodiscard]] auto measure (Kernel &&kernel) -> double {
	const auto start = std::chrono::steady_clock::now();
	for (int pass = 0; pass < 50; ++pass)
		kernel();
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

/// Compare native row traversal with equivalent handwritten byte loops on padded or contiguous storage.
void benchmark (int padding)
{
	constexpr int width = 1920;
	constexpr int height = 1080;
	const int pitch = width * 4 + padding;
	std::vector<std::byte> first(std::size_t(pitch) * height, std::byte{42});
	auto second = first;
	auto image = fcg::Image::adopt(SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, first.data(), pitch));
	if (!image)
		throw std::runtime_error(image.error().message);
	auto guard = image->mapPixels<SDL_PIXELFORMAT_RGBA32>();
	if (!guard)
		throw std::runtime_error(guard.error().message);
	const auto view = *guard->view();
	const auto rowTime = measure([view] {
		for (auto row : view)
			for (int x = 0; x < row.size(); ++x) {
				auto value = row.readUnchecked(x);
				value.r = 255 - value.r;
				value.g = 255 - value.g;
				value.b = 255 - value.b;
				row.writeUnchecked(x, value);
			}
	});
	const auto handwrittenTime = measure([&second, pitch] {
		for (int y = 0; y < height; ++y) {
			auto *row = second.data() + std::size_t(y) * pitch;
			for (int x = 0; x < width; ++x) {
				auto *pixel = row + x * 4;
				pixel[0] = std::byte(255 - std::to_integer<unsigned>(pixel[0]));
				pixel[1] = std::byte(255 - std::to_integer<unsigned>(pixel[1]));
				pixel[2] = std::byte(255 - std::to_integer<unsigned>(pixel[2]));
			}
		}
	});
	if (first != second)
		throw std::runtime_error("Benchmark outputs or padding differ");
	std::cout << "padding=" << padding << " bytes: rows=" << rowTime
		<< " ms, handwritten=" << handwrittenTime << " ms, ratio=" << rowTime / handwrittenTime << '\n';
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Optional release benchmark; reports measurements without correctness timing thresholds.
auto main () -> int
{
	try {
		benchmark(0);
		benchmark(37);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
