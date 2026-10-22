
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

// FCG Framework
#include <FCG/Image/sdl_image.h>

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "sdl_image_capabilities.h"
#include "../../extras/include/FCG/Extras/file_filters.h"
#include "../src/image_upload_internal.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Keep verification active in release builds.
void require (bool value, const char *message) {
	if (!value)
		throw std::runtime_error(message);
}

/// A minimal decoder advertises no file formats by default.
class LegacyHandler : public fcg::ImageFormatHandler
{
public:

	////
	// Interface: fcg::ImageFormatHandler

	/// Decline encoded data without probing.
	[[nodiscard]] auto accepts (std::span<const std::byte>, std::string_view) const
		-> std::expected<bool, fcg::ImageError> override {
		return false;
	}

	/// Never decode; metadata must be independent of load operations.
	[[nodiscard]] auto load (std::span<const std::byte>, std::string_view, fcg::ImageLoaderOptions options) const
		-> std::expected<fcg::Image, fcg::ImageError> override {
		throw std::runtime_error("Metadata queried the decoder");
	}
};

/// Configurable metadata source with owning return values.
class MetadataHandler final : public LegacyHandler
{
public:

	////
	// Object construction

	/// Own the metadata for later snapshots.
	explicit MetadataHandler(std::vector<fcg::ImageFileFormat> formats) : m_formats(std::move(formats)) {}


	////
	// Interface: fcg::ImageFormatHandler

	/// Copy rather than borrow labels or extension groups.
	[[nodiscard]] auto fileFormats () const -> std::vector<fcg::ImageFileFormat> override { return m_formats; }


private:

	////
	// Fields

	/// Handler-owned format records.
	std::vector<fcg::ImageFileFormat> m_formats;
};

/// Verify registry order, independent snapshots, overlaps, and repeated filter groups.
void registryTests ()
{
	fcg::ImageLoader loader;
	require(loader.fileFormats().empty(), "Empty registry advertised formats");
	require(bool(loader.registerHandler("legacy", std::make_unique<LegacyHandler>())), "Legacy registration failed");
	require(loader.fileFormats().empty(), "Legacy handler advertised formats");
	require(bool(loader.registerHandler("z", std::make_unique<MetadataHandler>(std::vector<fcg::ImageFileFormat>{
		{"JPEG second", {"jpeg", "jpg"}}, {"Other", {"custom", "png"}}}), 5)), "Registration failed");
	require(bool(loader.registerHandler("a", std::make_unique<MetadataHandler>(std::vector<fcg::ImageFileFormat>{
		{"JPEG first", {"jpg", "jpeg", "jpg"}}, {"Empty", {}}}), 5)), "Registration failed");
	require(bool(loader.registerHandler("higher", std::make_unique<MetadataHandler>(std::vector<fcg::ImageFileFormat>{
		{"PNG", {"png"}}}), 10)), "Registration failed");
	auto formats = loader.fileFormats();
	require(formats.size() == 5 && formats[0].name == "PNG" && formats[1].name == "JPEG first"
		&& formats[3].name == "JPEG second", "Registry metadata order or overlaps lost");
	const auto filters = fcg::extra::imageFileFilters(formats);
	require(filters.size() == 5 && filters[0].name == "All supported images" && filters[2].name == "JPEG first"
		&& filters.back().extensions == std::vector<std::string>{"*"}, "Filter order, deduplication, or first label lost");
	require(filters[0].extensions == std::vector<std::string>{"png", "jpg", "jpeg", "custom"}, "Aggregate extensions were not deduplicated");
	require(filters[2].extensions == std::vector<std::string>{"jpg", "jpeg"}, "Repeated extension inside a group survived");
	require(loader.removeHandler("a") && !loader.removeHandler("missing"), "Removal failed");
	require(formats[1].name == "JPEG first" && formats[1].extensions[0] == "jpg", "Snapshot borrowed removed handler");
	formats[0].name = "Mutated";
	formats[0].extensions[0] = "changed";
	require(loader.fileFormats()[0].name == "PNG" && loader.fileFormats()[0].extensions[0] == "png", "Snapshot mutation changed registry");
	require(fcg::extra::imageFileFilters({}).size() == 1, "Empty metadata did not retain all-files filter");
}

