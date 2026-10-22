
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

/// Check that handlers choose candidates independently of storage and that only requested missing components change.
void loadingOptionsTests ()
{
	class Decoder final : public fcg::ImageFormatHandler
	{
	public:
		Decoder(SDL_PixelFormat format, SDL_Colorspace encoding, SDL_Colorspace guesses)
			: format(format), encoding(encoding), guesses(guesses) {}

		using ImageFormatHandler::finishLoading;

		auto accepts (std::span<const std::byte>, std::string_view) const -> std::expected<bool, fcg::ImageError> override {
			return true;
		}

		auto load (std::span<const std::byte>, std::string_view, fcg::ImageLoaderOptions options) const
			-> std::expected<fcg::Image, fcg::ImageError> override
		{
			auto image = fcg::Image::adopt(SDL_CreateSurface(2, 2, format));
			if (!image)
				return image;
			if (!SDL_SetSurfaceColorspace(image->handle(), encoding))
				return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, SDL_GetError()});
			return finishLoading(std::move(image), options, guesses);
		}

		SDL_PixelFormat format;
		SDL_Colorspace encoding;
		SDL_Colorspace guesses;
	};
	const auto encoding = [] (SDL_ColorPrimaries primaries, SDL_TransferCharacteristics curve, SDL_ColorRange range) {
		return static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, range,
			primaries, curve, SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE));
	};
	const auto wide = encoding(SDL_COLOR_PRIMARIES_BT2020, SDL_TRANSFER_CHARACTERISTICS_PQ, SDL_COLOR_RANGE_LIMITED);
	const auto partial = encoding(SDL_COLOR_PRIMARIES_CUSTOM, SDL_TRANSFER_CHARACTERISTICS_UNSPECIFIED, SDL_COLOR_RANGE_FULL);
	const std::array<std::byte, 1> bytes{std::byte{1}};
	for (const auto format : {SDL_PIXELFORMAT_RGBA32, SDL_PIXELFORMAT_RGB96_FLOAT, SDL_PIXELFORMAT_INDEX8})
		for (const auto guesses : {SDL_COLORSPACE_SRGB_LINEAR, wide})
			for (const auto original : {SDL_COLORSPACE_UNKNOWN, partial})
				for (unsigned mask = 0; mask < 8; ++mask)
				{
					const fcg::ImageLoaderOptions options{bool(mask & 1), bool(mask & 2), bool(mask & 4)};
					fcg::ImageLoader registry;
					require(bool(registry.registerHandler("options", std::make_unique<Decoder>(format, original, guesses))),
						"Options decoder registration failed");
					auto image = registry.load(bytes, {}, options);
					require(bool(image), "Registry failed to forward loader options");
					const auto info = image->pixelInfo();
					require(bool(info), "Could not inspect loading annotations");
					const auto &actions = info->loadAdjustments;
					using A = fcg::ImageLoadAdjustment;
					require(actions.colorPrimaries == (options.guessColorPrimaries && original != partial ? A::Guessed : A::None)
						&& actions.transferCharacteristics == (options.guessTransferCharacteristics ? A::Guessed : A::None)
						&& actions.colorRange == (options.guessColorRange && original != partial ? A::Guessed : A::None),
						"Annotations did not distinguish actual guesses from enabled options");
					const auto color = SDL_GetSurfaceColorspace(image->handle());
					require(SDL_COLORSPACEPRIMARIES(color) == (original == partial ? SDL_COLOR_PRIMARIES_CUSTOM
						: options.guessColorPrimaries ? SDL_COLORSPACEPRIMARIES(guesses) : SDL_COLOR_PRIMARIES_UNKNOWN),
						"Handler-chosen primary guessing failed");
					require(SDL_COLORSPACETRANSFER(color) == (options.guessTransferCharacteristics ? SDL_COLORSPACETRANSFER(guesses)
						: SDL_COLORSPACETRANSFER(original)), "Handler-chosen transfer guessing failed");
					require(SDL_COLORSPACERANGE(color) == (original == partial ? SDL_COLOR_RANGE_FULL
						: options.guessColorRange ? SDL_COLORSPACERANGE(guesses) : SDL_COLOR_RANGE_UNKNOWN),
						"Handler-chosen range guessing failed");
				}
	for (const auto format : {SDL_PIXELFORMAT_RGBA32, SDL_PIXELFORMAT_NV12})
	{
		const auto declared = format == SDL_PIXELFORMAT_NV12 ? SDL_COLORSPACE_BT709_LIMITED
			: encoding(SDL_COLOR_PRIMARIES_CUSTOM, SDL_TRANSFER_CHARACTERISTICS_HLG, SDL_COLOR_RANGE_LIMITED);
		Decoder decoder(format, declared, SDL_COLORSPACE_SRGB);
		auto image = decoder.loadAndGuess(bytes);
		require(image && SDL_GetSurfaceColorspace(image->handle()) == declared, "Guessing replaced identified metadata or YUV encoding");
	}
	Decoder yuv(SDL_PIXELFORMAT_NV12, SDL_COLORSPACE_UNKNOWN, wide);
	auto unchanged = yuv.loadAndGuess(bytes);
	require(unchanged && SDL_GetSurfaceColorspace(unchanged->handle()) == SDL_COLORSPACE_UNKNOWN, "Guessing filled YUV metadata");
	auto failed = Decoder::finishLoading(std::unexpected(fcg::ImageError{fcg::ImageErrorCode::DecodeFailure, "source error"}),
		{true, true, true}, wide, {fcg::ImageLoadAdjustment::Adapted});
	require(!failed && failed.error().message == "source error", "Guessing changed a decode failure");
	// Handler-supplied adaptations combine with guesses; a forwarding pass preserves the combined result.
	using A = fcg::ImageLoadAdjustment;
	for (const auto action : {A::None, A::Guessed, A::Adapted, A::GuessedAndAdapted})
	{
		auto image = fcg::Image::adopt(SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32));
		require(bool(image) && SDL_SetSurfaceColorspace(image->handle(), SDL_COLORSPACE_UNKNOWN), "Annotation fixture failed");
		image = Decoder::finishLoading(std::move(image), {true, false, false}, SDL_COLORSPACE_SRGB,
			{.colorPrimaries=action});
		require(image && image->pixelInfo()->loadAdjustments.colorPrimaries == (action | A::Guessed), "Completed loading actions were not combined");
		image = Decoder::finishLoading(std::move(image), {true, true, true}, SDL_COLORSPACE_SRGB);
		require(image && image->pixelInfo()->loadAdjustments.colorPrimaries == (action | A::Guessed), "Forwarding lost loading annotations");
	}
	Decoder unresolved(SDL_PIXELFORMAT_RGBA32, SDL_COLORSPACE_UNKNOWN, SDL_COLORSPACE_UNKNOWN);
	auto unknown = unresolved.loadAndGuess(bytes);
	require(unknown && unknown->pixelInfo()->loadAdjustments.colorPrimaries == A::None
		&& unknown->pixelInfo()->loadAdjustments.transferCharacteristics == A::None
		&& unknown->pixelInfo()->loadAdjustments.colorRange == A::None, "Unresolved metadata was annotated as guessed");

}

