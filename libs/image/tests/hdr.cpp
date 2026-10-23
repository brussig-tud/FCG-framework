
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
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

/// Expected interpretation actions used to verify independent source normalization.
enum class ExpectedInterpretation { Known, Assumed, Adapted, AssumedAndAdapted };

/// Check assumption and adaptation independently for the three color components.
void requireAdjustments (const fcg::Image &image, std::array<ExpectedInterpretation, 3> expected)
{
	const auto info = take(image.pixelInfo());
	const std::array records{info.provenance.colorPrimaries, info.provenance.transferCharacteristics, info.provenance.colorRange};
	for (unsigned i = 0; i < records.size(); ++i) {
		const bool assumed = expected[i] == ExpectedInterpretation::Assumed || expected[i] == ExpectedInterpretation::AssumedAndAdapted;
		const bool adapted = expected[i] == ExpectedInterpretation::Adapted || expected[i] == ExpectedInterpretation::AssumedAndAdapted;
		require(records[i].dependsOnAssumptions == assumed && records[i].adapted == adapted, "Incorrect HDR/PFM provenance");
	}
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
			auto image = take(decoder.load(bytes, ".HDR", fcg::ImageLoaderOptions::conservative()));
			requireAdjustments(image, {});
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
	// Guesses label the independently decoded image while retaining all native samples.
	for (unsigned mask = 0; mask < 8; ++mask)
	{
		const fcg::ImageLoaderOptions options{bool(mask & 1), bool(mask & 2), bool(mask & 4)};
		auto guessed = take(decoder.load(pfm(true, true), {}, options));
		const auto color = SDL_GetSurfaceColorspace(guessed.handle());
		require(SDL_COLORSPACEPRIMARIES(color) == (options.inferColorPrimaries
			? SDL_COLOR_PRIMARIES_BT709 : SDL_COLOR_PRIMARIES_UNSPECIFIED), "PFM primary guessing toggle ignored");
		require(SDL_COLORSPACETRANSFER(color) == (options.inferTransferCharacteristics
			? SDL_TRANSFER_CHARACTERISTICS_LINEAR : SDL_TRANSFER_CHARACTERISTICS_UNSPECIFIED), "PFM transfer guessing toggle ignored");
		require(sample(guessed, 0, 0, 0) == 7.f, "Guessing changed PFM samples");
		using A = ExpectedInterpretation;
		requireAdjustments(guessed, {options.inferColorPrimaries ? A::Assumed : A::Known,
			options.inferTransferCharacteristics ? A::Assumed : A::Known, A::Known});
	}
	const fcg::ImageFormatHandler &base = decoder;
	auto guessed = take(base.load(pfm(true, false)));
	require(SDL_GetSurfaceColorspace(guessed.handle()) == SDL_COLORSPACE_SRGB_LINEAR, "Inherited guessing alias failed");
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
		auto image = take(decoder.load(bytes, "png", fcg::ImageLoaderOptions::conservative()));
		requireAdjustments(image, {});
		if (sample(image, 0, 0, 0) != 2.f || sample(image, 0, 1, 0) != 4.f || sample(image, 0, 0, 1) != 1.f)
			throw std::runtime_error("HDR samples or orientation changed: " + std::to_string(sample(image, 0, 0, 0))
				+ ", " + std::to_string(sample(image, 0, 1, 0)) + ", " + std::to_string(sample(image, 0, 0, 1)));
		for (std::size_t size = 1; size < bytes.size(); ++size)
			require(!decoder.load(std::span(bytes).first(size)), "Truncated HDR was accepted");
		const auto color = SDL_GetSurfaceColorspace(image.handle());
		require(SDL_COLORSPACEPRIMARIES(color) == SDL_COLOR_PRIMARIES_UNSPECIFIED
			&& SDL_COLORSPACETRANSFER(color) == SDL_TRANSFER_CHARACTERISTICS_LINEAR, "HDR encoding was assumed sRGB");
	}
	// Flat files can have RLE-sized widths without carrying scanline markers.
	auto wideFlat = encoded("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 8\n");
	for (unsigned y = 0; y < 2; ++y)
		for (unsigned x = 0; x < 8; ++x)
			for (const unsigned value : {128u, 64u, 32u, 130u + y})
				byte(wideFlat, value);
	auto wide = take(decoder.load(wideFlat, {}, fcg::ImageLoaderOptions::conservative()));
	require(sample(wide, 7, 1, 0) == 4.f && sample(wide, 7, 1, 2) == 1.f, "Wide flat HDR raster lost rows");
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
	// Shared recognition includes each SDL RGB primary set, its white point, and a custom-gamut result.
	const std::array<std::pair<std::string_view, SDL_ColorPrimaries>, 13> primaries{{
		{".64 .33 .30 .60 .15 .06 .3127 .3290", SDL_COLOR_PRIMARIES_BT709},
		{".67 .33 .21 .71 .14 .08 .310 .316", SDL_COLOR_PRIMARIES_BT470M},
		{".64 .33 .29 .60 .15 .06 .3127 .3290", SDL_COLOR_PRIMARIES_BT470BG},
		{".63 .34 .31 .595 .155 .07 .3127 .3290", SDL_COLOR_PRIMARIES_BT601},
		{".681 .319 .243 .692 .145 .049 .310 .316", SDL_COLOR_PRIMARIES_GENERIC_FILM},
		{".708 .292 .170 .797 .131 .046 .3127 .3290", SDL_COLOR_PRIMARIES_BT2020},
		{"1 0 0 1 0 0 .333333 .333333", SDL_COLOR_PRIMARIES_XYZ},
		{".68 .32 .265 .69 .15 .06 .314 .351", SDL_COLOR_PRIMARIES_SMPTE431},
		{".68 .32 .265 .69 .15 .06 .3127 .3290", SDL_COLOR_PRIMARIES_SMPTE432},
		{".63 .34 .295 .605 .155 .077 .3127 .3290", SDL_COLOR_PRIMARIES_EBU3213},
		{".64 .33 .30 .60 .15 .06 .333 .333", SDL_COLOR_PRIMARIES_CUSTOM},
		{"+.64009 +.33 +.30 +.60 +.15 +.06 +.3127 +.3290", SDL_COLOR_PRIMARIES_BT709},
		{".64011 .33 .30 .60 .15 .06 .3127 .3290", SDL_COLOR_PRIMARIES_CUSTOM}
	}};
	for (const auto &[coordinates, expected] : primaries)
	{
		auto tagged = encoded(std::string("#?RGBE\nPRIMARIES=") + std::string(coordinates)
			+ "\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n");
		for (const auto value : {128u, 64u, 32u, 130u})
			byte(tagged, value);
		auto decoded = take(decoder.load(tagged));
		const bool supported = expected == SDL_COLOR_PRIMARIES_BT709 || expected == SDL_COLOR_PRIMARIES_BT601
			|| expected == SDL_COLOR_PRIMARIES_BT2020 || expected == SDL_COLOR_PRIMARIES_SMPTE431
			|| expected == SDL_COLOR_PRIMARIES_SMPTE432;
		require(SDL_COLORSPACEPRIMARIES(SDL_GetSurfaceColorspace(decoded.handle())) == (supported ? expected : SDL_COLOR_PRIMARIES_BT709),
			"HDR known gamut was neither retained nor converted accurately");
		requireAdjustments(decoded, {supported ? ExpectedInterpretation::Known : ExpectedInterpretation::Adapted});
	}
	for (const auto &[gamma, exponent] : std::array<std::pair<std::string_view, float>, 4>{{
		{"1", 1.f}, {"2.2", 2.2f}, {"2.8", 2.8f}, {"2.4", 2.4f}}})
	{
		auto tagged = encoded(std::string("#?RGBE\nGAMMA=") + std::string(gamma)
			+ "\nPRIMARIES=.64 .33 .30 .60 .15 .06 .3127 .3290\n"
			"EXPOSURE=+2\nCOLORCORR=+1 +2 +3\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n");
		for (const auto value : {128u, 64u, 32u, 130u})
			byte(tagged, value);
		auto decoded = take(decoder.load(tagged));
		require(SDL_GetSurfaceColorspace(decoded.handle()) == SDL_COLORSPACE_SRGB_LINEAR, "HDR gamma was not linearized");
		requireAdjustments(decoded, {ExpectedInterpretation::Known, exponent == 1.f
			? ExpectedInterpretation::Known : ExpectedInterpretation::Adapted});
		require(std::abs(sample(decoded, 0, 0, 0) - std::pow(2.f, exponent)) < 1e-5f
			&& std::abs(sample(decoded, 0, 0, 2) - std::pow(.5f, exponent)) < 1e-5f,
			"HDR gamma conversion or stored radiometric adjustment is incorrect");
		require(SDL_HasProperty(SDL_GetSurfaceProperties(decoded.handle()), "fcg.image.source.color_metadata"),
			"HDR declared metadata was not retained");
	}

	for (const auto records : {"GAMMA=nan\n", "GAMMA=0\n", "GAMMA=128\n",
		"PRIMARIES=.1 .2 .1 .2 .1 .2 .3 .3\n", "PRIMARIES=.64 .33 .30 .60 .15 .06 .3 0\n", "PRIMARIES=.64 .33 .30 .60 .15 .06 .3127 nan\n",
		"GAMMA=1\nGAMMA=2.2\n", "PRIMARIES=.64 .33 .30 .60 .15 .06 .3127 .3290\nPRIMARIES=.1 .2 .3 .4 .5 .6 .3 .3\n"})
	{
		auto tagged = encoded(std::string("#?RGBE\n") + records + "FORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n");
		for (const auto value : {128u, 64u, 32u, 130u})
			byte(tagged, value);
		const auto rejected = decoder.load(tagged);
		require(!rejected && rejected.error().code == fcg::ImageErrorCode::DecodeFailure,
			"Malformed, contradictory or unrepresentable HDR color metadata was accepted");
	}

}

