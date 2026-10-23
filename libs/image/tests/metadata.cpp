
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>
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
#include "../src/sdl_image_internal.h"



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
	[[nodiscard]] auto decode (std::span<const std::byte>, std::string_view, fcg::ImageLoaderOptions options) const
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

/// Unwrap checked image operations with their diagnostic retained.
template<class T>
[[nodiscard]] auto take (std::expected<T, fcg::ImageError> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Construct a known float image, including invisible RGB that still contributes to stored-sample headroom.
[[nodiscard]] auto floatImage (float peak=8.f) -> fcg::Image
{
	auto image = take(fcg::Image::adopt(SDL_CreateSurface(2, 2, SDL_PIXELFORMAT_RGBA128_FLOAT)));
	require(bool(image.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(0, 0, {peak, -2.f, 1.f, 0.f})), "Write peak fixture");
	require(bool(image.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(1, 0, {1.f, 2.f, 3.f, 1.f})), "Write fixture");
	require(bool(image.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(0, 1, {0.f, 0.f, 0.f, 0.f})), "Write fixture");
	require(bool(image.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(1, 1, {-1.f, -2.f, -3.f, 1.f})), "Write fixture");
	return image;
}

/// Check selective assumptions, factual candidates, forwarding and common default finalization.
void loadingOptionsTests ()
{
	class Decoder final : public fcg::ImageFormatHandler
	{
	public:
		using ImageFormatHandler::resolveMetadata;

		auto accepts (std::span<const std::byte>, std::string_view) const -> std::expected<bool, fcg::ImageError> override {
			return true;
		}

		auto decode (std::span<const std::byte>, std::string_view, fcg::ImageLoaderOptions options) const
			-> std::expected<fcg::Image, fcg::ImageError> override
		{
			auto image = floatImage(peak);
			if (!SDL_SetSurfaceColorspace(image.handle(), encoding))
				return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, SDL_GetError()});
			if (whitePoint && !SDL_SetFloatProperty(SDL_GetSurfaceProperties(image.handle()), SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, *whitePoint))
				return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, SDL_GetError()});
			if (headroom && !SDL_SetFloatProperty(SDL_GetSurfaceProperties(image.handle()), SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, *headroom))
				return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, SDL_GetError()});
			return resolveMetadata(std::move(image), options, candidates, provenance);
		}

		float peak = 8.f;
		std::optional<float> headroom;
		std::optional<float> whitePoint;
		SDL_Colorspace encoding = SDL_COLORSPACE_UNKNOWN;
		fcg::ImageMetadataCandidates candidates;
		fcg::ImageMetadataProvenance provenance;
	};
	using Origin = fcg::ImageMetadataOrigin;
	const fcg::ImagePropertyProvenance assumed{Origin::Assumed, false, true, "Fixture format convention"};
	const fcg::ImageMetadataCandidates candidates{SDL_COLORSPACE_SRGB_LINEAR, {assumed, assumed, assumed}};
	const std::array<std::byte, 1> bytes{std::byte{1}};
	for (unsigned mask = 0; mask < 8; ++mask)
	{
		Decoder direct;
		direct.candidates = candidates;
		fcg::ImageLoader registry;
		require(bool(registry.registerHandler("fixture", std::make_unique<Decoder>(direct))), "Register fixture");
		const fcg::ImageLoaderOptions options{bool(mask & 1), bool(mask & 2), bool(mask & 4), {false, false}};
		const auto image = take(registry.load(bytes, ".PFM", options));
		const auto info = take(image.pixelInfo());
		require(info.provenance.colorPrimaries.origin == (mask & 1 ? Origin::Assumed : Origin::Unknown)
			&& info.provenance.transferCharacteristics.origin == (mask & 2 ? Origin::Assumed : Origin::Unknown)
			&& info.provenance.colorRange.origin == (mask & 4 ? Origin::Assumed : Origin::Unknown), "Selective provenance");
		require(!info.sdrWhitePoint && !info.hdrHeadroom, "Disabled brightness options were applied");
	}

	Decoder decoder;
	decoder.candidates = candidates;
	auto conservative = take(decoder.load(bytes, {}, fcg::ImageLoaderOptions::conservative()));
	require(take(conservative.pixelInfo()).colorspace == SDL_COLORSPACE_UNKNOWN, "Conservative loading assumed encoding");
	auto completed = take(decoder.load(bytes));
	const auto info = take(completed.pixelInfo());
	require(info.sdrWhitePoint == 1.f && info.hdrHeadroom == 8.f, "Default finalization did not complete HDR metadata");
	require(info.provenance.hdrHeadroom.origin == Origin::Derived
		&& info.provenance.hdrHeadroom.dependsOnAssumptions, "Derived peak lost its assumption dependency");
	// A supplied zero deliberately disables SDL tone mapping and must survive automatic completion.
	decoder.headroom = 0.f;
	decoder.peak = std::numeric_limits<float>::quiet_NaN();
	decoder.provenance.hdrHeadroom = {Origin::Declared, false, false, "Supplied content metadata"};
	auto supplied = take(decoder.load(bytes));
	require(supplied.pixelInfo()->hdrHeadroom == 0.f
		&& supplied.pixelInfo()->provenance.hdrHeadroom.origin == Origin::Declared,
		"Default completion overwrote supplied zero headroom");
	const auto unavailable = take(supplied.refreshMetadata());
	require(!unavailable.headroomDerived && !unavailable.diagnostic.empty() && supplied.pixelInfo()->hdrHeadroom == 0.f
		&& supplied.pixelInfo()->provenance.hdrHeadroom.origin == Origin::Declared,
		"Unavailable explicit analysis discarded supplied metadata");

	// Decoder declarations survive completion, forwarding, moves and independent surface copies.
	/* Exercise every declared property in default and conservative loading. */ {
		Decoder trustedDecoder;
		trustedDecoder.encoding = SDL_COLORSPACE_SRGB_LINEAR;
		trustedDecoder.whitePoint = 2.f;
		trustedDecoder.headroom = 0.f;
		const fcg::ImagePropertyProvenance declared{Origin::Declared, false, false, "Decoder supplied this value."};
		trustedDecoder.provenance = {declared, declared, declared, declared, declared};
		for (const auto options : {fcg::ImageLoaderOptions{}, fcg::ImageLoaderOptions::conservative()})
		{
			auto trusted = take(trustedDecoder.load(bytes, {}, options));
			auto moved = std::move(trusted);
			auto forwarded = take(Decoder::resolveMetadata(std::move(moved), options, candidates));
			const auto duplicate = take(fcg::Image::adopt(SDL_DuplicateSurface(forwarded.handle())));
			for (const auto *image : std::array<const fcg::Image*, 2>{&forwarded, &duplicate})
			{
				const auto metadata = take(image->pixelInfo());
				require(metadata.colorspace == trustedDecoder.encoding && metadata.sdrWhitePoint == 2.f
					&& metadata.hdrHeadroom == 0.f, "Decoder declarations were changed");
				for (const auto *record : {&metadata.provenance.colorPrimaries, &metadata.provenance.transferCharacteristics,
					&metadata.provenance.colorRange, &metadata.provenance.sdrWhitePoint, &metadata.provenance.hdrHeadroom})
					require(record->origin == Origin::Declared && !record->adapted && !record->dependsOnAssumptions
						&& record->explanation == declared.explanation, "Decoder declaration provenance was lost");
			}
		}
	}

	decoder.headroom.reset();
	decoder.peak = 8.f;
	decoder.provenance = {};
	for (auto *record : {&decoder.candidates.provenance.colorPrimaries,
		&decoder.candidates.provenance.transferCharacteristics, &decoder.candidates.provenance.colorRange}) {
		record->origin = Origin::Declared;
		record->dependsOnAssumptions = false;
	}
	const auto factual = take(decoder.load(bytes, {}, fcg::ImageLoaderOptions::conservative()));
	require(take(factual.pixelInfo()).colorspace == SDL_COLORSPACE_SRGB_LINEAR, "Conservative load lost factual candidates");
	decoder.encoding = static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL,
		SDL_COLOR_PRIMARIES_CUSTOM, SDL_TRANSFER_CHARACTERISTICS_HLG, SDL_MATRIX_COEFFICIENTS_IDENTITY,
		SDL_CHROMA_LOCATION_NONE));
	require(take(decoder.load(bytes)).pixelInfo()->colorspace == decoder.encoding, "Completion replaced custom declarations");

	decoder.encoding = SDL_COLORSPACE_SRGB_LINEAR;
	decoder.whitePoint = 2.f;
	decoder.headroom = 8.f;
	const fcg::ImagePropertyProvenance declared{Origin::Declared, false, false, "Known interpretation"};
	decoder.provenance = {declared, declared, declared, declared, {Origin::Assumed, false, true, "Content-only hint"}};
	const auto contentHint = take(decoder.load(bytes));
	const auto wide = static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL,
		SDL_COLOR_PRIMARIES_BT2020, SDL_TRANSFER_CHARACTERISTICS_LINEAR, SDL_MATRIX_COEFFICIENTS_IDENTITY,
		SDL_CHROMA_LOCATION_NONE));
	const auto transformed = take(contentHint.converted(SDL_PIXELFORMAT_RGBA128_FLOAT,
		{.lossPolicy=fcg::PixelLossPolicy::Allow, .colorspace=wide}));
	require(!transformed.pixelInfo()->provenance.colorPrimaries.dependsOnAssumptions,
		"A content-only peak assumption contaminated color-conversion evidence");
	require(transformed.pixelInfo()->sdrWhitePoint == 1.f
		&& transformed.pixelInfo()->provenance.sdrWhitePoint.origin == Origin::Assigned
		&& transformed.pixelInfo()->provenance.sdrWhitePoint.adapted,
		"Conversion failed to describe its selected destination white units");

	const auto failed = Decoder::resolveMetadata(std::unexpected(fcg::ImageError{
		fcg::ImageErrorCode::DecodeFailure, "source failure"}), {}, candidates);
	require(!failed && failed.error().message == "source failure", "Resolution changed source failure");
	auto forwarded = Decoder::resolveMetadata(std::move(completed), {}, candidates);
	require(forwarded && forwarded->pixelInfo()->provenance.hdrHeadroom.origin == Origin::Derived,
		"Forwarding lost established evidence");
}