/// Check exact filter contents and order for default, explicit inclusion, and opt-out calls.
void filterTests ()
{
	using fcg::ImageFileFormat;
	using fcg::extra::FileDialogFilter;
	const auto equal = [] (const auto &left, const auto &right) {
		return std::ranges::equal(left, right, [] (const auto &a, const auto &b) {
			return a.name == b.name && a.extensions == b.extensions;
		});
	};
	const auto check = [&] (const std::vector<ImageFileFormat> &formats, std::vector<FileDialogFilter> expected) {
		require(equal(fcg::extra::imageFileFilters(formats, false), expected), "Opt-out filter contents or order differ");
		expected.push_back({"All files", {"*"}});
		require(equal(fcg::extra::imageFileFilters(formats), expected), "Default filter contents or order differ");
		require(equal(fcg::extra::imageFileFilters(formats, true), expected), "Explicit inclusion differs from default");
	};
	check({}, {});
	check({{"PNG", {"png", "png"}}, {"JPEG", {"jpg", "jpeg"}}}, {
		{"All supported images", {"png", "jpg", "jpeg"}}, {"PNG", {"png"}}, {"JPEG", {"jpg", "jpeg"}}
	});
	check({{"Any image", {"*"}}}, {{"Any image", {"*"}}});
	check({{"First wildcard", {"*", "*"}}, {"Second wildcard", {"*"}}, {"Empty", {}}}, {
		{"First wildcard", {"*"}}
	});
	for (std::size_t position = 0; position < 3; ++position)
	{
		std::vector<ImageFileFormat> formats{{"PNG", {"png"}}, {"JPEG", {"jpg", "jpeg", "png"}}};
		formats.insert(formats.begin() + position, {"Any image", {"*"}});
		std::vector<FileDialogFilter> expected{{"PNG", {"png"}}, {"JPEG", {"jpg", "jpeg", "png"}}};
		expected.insert(expected.begin() + position, {"Any image", {"*"}});
		expected.insert(expected.begin(), {"All supported images", {"png", "jpg", "jpeg"}});
		check(formats, std::move(expected));
	}
	check({{"Invalid mixed filter", {"png", "*"}}}, {
		{"All supported images", {"png"}}, {"Invalid mixed filter", {"png", "*"}}
	});
}

/// Compare every advertised backend extension with the dependency's resolved capabilities.
void backendTests ()
{
	const auto formats = fcg::SDLImageFormatHandler().fileFormats();
	std::set<std::string> extensions;
	for (const auto &format : formats) {
		require(!format.name.empty() && !format.extensions.empty(), "Empty backend format");
		for (const auto &extension : format.extensions)
			require(extensions.insert(extension).second, "Backend repeated an extension");
	}
	const std::pair<const char*, bool> expected[] {
		{"avif", FCG_SDL_IMAGE_AVIF}, {"bmp", FCG_SDL_IMAGE_BMP}, {"ico", FCG_SDL_IMAGE_BMP}, {"cur", FCG_SDL_IMAGE_BMP},
		{"gif", FCG_SDL_IMAGE_GIF}, {"jpg", FCG_SDL_IMAGE_JPG}, {"jpeg", FCG_SDL_IMAGE_JPG}, {"jxl", FCG_SDL_IMAGE_JXL},
		{"lbm", FCG_SDL_IMAGE_LBM}, {"iff", FCG_SDL_IMAGE_LBM}, {"pcx", FCG_SDL_IMAGE_PCX}, {"png", FCG_SDL_IMAGE_PNG},
		{"pnm", FCG_SDL_IMAGE_PNM}, {"pbm", FCG_SDL_IMAGE_PNM}, {"pgm", FCG_SDL_IMAGE_PNM}, {"ppm", FCG_SDL_IMAGE_PNM},
		{"qoi", FCG_SDL_IMAGE_QOI}, {"svg", FCG_SDL_IMAGE_SVG}, {"tga", FCG_SDL_IMAGE_TGA}, {"tif", FCG_SDL_IMAGE_TIF},
		{"tiff", FCG_SDL_IMAGE_TIF}, {"webp", FCG_SDL_IMAGE_WEBP}, {"xcf", FCG_SDL_IMAGE_XCF}, {"xpm", FCG_SDL_IMAGE_XPM},
		{"xv", FCG_SDL_IMAGE_XV}
	};
	for (const auto &[extension, enabled] : expected)
		require(extensions.contains(extension) == enabled, "Backend metadata differs from resolved configuration");
	require(!extensions.contains("ani"), "Animation-only codec advertised");
	require(fcg::ImageLoader::global().fileFormats().size() >= formats.size(), "Singleton lost shipped metadata");
}