/// Verify conversion against independently calculated Bradford/RGB-XYZ reference values.
void hdrConversionTests ()
{
	fcg::RadianceHDRFormatHandler decoder;
	const auto fixture = [] (std::string_view records, std::array<unsigned, 4> pixel) {
		auto bytes = encoded(std::string("#?RADIANCE\n") + std::string(records)
			+ "FORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n");
		for (const auto value : pixel)
			byte(bytes, value);
		return bytes;
	};
	const auto near = [] (float actual, double expected) {
		require(std::abs(actual - expected) <= 2e-5 * std::max(1., std::abs(expected)), "Radiance color conversion differs from reference");
	};
	auto zero = take(decoder.load(fixture("", {128, 64, 32, 0})));
	for (unsigned c = 0; c < 3; ++c)
		require(sample(zero, 0, 0, c) == 0.f, "Zero RGBE exponent did not decode to black");
	// Columns of the conventional Radiance-to-BT.709 matrix, computed independently at double precision.
	const std::array<std::array<double, 3>, 3> reference{{
		{1.1644749390, .0075987688, .0035539147},
		{-.1565466170, .9930151959, .0182169224},
		{-.0079283220, -.0006139647, .9782291629}
	}};
	for (unsigned channel = 0; channel < 3; ++channel)
	{
		std::array<unsigned, 4> pixel{0, 0, 0, 129};
		pixel[channel] = 128;
		const auto bytes = fixture("", pixel);
		for (unsigned mask = 0; mask < 8; ++mask)
		{
			auto image = take(decoder.load(bytes, "png", {bool(mask & 1), bool(mask & 2), bool(mask & 4)}));
			requireAdjustments(image, {mask & 1 ? ExpectedInterpretation::AssumedAndAdapted : ExpectedInterpretation::Known});
			require(SDL_COLORSPACEPRIMARIES(SDL_GetSurfaceColorspace(image.handle())) == (mask & 1
				? SDL_COLOR_PRIMARIES_BT709 : SDL_COLOR_PRIMARIES_UNSPECIFIED), "Radiance primary inference toggle ignored");
			for (unsigned c = 0; c < 3; ++c)
				near(sample(image, 0, 0, c), mask & 1 ? reference[channel][c] : double(c == channel));
		}
	}
	for (const unsigned exponent : {129u, 132u})
	{
		auto neutral = take(decoder.load(fixture("", {128, 128, 128, exponent})));
		for (unsigned c = 0; c < 3; ++c)
			near(sample(neutral, 0, 0, c), std::ldexp(1., int(exponent) - 129));
	}
	// Declared custom white is converted on an ordinary load; gamma is decoded before mixing channels.
	const auto records = "PRIMARIES=.64 .33 .30 .60 .15 .06 .333 .333\nGAMMA=2\nEXPOSURE=2\nCOLORCORR=1 2 3\n";
	const auto bytes = fixture(records, {128, 64, 32, 130});
	auto custom = take(decoder.load(bytes));
	requireAdjustments(custom, {ExpectedInterpretation::Adapted, ExpectedInterpretation::Adapted});
	auto inferred = take(decoder.load(fixture("GAMMA=2\n", {128, 64, 32, 130})));
	requireAdjustments(inferred, {ExpectedInterpretation::AssumedAndAdapted, ExpectedInterpretation::Adapted});
	require(SDL_GetSurfaceColorspace(custom.handle()) == SDL_COLORSPACE_SRGB_LINEAR, "Ordinary Radiance loading did not normalize a custom encoding");
	const std::array<double, 3> expected{4.3799359997, 1.0223044413, .2668932999};
	for (unsigned c = 0; c < 3; ++c)
		near(sample(custom, 0, 0, c), expected[c]);
	require(std::string_view(SDL_GetStringProperty(SDL_GetSurfaceProperties(custom.handle()),
		"fcg.image.source.color_metadata", "")) == records, "Radiance conversion lost original metadata records");
	auto unknown = take(decoder.load(fixture("GAMMA=2\n", {128, 64, 32, 130}), {}, fcg::ImageLoaderOptions::conservative()));
	requireAdjustments(unknown, {ExpectedInterpretation::Known, ExpectedInterpretation::Adapted});
	require(SDL_COLORSPACEPRIMARIES(SDL_GetSurfaceColorspace(unknown.handle())) == SDL_COLOR_PRIMARIES_UNSPECIFIED,
		"Gamma conversion guessed disabled primaries");
	near(sample(unknown, 0, 0, 0), 4.);
	near(sample(unknown, 0, 0, 2), .25);
	// Linear sources in a supported gamut retain their native components exactly.
	auto supported = take(decoder.load(fixture("PRIMARIES=.708 .292 .170 .797 .131 .046 .3127 .3290\n", {128, 64, 32, 130})));
	requireAdjustments(supported, {});
	require(SDL_COLORSPACEPRIMARIES(SDL_GetSurfaceColorspace(supported.handle())) == SDL_COLOR_PRIMARIES_BT2020
		&& sample(supported, 0, 0, 0) == 2.f && sample(supported, 0, 0, 2) == .5f, "Supported Radiance gamut was transformed");
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
	auto portableDirect = take(portable.load(path));
	auto portableRegistered = take(fcg::ImageLoader::global().load(path));
	require(SDL_GetSurfaceColorspace(portableDirect.handle()) == SDL_COLORSPACE_SRGB_LINEAR
		&& SDL_GetSurfaceColorspace(portableRegistered.handle()) == SDL_COLORSPACE_SRGB_LINEAR, "File guessing aliases failed");
	requireAdjustments(portableDirect, {ExpectedInterpretation::Assumed, ExpectedInterpretation::Assumed});
	requireAdjustments(portableRegistered, {ExpectedInterpretation::Assumed, ExpectedInterpretation::Assumed});
	require(bool(portable.load(path)) && bool(fcg::ImageLoader::global().load(path)), "File overload or signature precedence failed");
	const auto radiancePath = scratch / "radiance.PNG";
	/* Close the Radiance fixture before testing filesystem dispatch. */ {
		const auto radianceBytes = hdr(false);
		std::ofstream file(radiancePath, std::ios::binary);
		file.write(reinterpret_cast<const char*>(radianceBytes.data()), static_cast<std::streamsize>(radianceBytes.size()));
	}
	const fcg::ImageFormatHandler &base = radiance;
	auto direct = take(base.load(radiancePath));
	auto registered = take(fcg::ImageLoader::global().load(radiancePath));
	requireAdjustments(direct, {ExpectedInterpretation::AssumedAndAdapted});
	requireAdjustments(registered, {ExpectedInterpretation::AssumedAndAdapted});
	require(SDL_GetSurfaceColorspace(direct.handle()) == SDL_COLORSPACE_SRGB_LINEAR
		&& SDL_GetSurfaceColorspace(registered.handle()) == SDL_COLORSPACE_SRGB_LINEAR
		&& sample(direct, 0, 0, 0) == sample(registered, 0, 0, 0)
		&& sample(direct, 0, 0, 0) > 2.f, "Radiance filesystem aliases lost conversion or selected the filename hint");
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
		hdrConversionTests();
		dispatchTests(argv[1], argv[2]);
		std::cout << "HDR/PFM samples, parsing, dispatch, metadata, ownership and stb isolation passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