/// Check current-image provenance, peak invalidation and both explicitly named conversion choices.
void provenanceLifecycleTests ()
{
	using Origin = fcg::ImageMetadataOrigin;
	auto source = floatImage();
	const auto first = take(source.refreshMetadata());
	require(first.whitePointAssumed && first.headroomDerived && first.diagnostic.empty(), "Refresh result incorrect");
	auto moved = std::move(source);
	require(!source.pixelInfo(), "Moved-from image retained metadata");
	source = std::move(moved);
	const auto duplicate = take(fcg::Image::adopt(SDL_DuplicateSurface(source.handle())));
	require(duplicate.pixelInfo()->provenance.hdrHeadroom.origin == Origin::Derived
		&& duplicate.pixelInfo()->hdrHeadroom == 8.f, "Surface duplication lost provenance");
	const auto snapshot = take(source.pixelInfo());
	(void)take(source.readColorFloat(0, 0));
	{
		auto reader = take(std::as_const(source).mapPixels<SDL_PIXELFORMAT_RGBA128_FLOAT>());
		require(source.pixelInfo()->hdrHeadroom == 8.f, "Read-only mapping invalidated peak");
	}
	const auto reordered = take(source.converted(SDL_PIXELFORMAT_BGRA128_FLOAT));
	require(reordered.pixelInfo()->hdrHeadroom == 8.f, "Exact reorder lost headroom");
	const auto narrowed = take(source.converted(SDL_PIXELFORMAT_RGBA64_FLOAT, {.lossPolicy=fcg::PixelLossPolicy::Allow}));
	require(!narrowed.pixelInfo()->hdrHeadroom, "Quantization retained stale headroom");
	const auto refreshed = take(source.convertedWithMetadata(SDL_PIXELFORMAT_RGBA64_FLOAT,
		{.lossPolicy=fcg::PixelLossPolicy::Allow}));
	require(refreshed.pixelInfo()->hdrHeadroom == 8.f, "Named conversion did not remeasure output");
	const auto sdr = take(source.convertedWithMetadata(SDL_PIXELFORMAT_RGBA32, {.lossPolicy=fcg::PixelLossPolicy::Allow}));
	require(!sdr.pixelInfo()->hdrHeadroom, "Ineligible conversion fabricated a peak");
	const auto wide = static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL,
		SDL_COLOR_PRIMARIES_BT2020, SDL_TRANSFER_CHARACTERISTICS_LINEAR, SDL_MATRIX_COEFFICIENTS_IDENTITY,
		SDL_CHROMA_LOCATION_NONE));
	const auto converted = take(source.convertedWithMetadata(SDL_PIXELFORMAT_RGBA128_FLOAT,
		{.lossPolicy=fcg::PixelLossPolicy::Allow, .colorspace=wide}));
	require(converted.pixelInfo()->provenance.colorPrimaries.origin == Origin::Assigned
		&& converted.pixelInfo()->provenance.colorPrimaries.adapted
		&& converted.pixelInfo()->provenance.colorPrimaries.dependsOnAssumptions,
		"Conversion discarded assumption-dependent processing");
	require(source.pixelInfo()->hdrHeadroom == snapshot.hdrHeadroom, "Independent operation changed source metadata");

	// Writable borrowing invalidates before any view escapes, even when the session performs no write.
	{
		auto writer = take(source.mapPixels<SDL_PIXELFORMAT_RGBA128_FLOAT>());
		require(!source.pixelInfo()->hdrHeadroom && !source.refreshMetadata(), "Writable mapping retained a valid peak");
	}
	(void)take(source.refreshMetadata());
	try {
		auto guard = take(source.editSurface(fcg::ImageSurfaceChanges::Pixels));
		require(!source.pixelInfo()->hdrHeadroom && !source.converted(SDL_PIXELFORMAT_RGBA128_FLOAT), "Edit guard lost exclusivity");
		auto transferred = std::move(guard);
		require(!guard.surface() && transferred.surface(), "Surface guard move did not transfer ownership");
		throw std::runtime_error("partial external edit");
	} catch (const std::runtime_error&) {
		(void)take(source.refreshMetadata());
	}
	require(source.pixelInfo()->hdrHeadroom == 8.f, "Exception left editing access active");
	require(SDL_SetSurfaceColorspace(source.handle(), wide), "Change raw metadata");
	require(bool(source.notifySurfaceChanged(fcg::ImageSurfaceChanges::ColorMetadata)), "Notify raw metadata edit");
	require(!source.pixelInfo()->hdrHeadroom && source.pixelInfo()->provenance.sdrWhitePoint.origin == Origin::Unknown,
		"External edit retained dependent provenance");
	require(SDL_SetSurfaceColorspace(source.handle(), SDL_COLORSPACE_SRGB_LINEAR), "Restore raw colorspace");
	require(source.pixelInfo()->provenance.hdrHeadroom.origin == Origin::Unknown, "Invalidated provenance resurrected");
	require(!source.notifySurfaceChanged(static_cast<fcg::ImageSurfaceChanges>(0)), "Invalid change categories accepted");
}

