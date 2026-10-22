
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

// FCG Framework
#include <FCG/Image/radiance_hdr.h>
#include <FCG/Image/pfm.h>
#include <FCG/Image/openexr.h>



//////
//
// Forward declarations
//

/// Decode a PNG with an application-owned stb implementation and change only its own flip setting.
[[nodiscard]] auto applicationSTBDecode (std::span<const std::byte> bytes) -> bool;



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Keep checks active in release builds.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Unwrap a successful operation while retaining decoder diagnostics on failure.
template<class T>
[[nodiscard]] auto take (std::expected<T, fcg::ImageError> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Copy an ASCII prefix into an encoded test image.
[[nodiscard]] auto encoded (std::string_view text) -> std::vector<std::byte> {
	const auto bytes = std::as_bytes(std::span(text));
	return {bytes.begin(), bytes.end()};
}

/// Add a wire byte without signed-char conversion dependencies.
void byte (std::vector<std::byte> &bytes, unsigned value) {
	bytes.push_back(static_cast<std::byte>(value));
}

/// Build a two-row PFM with independently encoded IEEE sample bytes.
[[nodiscard]] auto pfm (bool rgb, bool little, float scale=1) -> std::vector<std::byte>
{
	auto bytes = encoded(std::string(rgb ? "PF\r\n" : "Pf\r\n")
		+ "# fixture comment\r\n\r\n2 2\r\n" + (little ? "-" : "+") + std::to_string(scale) + "\r\n");
	for (unsigned i = 0; i < (rgb ? 12u : 4u); ++i)
	{
		const auto bits = std::bit_cast<std::uint32_t>(float(i + 1));
		for (unsigned c = 0; c < 4; ++c)
			byte(bytes, (bits >> ((little ? c : 3 - c) * 8)) & 255);
	}
	return bytes;
}

/// Read a native binary32 component without color conversion or nonfinite checks.
[[nodiscard]] auto sample (const fcg::Image &image, int x, int y, unsigned c) -> float {
	float value;
	const auto *surface = image.handle();
	std::memcpy(&value, static_cast<const std::byte*>(surface->pixels) + std::size_t(y) * surface->pitch
		+ (std::size_t(x) * SDL_BYTESPERPIXEL(surface->format)) + c * 4, 4);
	return value;
}

/// Construct a flat or RLE Radiance image with distinguishable rows and HDR values.
[[nodiscard]] auto hdr (bool rle) -> std::vector<std::byte>
{
	auto bytes = encoded(rle ? "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 8\n"
		: "#?RGBE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 2\n");
	for (unsigned y = 0; y < 2; ++y)
		if (rle) {
			for (const auto value : {2u, 2u, 0u, 8u, 136u, 128u, 136u, 64u, 136u, 32u, 136u, 130u + y})
				byte(bytes, value);
		} else
			for (unsigned x = 0; x < 2; ++x)
				for (const auto value : {128u, 64u, 32u, 130u + y})
					byte(bytes, value);
	return bytes;
}

/// Verify endian/scale/row handling, native sample preservation, and malformed PFM rejection.
void pfmTests ()
{
	fcg::PFMImageFormatHandler decoder;
	for (const bool rgb : {false, true})
		for (const bool little : {false, true})
		{
			auto bytes = pfm(rgb, little, 2);
			auto image = take(decoder.load(bytes, ".HDR"));
			require(image.width() == 2 && image.height() == 2 && image.format() == SDL_PIXELFORMAT_RGB96_FLOAT,
				"PFM dimensions or float representation changed");
			require(sample(image, 0, 0, 0) == (rgb ? 14.f : 6.f) && sample(image, 0, 1, 0) == 2.f,
				"PFM row orientation, scale, or byte order is incorrect");
			require(sample(image, 0, 0, 1) == (rgb ? 16.f : 6.f), "PFM RGB/grayscale mapping is incorrect");
			bytes.clear();
			require(sample(image, 0, 1, 0) == 2.f, "PFM image borrowed encoded bytes");
		}
	for (const auto header : {"Pf\n1 1\n0\n", "Pf\n1 1\nnan\n", "Pf\n1 1\ninf\n", "Pf\n0 1\n1\n",
		"Pf\n-1 1\n1\n", "PF\n2147483647 2147483647\n1\n", "PF\n2 invalid\n1\n", "PF\n1 1\n-1"})
		require(!decoder.load(encoded(header)), "Malformed PFM header was accepted");
	auto bytes = pfm(true, true);
	for (std::size_t size = 1; size < bytes.size(); ++size)
		require(!decoder.load(std::span(bytes).first(size)), "Truncated PFM was accepted");
	byte(bytes, 0);
	require(!decoder.load(bytes), "Trailing PFM bytes were accepted");
	// A binary sample may start with ASCII whitespace or a comment introducer.
	for (const unsigned first : {9u, 10u, 13u, 32u, 35u})
	{
		auto special = encoded("Pf\n1 1\n-1\n");
		for (const unsigned value : {first, 0u, 128u, 63u})
			byte(special, value);
		auto image = take(decoder.load(special));
		require(std::bit_cast<std::uint32_t>(sample(image, 0, 0, 0)) == (0x3f800000u | first),
			"PFM raster whitespace was consumed as header text");
	}
	for (const auto value : {-2.f, 275.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		auto special = encoded("Pf\n1 1\n-1\n");
		const auto bits = std::bit_cast<std::uint32_t>(value);
		for (unsigned c = 0; c < 4; ++c)
			byte(special, (bits >> (c * 8)) & 255);
		auto image = take(decoder.load(special));
		require(std::isnan(value) ? std::isnan(sample(image, 0, 0, 0)) : sample(image, 0, 0, 0) == value,
			"PFM special or HDR sample was clipped");
	}
	const auto unaligned = pfm(false, false);
	auto storage = encoded("x");
	storage.insert(storage.end(), unaligned.begin(), unaligned.end());
	require(bool(decoder.load(std::span(storage).subspan(1))), "Unaligned PFM input failed");
}

/// Verify HDR-only dispatch, RLE bounds, native values, and independent stb state.
void hdrTests ()
{
	fcg::RadianceHDRFormatHandler decoder;
	for (const bool rle : {false, true})
	{
		auto bytes = hdr(rle);
		auto image = take(decoder.load(bytes, "png"));
		require(sample(image, 0, 0, 0) == 2.f && sample(image, 0, 1, 0) == 4.f
			&& sample(image, 0, 0, 1) == 1.f, "HDR samples or orientation changed");
		for (std::size_t size = 1; size < bytes.size(); ++size)
			require(!decoder.load(std::span(bytes).first(size)), "Truncated HDR was accepted");
		const auto color = SDL_GetSurfaceColorspace(image.handle());
		require(SDL_COLORSPACEPRIMARIES(color) == SDL_COLOR_PRIMARIES_UNSPECIFIED
			&& SDL_COLORSPACETRANSFER(color) == SDL_TRANSFER_CHARACTERISTICS_LINEAR, "HDR encoding was assumed sRGB");
	}
	auto invalid = hdr(true);
	invalid[invalid.size() - 2] = std::byte{0};
	require(!decoder.load(invalid), "Zero HDR RLE run was accepted");
	for (const auto text : {"#?RGBE\nFORMAT=32-bit_rle_xyze\n\n-Y 1 +X 1\n",
		"#?RGBE\nFORMAT=32-bit_rle_rgbe\n\n+Y 1 +X 1\n",
		"#?RGBE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2147483647 +X 2147483647\n"})
		require(!decoder.load(encoded(text)), "Unsupported HDR layout was accepted");
	auto explicitColor = encoded("#?RGBE\nPRIMARIES=.64 .33 .30 .60 .15 .06 .3127 .3290\n"
		"FORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n");
	for (const auto value : {128u, 64u, 32u, 130u})
		byte(explicitColor, value);
	auto image = take(decoder.load(explicitColor));
	require(SDL_GetSurfaceColorspace(image.handle()) == SDL_COLORSPACE_SRGB_LINEAR, "HDR explicit Rec.709 primaries lost");
}

/// Load a fixture without introducing a runtime image-codec dependency in the test helper.
[[nodiscard]] auto read (const std::filesystem::path &path) -> std::vector<std::byte> {
	std::ifstream file(path, std::ios::binary);
	const std::vector<char> chars{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	const auto span = std::as_bytes(std::span(chars));
	return {span.begin(), span.end()};
}

/// Validate strict format ownership, global metadata, file loading, and hint precedence.
void dispatchTests (const std::filesystem::path &fixtures, const std::filesystem::path &scratch)
{
	fcg::RadianceHDRFormatHandler radiance;
	fcg::PFMImageFormatHandler portable;
	for (const auto name : {"colors.png", "solid.jpg", "palette.bmp", "palette.gif", "colors.tga", "float.exr"})
	{
		const auto bytes = read(fixtures / name);
		require(!take(radiance.accepts(bytes, "hdr")) && !radiance.load(bytes, "hdr"), "Radiance handler accepted another format");
		require(!take(portable.accepts(bytes, "pfm")), "PFM handler accepted another format");
		if (std::string_view(name) == "colors.png")
			require(applicationSTBDecode(bytes), "Application-owned stb PNG decoder failed");
	}
	hdrTests(); // The application copy changed its flip setting; FCG's copy must retain its own state.
	require(!take(radiance.accepts(pfm(true, true), "hdr")) && !take(portable.accepts(hdr(false), "pfm")),
		"HDR/PFM signature ownership overlaps");
	const auto pnm = encoded("P6\n1 1\n255\nRGB");
	require(!take(radiance.accepts(pnm, "hdr")) && !take(portable.accepts(pnm, "pfm")), "Integer PNM accepted as HDR/PFM");
	for (const auto hint : {"", "png", "tga", ".HDR", ".PFM"}) {
		require(bool(fcg::ImageLoader::global().load(hdr(false), hint)), "Global HDR hint dispatch failed");
		require(bool(fcg::ImageLoader::global().load(pfm(true, false), hint)), "Global PFM hint dispatch failed");
	}
	std::filesystem::create_directories(scratch);
	const auto bytes = pfm(false, true);
	const auto path = scratch / "misleading.TGA";
	/* Close the fixture before asking the decoders to read it. */ {
		std::ofstream file(path, std::ios::binary);
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	require(bool(portable.load(path)) && bool(fcg::ImageLoader::global().load(path)), "File overload or signature precedence failed");
	require(!portable.load(std::span<const std::byte>{}) && !radiance.load(hdr(false), std::string_view("a\0b", 3)),
		"Invalid argument accepted");
	const auto formats = fcg::ImageLoader::global().fileFormats();
	for (const auto extension : {"hdr", "rgbe", "pfm", "exr"})
	{
		const bool listed = std::ranges::any_of(formats, [&] (const auto &format) {
			return std::ranges::find(format.extensions, extension) != format.extensions.end();
		});
		require(listed == (std::string_view(extension) != "exr" || FCG_IMAGE_HAS_OPENEXR), "Global HDR format metadata incorrect");
	}
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run CPU-only HDR/PFM and decoder-isolation checks.
auto main (int argc, char **argv) -> int
{
	try {
		require(argc == 3, "Expected fixture and scratch directories");
		pfmTests();
		dispatchTests(argv[1], argv[2]);
		std::cout << "HDR/PFM samples, parsing, dispatch, metadata, ownership and stb isolation passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
