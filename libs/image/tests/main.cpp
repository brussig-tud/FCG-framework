//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/Image/sdl_image.h>


//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

static_assert(!std::is_default_constructible_v<fcg::Image>);
static_assert(!std::is_copy_constructible_v<fcg::Image> && !std::is_copy_assignable_v<fcg::Image>);
static_assert(std::is_nothrow_move_constructible_v<fcg::Image> && std::is_nothrow_move_assignable_v<fcg::Image>);

/// Assertions must also run in release builds.
void require (bool value, const char *message)
{
	if (!value)
		throw std::runtime_error(message);
}

/// Extract successful ownership or report its owned diagnostic.
template <class T> auto take (std::expected<T, fcg::ImageError> result) -> T
{
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Verify an expected recoverable error category.
template <class T> void fails (const std::expected<T, fcg::ImageError> &result, fcg::ImageErrorCode code)
{
	require(!result && result.error().code == code, "Incorrect image error category");
}

/// Read fixture bytes so the disk and memory decoder paths can be compared.
auto read (const std::filesystem::path &path) -> std::vector<std::byte>
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	require(bool(file), "Cannot open fixture");
	const auto size = file.tellg();
	require(size > 0, "Empty fixture");
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	file.seekg(0);
	require(bool(file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
			"Cannot read fixture");
	return bytes;
}

/// Inspect native surfaces through SDL, including palettes and row padding.
auto pixel (fcg::Image &image, int x = 0, int y = 0) -> SDL_Color
{
	SDL_Color color;
	require(SDL_ReadSurfacePixel(image.handle(), x, y, &color.r, &color.g, &color.b, &color.a), SDL_GetError());
	return color;
}

/// Ordered fallback test double; can decline, fail detection, fail decoding, or succeed.
class Handler final : public fcg::ImageFormatHandler
{
public:
	Handler (std::vector<std::string> &trace, std::string id, int behavior, int *destroyed = nullptr)
		: trace(trace), id(std::move(id)), behavior(behavior), destroyed(destroyed)
	{
	}
	~Handler () override
	{
		if (destroyed)
			++*destroyed;
	}
	auto accepts (std::span<const std::byte>, std::string_view hint) const
		-> std::expected<bool, fcg::ImageError> override
	{
		trace.push_back(id + ":probe:" + std::string(hint));
		if (behavior == 1)
			return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, "probe failed"});
		return behavior != 0;
	}
	auto load (std::span<const std::byte>, std::string_view) const
		-> std::expected<fcg::Image, fcg::ImageError> override
	{
		trace.push_back(id + ":decode");
		if (behavior == 2)
			return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::DecodeFailure, "decode failed"});
		return fcg::Image::adopt(SDL_CreateSurface(9, 5, SDL_PIXELFORMAT_RGBA32));
	}

private:
	std::vector<std::string> &trace;
	std::string id;
	int behavior;
	int *destroyed;
};

