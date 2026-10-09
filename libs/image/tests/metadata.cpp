
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

// FCG Framework
#include <FCG/Image/sdl_image.h>

// Local includes
#include "sdl_image_capabilities.h"
#include "../../extras/include/FCG/Extras/file_filters.h"



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

/// A pre-metadata implementation remains source-compatible and advertises nothing.
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
	[[nodiscard]] auto load (std::span<const std::byte>, std::string_view) const
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
		backendTests();
		std::cout << "Format snapshots, registry order, codec capabilities, and viewer filters passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