/// Exercise native-format rejection and widening without depending on a particular GPU's limitations.
void uploadCapabilityTests ()
{
	for (const auto format : {SDL_PIXELFORMAT_RGB24, SDL_PIXELFORMAT_RGB48, SDL_PIXELFORMAT_RGB48_FLOAT,
		SDL_PIXELFORMAT_RGB96_FLOAT, SDL_PIXELFORMAT_ARGB2101010, SDL_PIXELFORMAT_INDEX4MSB})
	{
		const auto source = *fcg::pixelFormatInfo(format);
		const auto wide = fcg::detail::storedImageLayout(source, [] (SDL_GPUTextureFormat candidate) {
			return candidate == SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
		});
		require(wide && wide->cpu == SDL_PIXELFORMAT_RGBA128_FLOAT, "Preservation did not widen to float when native sampling was unavailable");
		const auto unavailable = fcg::detail::storedImageLayout(source, [] (SDL_GPUTextureFormat) { return false; });
		require(!unavailable, "Preservation selected an unsupported texture representation");
	}
	const auto yuv = *fcg::pixelFormatInfo(SDL_PIXELFORMAT_NV12);
	require(!fcg::detail::storedImageLayout(yuv, [] (SDL_GPUTextureFormat) { return true; }), "Preservation selected RGB for YUV storage");
}

/// Check selective metadata assumptions through a client decoder and registry dispatch.
void loadingOptionsTests ()
{
	class Decoder final : public fcg::ImageFormatHandler
	{
	public:
		Decoder(SDL_PixelFormat format, SDL_Colorspace encoding) : format(format), encoding(encoding) {}

		auto accepts (std::span<const std::byte>, std::string_view) const -> std::expected<bool, fcg::ImageError> override {
			return true;
		}

		auto load (std::span<const std::byte>, std::string_view, fcg::ImageLoaderOptions options) const
			-> std::expected<fcg::Image, fcg::ImageError> override
		{
			auto image = fcg::Image::adopt(SDL_CreateSurface(1, 1, format));
			if (!image)
				return image;
			if (!SDL_SetSurfaceColorspace(image->handle(), encoding))
				return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, SDL_GetError()});
			return finishLoading(std::move(image), options);
		}

		SDL_PixelFormat format;
		SDL_Colorspace encoding;
	};
	const std::array<std::byte, 1> bytes{std::byte{1}};
	for (const auto format : {SDL_PIXELFORMAT_RGBA32, SDL_PIXELFORMAT_RGB96_FLOAT, SDL_PIXELFORMAT_INDEX8})
		for (unsigned mask = 0; mask < 8; ++mask)
		{
			fcg::ImageLoader registry;
			require(bool(registry.registerHandler("options", std::make_unique<Decoder>(format, SDL_COLORSPACE_UNKNOWN))),
				"Options decoder registration failed");
			auto image = registry.load(bytes, {}, {bool(mask & 1), bool(mask & 2), bool(mask & 4)});
			require(bool(image), "Registry failed to forward loader options");
			const auto color = SDL_GetSurfaceColorspace(image->handle());
			require((SDL_COLORSPACEPRIMARIES(color) == SDL_COLOR_PRIMARIES_BT709) == bool(mask & 1),
				"Selective primary guessing failed");
			const auto transfer = SDL_COLORSPACETRANSFER(color);
			require(bool(mask & 2) ? transfer == (SDL_ISPIXELFORMAT_FLOAT(format)
				? SDL_TRANSFER_CHARACTERISTICS_LINEAR : SDL_TRANSFER_CHARACTERISTICS_SRGB)
				: transfer == SDL_TRANSFER_CHARACTERISTICS_UNKNOWN,
				"Selective transfer guessing failed");
			require((SDL_COLORSPACERANGE(color) == SDL_COLOR_RANGE_FULL) == bool(mask & 4), "Selective range guessing failed");
		}
	const auto declared = static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_LIMITED,
		SDL_COLOR_PRIMARIES_CUSTOM, SDL_TRANSFER_CHARACTERISTICS_HLG, SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE));
	Decoder decoder(SDL_PIXELFORMAT_RGBA32, declared);
	auto image = decoder.loadAndGuess(bytes);
	require(image && SDL_GetSurfaceColorspace(image->handle()) == declared, "Guessing replaced identified custom metadata");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Headless metadata and imgview filter test entry point.
auto main () -> int
{
	try {
		registryTests();
		filterTests();
		backendTests();
		loadingOptionsTests();
		uploadCapabilityTests();
		std::cout << "Format snapshots, registry order, codec capabilities, and viewer filters passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