/// Registry selection, failure aggregation, normalization, and ownership.
void registry ()
{
	const std::array bytes{std::byte{1}};
	std::vector<std::string> trace;
	fcg::ImageLoader loader;
	fcg::ImageLoader other;
	require(loader.handlerCount() == 0, "Local loader must start empty");
	fails(loader.load(bytes), fcg::ImageErrorCode::UnsupportedFormat);
	fails(loader.registerHandler("", std::make_unique<Handler>(trace, "invalid", 0)),
		  fcg::ImageErrorCode::InvalidArgument);
	fails(loader.registerHandler("null", nullptr), fcg::ImageErrorCode::InvalidArgument);
	fails(loader.registerHandler(std::string("bad\0id", 6), std::make_unique<Handler>(trace, "invalid", 0)),
		  fcg::ImageErrorCode::InvalidArgument);
	int destroyed = 0;
	require(bool(loader.registerHandler("z", std::make_unique<Handler>(trace, "z", 3, &destroyed))), "Register z");
	require(bool(loader.registerHandler("b", std::make_unique<Handler>(trace, "b", 2))), "Register b");
	require(bool(loader.registerHandler("a", std::make_unique<Handler>(trace, "a", 1))), "Register a");
	require(bool(loader.registerHandler("decline", std::make_unique<Handler>(trace, "decline", 0), 10)),
			"Register decline");
	fails(loader.registerHandler("z", std::make_unique<Handler>(trace, "duplicate", 0, &destroyed)),
		  fcg::ImageErrorCode::DuplicateHandler);
	require(destroyed == 1 && loader.handlerCount() == 4, "Rejected registration ownership");
	auto result = take(loader.load(bytes, ".PnG"));
	require(result.width() == 9
				&& trace
					   == std::vector<std::string>{"decline:probe:png", "a:probe:png", "b:probe:png", "b:decode",
												   "z:probe:png", "z:decode"},
			"Priority, equal-priority ID order, or fallback failed");
	fails(other.load(bytes), fcg::ImageErrorCode::UnsupportedFormat);
	require(loader.removeHandler("z") && destroyed == 2 && !loader.removeHandler("z"), "Removal ownership");
	trace.clear();
	auto failure = loader.load(bytes);
	fails(failure, fcg::ImageErrorCode::DecodeFailure);
	require(failure.error().message.find("a (probe): probe failed") < failure.error().message.find("b: decode failed"),
			"Aggregate diagnostics order");
	fails(loader.load(bytes, std::string("png\0bad", 7)), fcg::ImageErrorCode::InvalidArgument);
	fails(loader.load(std::span<const std::byte>{}), fcg::ImageErrorCode::InvalidArgument);
	// Local loader destruction releases owned handlers; decoded results stay independent.
	{
		fcg::ImageLoader temporary;
		require(
			bool(temporary.registerHandler("temporary", std::make_unique<Handler>(trace, "temporary", 3, &destroyed))),
			"Temporary registration");
	}
	require(destroyed == 3 && result.width() == 9, "Local registry destruction");
	// Invalid helpers throw recoverably when used inside a function, without changing the global registry.
	const auto count = fcg::ImageLoader::global().handlerCount();
	bool threw = false;
	try {
		fcg::FormatHandlerRegistration invalid("", nullptr);
	}
	catch (const std::invalid_argument &) {
		threw = true;
	}
	require(threw && fcg::ImageLoader::global().handlerCount() == count, "Registration helper failure");
}

/// Native representation, move contracts, and externally borrowed pixel storage.
void ownership (const std::filesystem::path &fixtures)
{
	fcg::SDLImageFormatHandler decoder;
	auto indexed = take(decoder.load(fixtures / "palette.bmp"));
	require(indexed.format() == SDL_PIXELFORMAT_INDEX8 && SDL_GetSurfacePalette(indexed.handle()),
			"Lost native palette");
	require(indexed.pitch() >= indexed.width(), "Invalid native pitch");
	auto green = pixel(indexed, 1);
	require(green.g == 255 && green.r == 0 && green.b == 0, "Palette pixel mismatch");
	auto transparent = take(decoder.load(fixtures / "palette.gif"));
	Uint32 key;
	require(SDL_GetSurfaceColorKey(transparent.handle(), &key) || pixel(transparent, 1).a == 0,
			"Lost native transparency");
	auto *handle = indexed.handle();
	fcg::Image moved(std::move(indexed));
	require(moved.handle() == handle && !indexed.handle() && indexed.width() == 0 && indexed.height() == 0
				&& indexed.pitch() == 0 && indexed.format() == SDL_PIXELFORMAT_UNKNOWN,
			"Move constructor contract");
	moved = std::move(moved);
	require(moved.handle() == handle, "Self move must preserve storage");
	transparent = std::move(moved);
	require(!moved.handle() && transparent.handle() == handle, "Move assignment contract");
	fcg::Image empty(std::move(moved));
	require(!empty.handle(), "Moving empty owners");
	fails(fcg::Image::adopt(nullptr), fcg::ImageErrorCode::InvalidArgument);
	auto *rejected = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
	require(rejected, SDL_GetError());
	const auto width = rejected->w;
	rejected->w = 0;
	fails(fcg::Image::adopt(rejected), fcg::ImageErrorCode::InvalidArgument);
	rejected->w = width;
	// A rejected valid allocation remains caller-owned and can be adopted after fixing validation.
	auto recovered = take(fcg::Image::adopt(rejected));
	require(recovered.handle() == rejected, "Failed adoption consumed ownership");
	std::array<std::byte, 32> storage{};
	auto borrowed = take(fcg::Image::adopt(SDL_CreateSurfaceFrom(3, 2, SDL_PIXELFORMAT_RGB24, storage.data(), 16)));
	require(borrowed.pitch() == 16 && borrowed.handle()->pixels == storage.data(), "Adoption changed external storage");
	// storage outlives borrowed; SDL must not free external pixels on destruction.
}

