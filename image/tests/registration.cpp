//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <memory>

// FCG Framework
#include <FCG/image_loader.h>


//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// A test-only format whose four encoded bytes contain its entire payload.
class TestHandler final : public fcg::ImageFormatHandler
{
public:
	auto accepts (std::span<const std::byte> bytes, std::string_view) const
		-> std::expected<bool, fcg::ImageError> override
	{
		constexpr std::array magic{std::byte{'T'}, std::byte{'E'}, std::byte{'S'}, std::byte{'T'}};
		return std::ranges::equal(bytes, magic);
	}
	auto load (std::span<const std::byte>, std::string_view) const
		-> std::expected<fcg::Image, fcg::ImageError> override
	{
		return fcg::Image::adopt(SDL_CreateSurface(7, 3, SDL_PIXELFORMAT_RGBA32));
	}
};

const fcg::ImageFormatRegistration registration{"test_only", std::make_unique<TestHandler>()};

// Anonymous namespace end
}
