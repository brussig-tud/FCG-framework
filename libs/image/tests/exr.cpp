
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <tuple>
#include <stdexcept>

// OpenEXR library
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfOutputFile.h>
#include <ImfIntAttribute.h>
#include <ImfStandardAttributes.h>
#include <ImfTiledOutputFile.h>
#include <half.h>

// FCG Framework
#include <FCG/Image/openexr.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// The dependency's configured namespace alias.
namespace exr = OPENEXR_IMF_NAMESPACE;

/// Parameters for independently generated OpenEXR writer fixtures.
struct Fixture
{
	/// Use half storage for selected channels.
	bool half = false;

	/// Retain float precision in the green channel of a half fixture.
	bool mixed = false;

	/// Include associated alpha.
	bool alpha = false;

	/// Store luminance instead of RGB.
	bool gray = false;

	/// Store pixels as tiles instead of scanlines.
	bool tiled = false;

	/// Use decreasing scanline order.
	bool decreasing = false;

	/// Advertise unsigned color channels, which the handler must reject.
	bool integer = false;

	/// Subsample red, which the handler must reject.
	bool subsampled = false;

	/// Omit green, producing an unsupported partial color set.
	bool partial = false;

	/// Put color in a named layer, which the handler must reject.
	bool layered = false;

	/// Omit explicit primaries to check unknown color interpretation.
	bool untagged = false;

	/// Explicit color identifier, overriding chromaticities when identified.
	std::string interop;

	/// Override the generated primary coordinates.
	std::optional<exr::Chromaticities> chromaticities;

	/// Declare an ACES image container to exercise authoritative gamut metadata.
	bool aces = false;
};

/// Keep assertions active in release configurations.
void require (bool value, const char *message) {
	if (!value)
		throw std::runtime_error(message);
}