/// Real decoders from disk and borrowed bytes, including hints and error lifetimes.
void decoding (const std::filesystem::path &fixtures, const std::filesystem::path &scratch)
{
	std::filesystem::create_directories(scratch);
	fcg::SDLImageFormatHandler decoder;
	auto &loader = fcg::ImageLoader::global();
	for (const auto *name : {"colors.png", "solid.jpg", "palette.bmp", "palette.gif", "colors.tga"}) {
		const auto path = fixtures / name;
		auto bytes = read(path);
		auto hint = path.extension().string();
		require(take(decoder.accepts(bytes, hint)), "Fixture detection");
		auto disk = take(loader.load(path));
		auto memory = take(loader.load(bytes, hint));
		auto direct = take(decoder.load(bytes, hint));
		require(disk.width() == 3 && disk.height() == 2 && memory.width() == 3 && direct.height() == 2,
				"Fixture dimensions");
		const auto a = pixel(disk), b = pixel(memory), c = pixel(direct);
		require(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a && a.r == c.r, "Disk/memory pixel mismatch");
		if (hint == ".jpg")
			require(std::abs(int(a.r) - 240) <= 3 && std::abs(int(a.g) - 30) <= 3 && std::abs(int(a.b) - 10) <= 3,
					"JPEG pixels");
		else
			require(a.r == 255 && a.g == 0 && a.b == 0, "Lossless pixels");
		if (hint == ".png" || hint == ".tga")
			require(a.a == 128, "Alpha lost");
	}
	auto png = read(fixtures / "colors.png");
	auto image = take(loader.load(png, ".TGA")); // A misleading magicless hint must not override PNG's signature.
	std::fill(png.begin(), png.end(), std::byte{});
	require(pixel(image).r == 255, "Decoded surface borrows encoded input");
	for (const auto &name : {std::filesystem::path("extensionless"), std::filesystem::path("wrong.tga"),
							 std::filesystem::path(u8"Größe-画像.png")}) {
		const auto path = scratch / name;
		std::filesystem::copy_file(fixtures / "colors.png", path, std::filesystem::copy_options::overwrite_existing);
		require(take(loader.load(path)).width() == 3, "Path/hint handling");
	}
	fails(loader.load(scratch / "missing.png"), fcg::ImageErrorCode::IOFailure);
	fails(loader.load(std::filesystem::path{}), fcg::ImageErrorCode::InvalidArgument);
	fails(loader.load(std::filesystem::path(std::string("bad\0name", 8))), fcg::ImageErrorCode::InvalidArgument);
	{
		std::ofstream empty(scratch / "empty.png");
	}
	fails(loader.load(scratch / "empty.png"), fcg::ImageErrorCode::InvalidArgument);
	const std::array unknown{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
	fails(loader.load(unknown), fcg::ImageErrorCode::UnsupportedFormat);
	fails(decoder.load(std::span<const std::byte>{}), fcg::ImageErrorCode::InvalidArgument);
	fails(decoder.accepts(unknown, std::string("png\0x", 5)), fcg::ImageErrorCode::InvalidArgument);
	const std::array truncated{std::byte{0x89}, std::byte{'P'}, std::byte{'N'}, std::byte{'G'},
							   std::byte{13},   std::byte{10},  std::byte{26},  std::byte{10}};
	auto failed = decoder.load(truncated, "png");
	fails(failed, fcg::ImageErrorCode::DecodeFailure);
	const auto message = failed.error().message;
	SDL_SetError("replacement diagnostic");
	require(failed.error().message == message && message.find("replacement diagnostic") == std::string::npos,
			"Error message must own its text");
	fails(loader.load(truncated), fcg::ImageErrorCode::DecodeFailure);
	fcg::ImageLoader local;
	require(bool(local.registerHandler("decoder", std::make_unique<fcg::SDLImageFormatHandler>())), "Explicit backend");
	require(take(local.load(fixtures / "colors.png")).width() == 3, "Independent real decoder");
}

// Anonymous namespace end
}


//////
//
// Functions
//

auto main (int argc, char **argv) -> int
{
	try {
		require(argc == 3, "Expected fixture and scratch paths");
		registry();
		ownership(argv[1]);
		decoding(argv[1], argv[2]);
		std::cout << "Image ownership, decoding, and registry checks passed\n";
		return 0;
	}
	catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