/// Exercise peak definitions and unavailable-analysis outcomes without loading a particular file format.
void brightnessTests ()
{
	for (const auto format : {SDL_PIXELFORMAT_RGB48_FLOAT, SDL_PIXELFORMAT_BGR48_FLOAT,
		SDL_PIXELFORMAT_RGBA64_FLOAT, SDL_PIXELFORMAT_ARGB64_FLOAT, SDL_PIXELFORMAT_BGRA64_FLOAT,
		SDL_PIXELFORMAT_ABGR64_FLOAT, SDL_PIXELFORMAT_RGB96_FLOAT, SDL_PIXELFORMAT_BGR96_FLOAT,
		SDL_PIXELFORMAT_RGBA128_FLOAT, SDL_PIXELFORMAT_ARGB128_FLOAT, SDL_PIXELFORMAT_BGRA128_FLOAT,
		SDL_PIXELFORMAT_ABGR128_FLOAT})
	{
		auto image = take(fcg::Image::adopt(SDL_CreateSurface(3, 2, format)));
		for (int y = 0; y < 2; ++y)
			for (int x = 0; x < 3; ++x)
				require(bool(image.writeColorFloat(x, y, {8.f, -1.f, 2.f, 0.f}, fcg::PixelLossPolicy::Allow)), "Write ordered float");
		require(SDL_SetFloatProperty(SDL_GetSurfaceProperties(image.handle()), SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, 2.f), "Set white");
		for (int y = 0; y < image.height(); ++y) {
			const auto visible = std::size_t(image.width()) * SDL_BYTESPERPIXEL(image.format());
			std::memset(static_cast<std::byte*>(image.handle()->pixels) + std::size_t(y) * image.pitch() + visible,
				0xff, std::size_t(image.pitch()) - visible);
		}
		const auto result = take(image.refreshMetadata());
		require(result.headroomDerived && !result.whitePointAssumed && image.pixelInfo()->hdrHeadroom == 4.f,
			"Float order, precision, alpha or explicit white affected peak incorrectly");
	}
	auto image = floatImage();
	const auto missingWhite = take(image.refreshMetadata({false, true}));
	require(!missingWhite.headroomDerived && !missingWhite.diagnostic.empty(), "Missing white was silently assumed");
	const auto noScan = take(image.refreshMetadata({true, false}));
	require(noScan.whitePointAssumed && !noScan.headroomDerived && !image.pixelInfo()->hdrHeadroom, "Disabled peak scan ran");
	for (const auto value : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		require(SDL_SetFloatProperty(SDL_GetSurfaceProperties(image.handle()), SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, value), "Set invalid white");
		const auto report = take(image.refreshMetadata());
		require(!report.headroomDerived && !report.diagnostic.empty(), "Invalid white accepted for peak");
	}
	require(SDL_SetFloatProperty(SDL_GetSurfaceProperties(image.handle()), SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, 1.f), "Restore white");
	for (const auto value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		require(bool(image.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(0, 0, {value, 0.f, 0.f, 1.f})), "Set nonfinite RGB");
		const auto report = take(image.refreshMetadata());
		require(!report.headroomDerived && !report.diagnostic.empty() && !image.pixelInfo()->hdrHeadroom, "Nonfinite RGB yielded headroom");
	}
	require(bool(image.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(0, 0,
		{std::numeric_limits<float>::max(), 0.f, 0.f, 1.f})), "Set huge RGB");
	require(SDL_SetFloatProperty(SDL_GetSurfaceProperties(image.handle()), SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT,
		std::numeric_limits<float>::denorm_min()), "Set tiny white");
	require(!take(image.refreshMetadata()).headroomDerived, "Unrepresentable ratio accepted");

	// Luminous zero-alpha pixels count; a nonfinite alpha component itself is ignored.
	image = floatImage();
	require(bool(image.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(0, 0,
		{9.f, 0.f, 0.f, std::numeric_limits<float>::quiet_NaN()})), "Set ignored alpha");
	(void)take(image.refreshMetadata());
	require(image.pixelInfo()->hdrHeadroom == 9.f, "Alpha contaminated RGB peak");
	require(bool(image.notifySurfaceChanged(fcg::ImageSurfaceChanges::Pixels)), "Notify pixel edit");
	require(!image.pixelInfo()->hdrHeadroom, "Raw notification retained content peak");
	for (const auto value : {0.f, -3.f})
	{
		require(bool(image.transformPixels<SDL_PIXELFORMAT_RGBA128_FLOAT>([value](auto pixel) {
			pixel.r = pixel.g = pixel.b = value;
			return pixel;
		})), "Set nonpositive image");
		(void)take(image.refreshMetadata());
		require(image.pixelInfo()->hdrHeadroom == 1.f, "Nonpositive content did not yield unit headroom");
	}
}