/// Unwrap with the framework's useful decode diagnostic intact.
template<class T>
[[nodiscard]] auto take (std::expected<T, fcg::ImageError> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Known negative, non-half-exact, HDR and alpha components for each fixture pixel.
[[nodiscard]] auto value (unsigned component, unsigned index) -> float {
	return component == 0 ? -2.25f - index : component == 1 ? 1.0001f + index
		: component == 2 ? 275.f + index : .375f;
}

/// Produce valid files with the independent OpenEXR writer, including layouts the reader must decline.
void writeFixture (const std::filesystem::path &path, Fixture options)
{
	const int width = options.subsampled ? 4 : 3;
	const int height = 2;
	const IMATH_NAMESPACE::V2i origin = options.subsampled ? IMATH_NAMESPACE::V2i(0, 0) : IMATH_NAMESPACE::V2i(-3, 7);
	exr::Header header(width, height);
	header.dataWindow() = IMATH_NAMESPACE::Box2i(origin, origin + IMATH_NAMESPACE::V2i(width - 1, height - 1));
	if (options.decreasing)
		header.lineOrder() = exr::DECREASING_Y;
	header.compression() = options.half ? exr::PIZ_COMPRESSION : exr::ZIP_COMPRESSION;
	if (!options.untagged)
		exr::addChromaticities(header, options.chromaticities.value_or(exr::Chromaticities()));
	if (!options.interop.empty())
		exr::addColorInteropID(header, options.interop);
	if (options.aces)
		header.insert("acesImageContainerFlag", exr::IntAttribute(1));
	if (options.tiled)
		header.setTileDescription(exr::TileDescription(2, 2, exr::ONE_LEVEL));
	exr::FrameBuffer frame;
	std::array<std::vector<float>, 4> floats;
	std::array<std::vector<IMATH_NAMESPACE::half>, 4> halves;
	std::array<std::vector<unsigned>, 4> integers;
	const std::array<const char*, 4> names = options.gray ? std::array{"Y", "", "", "A"} : std::array{"R", "G", "B", "A"};
	for (unsigned c = 0; c < (options.alpha ? 4u : 3u); ++c)
	{
		if ((options.gray && c > 0 && c < 3) || (options.partial && c == 1))
			continue;
		const std::string name = std::string(options.layered ? "beauty." : "") + names[c];
		const auto type = options.integer ? exr::UINT : options.half && !(options.mixed && c == 1) ? exr::HALF : exr::FLOAT;
		const int sampling = options.subsampled && c == 0 ? 2 : 1;
		header.channels().insert(name, exr::Channel(type, sampling, 1));
		const unsigned count = width * height;
		floats[c].resize(count);
		halves[c].resize(count);
		integers[c].resize(count);
		for (unsigned i = 0; i < count; ++i) {
			floats[c][i] = value(c, i);
			halves[c][i] = IMATH_NAMESPACE::half(value(c, i));
			integers[c][i] = 0xffffffffu;
		}
		const void *pixels = type == exr::HALF ? static_cast<const void*>(halves[c].data())
			: type == exr::UINT ? static_cast<const void*>(integers[c].data()) : floats[c].data();
		const auto size = type == exr::HALF ? 2u : 4u;
		frame.insert(name, exr::Slice::Make(type, pixels, header.dataWindow(), size, width * size, sampling, 1));
	}
	if (options.tiled) {
		exr::TiledOutputFile file(path.string().c_str(), header);
		file.setFrameBuffer(frame);
		file.writeTiles(0, file.numXTiles() - 1, 0, file.numYTiles() - 1);
	} else {
		exr::OutputFile file(path.string().c_str(), header);
		file.setFrameBuffer(frame);
		file.writePixels(height);
	}
}

/// Read encoded bytes independently of the framework's file overload.
[[nodiscard]] auto read (const std::filesystem::path &path) -> std::vector<std::byte> {
	std::ifstream stream(path, std::ios::binary);
	const std::vector<char> chars{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
	const auto bytes = std::as_bytes(std::span(chars));
	return {bytes.begin(), bytes.end()};
}

/// Verify stored native samples without color conversion.
[[nodiscard]] auto sample (const fcg::Image &image, unsigned x, unsigned y, unsigned component) -> float {
	float result;
	const auto *surface = image.handle();
	std::memcpy(&result, static_cast<const std::byte*>(surface->pixels) + std::size_t(y) * surface->pitch
		+ std::size_t(x) * SDL_BYTESPERPIXEL(surface->format) + component * 4, 4);
	return result;
}

/// Compare optional local assets; return \c false if any required image is absent.
[[nodiscard]] auto assetTests (const std::filesystem::path &directory) -> bool
{
	// An incomplete local asset set must not prevent the generated-fixture tests from running.
	for (const auto *name : {"memorial.exr", "memorial.pfm", "memorial.hdr"})
		if (!std::filesystem::is_regular_file(directory / name)) {
			std::cout << "Skipping optional Memorial HDR comparison: missing " << directory / name << '\n';
			return false;
		}

	// Once all assets are present, decoding and comparison errors remain test failures.
	auto exrImage = take(fcg::ImageLoader::global().load(directory / "memorial.exr"));
	auto pfmImage = take(fcg::ImageLoader::global().load(directory / "memorial.pfm"));
	auto hdrImage = take(fcg::ImageLoader::global().load(directory / "memorial.hdr"));
	require(exrImage.width() == 512 && exrImage.height() == 768 && pfmImage.width() == 512
		&& pfmImage.height() == 768 && hdrImage.width() == 512 && hdrImage.height() == 768, "Teaching asset dimensions incorrect");
	float peak = 0;
	float hdrPeak = 0;
	for (unsigned y = 0; y < 768; ++y)
		for (unsigned x = 0; x < 512; ++x)
			for (unsigned c = 0; c < 3; ++c) {
				const auto v = sample(exrImage, x, y, c);
				const auto rgbPeak = std::max({sample(exrImage, x, y, 0), sample(exrImage, x, y, 1), sample(exrImage, x, y, 2)});
				const auto rgbe = sample(hdrImage, x, y, c);
				require(v == sample(pfmImage, x, y, c), "Teaching EXR/PFM samples differ");
				require(std::abs(v - rgbe) <= std::max(1e-6f, rgbPeak / 64.f), "Teaching RGBE samples exceed quantization tolerance");
				peak = std::max(peak, v);
				hdrPeak = std::max(hdrPeak, rgbe);
			}
	require(peak == 275.f, "Teaching EXR/PFM HDR peak was lost");
	require(hdrPeak > 1.f, "Teaching RGBE HDR peak was lost");
	std::cout << "Teaching HDR assets: EXR/PFM equality, RGBE tolerance and peaks " << peak << '/' << hdrPeak << " verified\n";
	return true;
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run CPU-only EXR mapping, precision, layout and failure tests.
auto main (int argc, char **argv) -> int
{
	try
	{
		require(argc >= 2 && argc <= 3, "Expected scratch directory and optional asset directory");
		if (argc == 3)
			return assetTests(argv[2]) ? 0 : 77;

		// The mandatory decoder tests use only independently generated fixtures.
		const std::filesystem::path scratch = argv[1];
		std::filesystem::create_directories(scratch);
		fcg::OpenEXRImageFormatHandler decoder;
		const auto path = scratch / "writer.exr";
		for (const auto options : {Fixture{}, Fixture{.half=true}, Fixture{.half=true, .mixed=true, .alpha=true},
			Fixture{.alpha=true, .gray=true, .tiled=true}, Fixture{.decreasing=true}, Fixture{.untagged=true}})
		{
			writeFixture(path, options);
			auto bytes = read(path);
			auto image = take(decoder.load(bytes, "png"));
			require(image.width() == 3 && image.height() == 2, "EXR data window extent incorrect");
			require(image.format() == (options.alpha ? SDL_PIXELFORMAT_RGBA128_FLOAT : SDL_PIXELFORMAT_RGB96_FLOAT),
				"EXR float format or alpha incorrect");
			for (unsigned y = 0; y < 2; ++y)
				for (unsigned x = 0; x < 3; ++x)
					for (unsigned c = 0; c < (options.alpha ? 4u : 3u); ++c)
					{
						const auto channel = options.gray && c < 3 ? 0u : c;
						const auto expected = value(channel, y * 3 + x);
						const bool half = options.half && !(options.mixed && channel == 1);
						require(sample(image, x, y, c) == (half ? float(IMATH_NAMESPACE::half(expected)) : expected),
							"EXR samples, origin, channel mapping or float precision incorrect");
					}
			const auto colorspace = SDL_GetSurfaceColorspace(image.handle());
			require(options.untagged ? SDL_COLORSPACEPRIMARIES(colorspace) == SDL_COLOR_PRIMARIES_UNSPECIFIED
				: colorspace == SDL_COLORSPACE_SRGB_LINEAR, "EXR color metadata incorrect");
			for (const auto hint : {"", "tga", ".EXR"})
				require(bool(fcg::ImageLoader::global().load(bytes, hint)), "Global EXR dispatch failed");
			for (std::size_t count = 1; count < bytes.size(); ++count)
				require(!decoder.load(std::span(bytes).first(count)), "Truncated EXR was accepted");
			for (const unsigned flag : {8u, 16u, 128u}) {
				auto invalid = bytes;
				invalid[flag == 128 ? 7 : 5] |= static_cast<std::byte>(flag);
				require(!decoder.load(invalid), "Deep, multipart or unknown EXR flags accepted");
			}
			bytes.clear();
			require(sample(image, 0, 0, 0) == -2.25f, "EXR image borrowed encoded bytes");
		}
		for (const auto options : {Fixture{.integer=true}, Fixture{.subsampled=true}, Fixture{.partial=true}, Fixture{.layered=true}}) {
			writeFixture(path, options);
			require(!decoder.load(path), "Unsupported EXR channel mapping accepted");
		}
		// Recognize interop components independently, retaining supported and explicitly custom metadata.
		for (const auto &[id, primaries, transfer] : std::array{
			std::tuple{"lin_rec709_scene", SDL_COLOR_PRIMARIES_BT709, SDL_TRANSFER_CHARACTERISTICS_LINEAR},
			std::tuple{"lin_rec2020_scene", SDL_COLOR_PRIMARIES_BT2020, SDL_TRANSFER_CHARACTERISTICS_LINEAR},
			std::tuple{"lin_p3d65_display", SDL_COLOR_PRIMARIES_SMPTE432, SDL_TRANSFER_CHARACTERISTICS_LINEAR},
			std::tuple{"srgb_p3d65_scene", SDL_COLOR_PRIMARIES_SMPTE432, SDL_TRANSFER_CHARACTERISTICS_SRGB},
			std::tuple{"pq_rec2020_display", SDL_COLOR_PRIMARIES_BT2020, SDL_TRANSFER_CHARACTERISTICS_PQ},
			std::tuple{"hlg_rec2020_display", SDL_COLOR_PRIMARIES_BT2020, SDL_TRANSFER_CHARACTERISTICS_HLG},
			std::tuple{"g22_rec709_scene", SDL_COLOR_PRIMARIES_BT709, SDL_TRANSFER_CHARACTERISTICS_GAMMA22},
			std::tuple{"lin_ap1_scene", SDL_COLOR_PRIMARIES_CUSTOM, SDL_TRANSFER_CHARACTERISTICS_LINEAR},
			std::tuple{"data", SDL_COLOR_PRIMARIES_CUSTOM, SDL_TRANSFER_CHARACTERISTICS_CUSTOM},
			std::tuple{"studio:custom", SDL_COLOR_PRIMARIES_CUSTOM, SDL_TRANSFER_CHARACTERISTICS_CUSTOM}})
		{
			writeFixture(path, Fixture{.interop=id});
			auto image = take(decoder.loadAndGuess(path));
			const auto color = SDL_GetSurfaceColorspace(image.handle());
			require(SDL_COLORSPACEPRIMARIES(color) == primaries && SDL_COLORSPACETRANSFER(color) == transfer,
				"EXR color identifier precedence or partial recognition failed");
			require(sample(image, 0, 0, 0) == -2.25f, "EXR metadata recognition changed samples");
		}
		const exr::Chromaticities rec2020({.708f, .292f}, {.170f, .797f}, {.131f, .046f}, {.3127f, .3290f});
		writeFixture(path, Fixture{.interop="unknown", .chromaticities=rec2020});
		auto wide = take(decoder.load(path));
		require(SDL_COLORSPACEPRIMARIES(SDL_GetSurfaceColorspace(wide.handle())) == SDL_COLOR_PRIMARIES_BT2020,
			"EXR unknown identifier blocked explicit chromaticities");
		writeFixture(path, Fixture{.interop="lin_rec709_scene", .aces=true});
		auto aces = take(decoder.loadAndGuess(path));
		require(SDL_COLORSPACEPRIMARIES(SDL_GetSurfaceColorspace(aces.handle())) == SDL_COLOR_PRIMARIES_CUSTOM,
			"EXR ACES container was ignored or guessed over");
		std::cout << "EXR float/half, PIZ/ZIP, tiled/scanline, windows, alpha, channels and failures passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