/// Check loader annotation lifetime, explicit conversion clearing and raw SDL metadata mismatch suppression.
void loadAdjustmentLifecycleTests ()
{
	using A = fcg::ImageLoadAdjustment;
	static_assert(static_cast<unsigned>(A::None) == 0 && static_cast<unsigned>(A::Guessed) == 1
		&& static_cast<unsigned>(A::Adapted) == 2 && static_cast<unsigned>(A::GuessedAndAdapted) == 3);
	static_assert((A::Guessed | A::Adapted) == A::GuessedAndAdapted);
	static_assert((A::GuessedAndAdapted & A::Guessed) == A::Guessed);
	static_assert((A::GuessedAndAdapted & A::Adapted) == A::Adapted);
	static_assert((A::Guessed & A::Adapted) == A::None);
	static_assert(std::is_same_v<decltype(A::Guessed | A::Adapted), A>);
	static_assert(std::is_same_v<decltype(A::Guessed & A::Adapted), A>);
	class Loader final : public fcg::ImageFormatHandler
	{
	public:
		auto accepts (std::span<const std::byte>, std::string_view) const -> std::expected<bool, fcg::ImageError> override {
			return true;
		}

		auto load (std::span<const std::byte>, std::string_view, fcg::ImageLoaderOptions options) const
			-> std::expected<fcg::Image, fcg::ImageError> override
		{
			std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(SDL_CreateSurface(2, 2, format), SDL_DestroySurface);
			if (!surface || !SDL_SetSurfaceColorspace(surface.get(), SDL_COLORSPACE_SRGB_LINEAR)
				|| (SDL_ISPIXELFORMAT_INDEXED(format) && !SDL_CreateSurfacePalette(surface.get())))
				return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, SDL_GetError()});
			std::fill_n(static_cast<std::byte*>(surface->pixels), surface->pitch * surface->h, std::byte{0});
			auto image = fcg::Image::adopt(surface.get());
			if (image)
				surface.release();
			return finishLoading(std::move(image), options, SDL_COLORSPACE_SRGB, adjustments);
		}

		SDL_PixelFormat format = SDL_PIXELFORMAT_RGBA32;
		fcg::ImageLoadAdjustments adjustments{A::GuessedAndAdapted, A::Adapted, A::Guessed};
	};
	const auto take = [] (std::expected<fcg::Image, fcg::ImageError> result) {
		if (!result)
			throw std::runtime_error(result.error().message);
		return std::move(*result);
	};
	const auto check = [] (const fcg::Image &image, fcg::ImageLoadAdjustments expected) {
		const auto info = image.pixelInfo();
		require(info && info->loadAdjustments.colorPrimaries == expected.colorPrimaries
			&& info->loadAdjustments.transferCharacteristics == expected.transferCharacteristics
			&& info->loadAdjustments.colorRange == expected.colorRange, "Incorrect loading annotations after image operation");
	};
	const fcg::ImageLoadAdjustments all{A::GuessedAndAdapted, A::Adapted, A::Guessed};
	const std::array<std::byte, 1> bytes{std::byte{1}};
	Loader loader;
	auto source = take(loader.load(bytes, {}, {}));
	check(source, all);
	auto snapshot = source.pixelInfo();
	require(bool(snapshot), "Could not snapshot loading annotations");
	snapshot->loadAdjustments = {};
	check(source, all);
	const auto analysis = source.conversionInfo(SDL_PIXELFORMAT_BGRA32);
	require(analysis && analysis->source.loadAdjustments.colorPrimaries == A::GuessedAndAdapted, "Conversion analysis lost source annotations");
	// Ownership transfers and adoption retain surface-owned records, while ordinary surfaces have no annotations.
	auto moved = std::move(source);
	require(!source.pixelInfo(), "Moved-from image retained metadata access");
	source = take(fcg::Image::adopt(SDL_CreateSurface(2, 2, SDL_PIXELFORMAT_RGBA32)));
	check(source, {});
	source = std::move(moved);
	check(source, all);
	check(take(fcg::Image::adopt(SDL_DuplicateSurface(source.handle()))), all);
	require(bool(source.writePixel<SDL_PIXELFORMAT_RGBA32>(0, 0, {16, 32, 64, 255})), "Pixel edit failed");
	check(source, all);
	// Both the native-copy path and the general representation conversion preserve unchanged color annotations.
	check(take(source.converted(SDL_PIXELFORMAT_BGRA32)), all);
	check(take(source.converted(SDL_PIXELFORMAT_RGBA128_FLOAT)), all);
	loader.format = SDL_PIXELFORMAT_INDEX8;
	auto palette = take(loader.load(bytes, {}, {}));
	check(take(palette.converted(SDL_PIXELFORMAT_RGBA32)), all);
	require(SDL_SetSurfaceColorKey(palette.handle(), true, 0), "Could not set indexed color key");
	check(take(palette.converted(SDL_PIXELFORMAT_RGBA128_FLOAT)), all);
	// Replacing only transfer or only primaries clears just that component, independently of earlier loading actions.
	auto transfer = take(source.converted(SDL_PIXELFORMAT_RGBA32,
		{.lossPolicy=fcg::PixelLossPolicy::Allow, .colorspace=SDL_COLORSPACE_SRGB}));
	check(transfer, {all.colorPrimaries, A::None, all.colorRange});
	require(SDL_SetSurfaceColorspace(transfer.handle(), SDL_COLORSPACE_SRGB_LINEAR), "Could not restore converted colorspace");
	check(transfer, {all.colorPrimaries, A::None, all.colorRange});
	const auto wide = static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL,
		SDL_COLOR_PRIMARIES_BT2020, SDL_TRANSFER_CHARACTERISTICS_LINEAR, SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE));
	auto gamut = take(source.converted(SDL_PIXELFORMAT_RGBA128_FLOAT,
		{.lossPolicy=fcg::PixelLossPolicy::Allow, .colorspace=wide}));
	check(gamut, {A::None, all.transferCharacteristics, all.colorRange});
	check(source, all);
	// Replacing every annotated component removes the complete record, including its stored comparison values.
	loader.format = SDL_PIXELFORMAT_RGBA32;
	loader.adjustments = {A::GuessedAndAdapted, A::Adapted};
	auto resetSource = take(loader.load(bytes, {}, {}));
	const auto wideSRGB = static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL,
		SDL_COLOR_PRIMARIES_BT2020, SDL_TRANSFER_CHARACTERISTICS_SRGB, SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE));
	auto reset = take(resetSource.converted(SDL_PIXELFORMAT_RGBA128_FLOAT,
		{.lossPolicy=fcg::PixelLossPolicy::Allow, .colorspace=wideSRGB}));
	check(reset, {});
	require(SDL_SetSurfaceColorspace(reset.handle(), SDL_COLORSPACE_SRGB_LINEAR), "Could not restore reset encoding");
	check(reset, {});
	// Raw SDL edits hide mismatches without changing their stored annotations, allowing the original values to restore them.
	const std::array changed{
		std::pair{wide, fcg::ImageLoadAdjustments{A::None, all.transferCharacteristics, all.colorRange}},
		std::pair{SDL_COLORSPACE_SRGB, fcg::ImageLoadAdjustments{all.colorPrimaries, A::None, all.colorRange}},
		std::pair{static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_LIMITED,
			SDL_COLOR_PRIMARIES_BT709, SDL_TRANSFER_CHARACTERISTICS_LINEAR, SDL_MATRIX_COEFFICIENTS_IDENTITY,
			SDL_CHROMA_LOCATION_NONE)), fcg::ImageLoadAdjustments{all.colorPrimaries, all.transferCharacteristics, A::None}}
	};
	for (const auto &[encoding, expected] : changed) {
		require(SDL_SetSurfaceColorspace(source.handle(), encoding), "Could not edit color metadata");
		check(source, expected);
		require(SDL_SetSurfaceColorspace(source.handle(), SDL_COLORSPACE_SRGB_LINEAR), "Could not restore color metadata");
		check(source, all);
	}
	// A copied output must not resurrect source annotations that were already suppressed before conversion.
	require(SDL_SetSurfaceColorspace(source.handle(), wide), "Could not edit source primaries");
	auto filtered = take(source.converted(SDL_PIXELFORMAT_BGRA32));
	require(SDL_SetSurfaceColorspace(filtered.handle(), SDL_COLORSPACE_SRGB_LINEAR), "Could not edit copy primaries");
	check(filtered, {A::None, all.transferCharacteristics, all.colorRange});
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
	// The backend owns its labels, even when it ignores the source flag; guessing must retain those labels.
	qoi.append("\xff\x80\x40\x20\xff", 5);
	qoi.append("\0\0\0\0\0\0\0\1", 8);
	fcg::SDLImageFormatHandler decoder;
	const auto bytes = std::as_bytes(std::span(qoi));
	auto ordinary = decoder.load(bytes, "png");
	auto guessed = decoder.loadAndGuess(bytes, "png");
	require(ordinary && guessed && SDL_GetSurfaceColorspace(ordinary->handle()) == SDL_GetSurfaceColorspace(guessed->handle()),
		"Guessing replaced SDL_image's established label");
	const auto info = guessed->pixelInfo();
	require(info && info->loadAdjustments.colorPrimaries == fcg::ImageLoadAdjustment::None
		&& info->loadAdjustments.transferCharacteristics == fcg::ImageLoadAdjustment::None
		&& info->loadAdjustments.colorRange == fcg::ImageLoadAdjustment::None, "SDL_image defaults were retrospectively annotated");
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
		loadAdjustmentLifecycleTests();
		backendGuessTests();
		uploadCapabilityTests();
		std::cout << "Format snapshots, registry order, codec capabilities, and viewer filters passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