/// Check contextual SDL_image fallback choices and preservation of backend-provided labels.
void backendGuessTests ()
{
	const auto candidate = [] (std::string_view bytes, std::string_view type, SDL_Colorspace source=SDL_COLORSPACE_UNKNOWN,
		SDL_PixelFormat format=SDL_PIXELFORMAT_RGBA32) {
		return fcg::detail::guessedSDLImageEncoding(std::as_bytes(std::span(bytes)), type, format, source);
	};
	for (const auto source : {SDL_COLORSPACE_HDR10, static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB,
		SDL_COLOR_RANGE_FULL, SDL_COLOR_PRIMARIES_UNSPECIFIED, SDL_TRANSFER_CHARACTERISTICS_HLG,
		SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE))})
		require(SDL_COLORSPACEPRIMARIES(candidate("", "AVIF", source)) == SDL_COLOR_PRIMARIES_BT2020,
			"HDR transfer did not inform missing SDL_image primaries");
	for (const auto magic : {"P2", "P3", "P5", "P6"})
		require(SDL_COLORSPACETRANSFER(candidate(magic, "PNM")) == SDL_TRANSFER_CHARACTERISTICS_BT709,
			"PGM/PPM fallback did not use its format convention");
	require(SDL_COLORSPACETRANSFER(candidate("P4", "PNM")) == SDL_TRANSFER_CHARACTERISTICS_SRGB,
		"PBM fallback unexpectedly inferred a transfer curve");
	require(SDL_COLORSPACETRANSFER(candidate("", "TIF", SDL_COLORSPACE_UNKNOWN, SDL_PIXELFORMAT_RGB96_FLOAT))
		== SDL_TRANSFER_CHARACTERISTICS_LINEAR, "Floating SDL_image fallback was nonlinear");
	std::string qoi("qoif\0\0\0\1\0\0\0\1\4\0", 14);
	for (const unsigned flag : {0u, 1u})
	{
		qoi[13] = static_cast<char>(flag);
		require(SDL_COLORSPACETRANSFER(candidate(qoi, "QOI")) == (flag ? SDL_TRANSFER_CHARACTERISTICS_LINEAR
			: SDL_TRANSFER_CHARACTERISTICS_SRGB), "QOI fallback ignored the header's transfer flag");
	}
	require(SDL_COLORSPACETRANSFER(candidate(qoi, "PNG")) == SDL_TRANSFER_CHARACTERISTICS_SRGB,
		"Another selected format was overridden by fallback byte inspection");
#if FCG_SDL_IMAGE_QOI
	// Both loading policies retain the decoder's declarations.
	qoi.append("\xff\x80\x40\x20\xff", 5);
	qoi.append("\0\0\0\0\0\0\0\1", 8);
	fcg::SDLImageFormatHandler decoder;
	const auto bytes = std::as_bytes(std::span(qoi));
	const auto ordinary = take(decoder.load(bytes, "png"));
	const auto conservative = take(decoder.load(bytes, "png", fcg::ImageLoaderOptions::conservative()));
	const auto ordinaryInfo = take(ordinary.pixelInfo());
	const auto conservativeInfo = take(conservative.pixelInfo());
	require(ordinaryInfo.colorspace == conservativeInfo.colorspace, "Loading policy changed SDL_image's declarations");
	for (const auto *info : {&ordinaryInfo, &conservativeInfo})
		for (const auto *record : {&info->provenance.colorPrimaries, &info->provenance.transferCharacteristics,
			&info->provenance.colorRange})
			require(record->origin == fcg::ImageMetadataOrigin::Declared && !record->adapted
				&& !record->dependsOnAssumptions && record->explanation == "SDL_image supplied this value.",
				"SDL_image supplied metadata was not recorded as a declaration");
#endif
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
		provenanceLifecycleTests();
		brightnessTests();
		backendGuessTests();
		uploadCapabilityTests();
		std::cout << "Format snapshots, registry order, codec capabilities, and viewer filters passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
