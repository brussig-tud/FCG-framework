
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// FCG Framework
#include <FCG/Image/image.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Keep correctness checks active in release builds.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

/// Unwrap a checked operation, reporting its actionable error on failure.
template<class T>
auto take (std::expected<T, fcg::ImageError> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	if constexpr (!std::is_void_v<T>)
		return std::move(*result);
}

/// Verify an expected error category, without dereferencing an unexpected success.
template<class T>
void fails (const std::expected<T, fcg::ImageError> &result, fcg::ImageErrorCode code) {
	require(!result && result.error().code == code, "Wrong error category");
}

/// Create owned headless pixel storage.
auto image (SDL_PixelFormat format, int width = 3, int height = 2) -> fcg::Image {
	return take(fcg::Image::adopt(SDL_CreateSurface(width, height, format)));
}

/// Whether a view exposes mutable pixel operations.
template<class View>
concept WritableView = requires(View view, typename View::Value value) { view.write(0, 0, value); };

/// Whether an image expression can lend an access session.
template<class Owner>
concept BorrowableImage = requires(Owner &&owner) { std::forward<Owner>(owner).template mapPixels<SDL_PIXELFORMAT_RGBA32>(); };

/// Whether a guard expression can lend a view.
template<class Guard>
concept BorrowableGuard = requires(Guard &&guard) { std::forward<Guard>(guard).view(); };

static_assert(WritableView<fcg::PixelView<SDL_PIXELFORMAT_RGBA32, true>>);
static_assert(!WritableView<fcg::PixelView<SDL_PIXELFORMAT_RGBA32, false>>);
static_assert(std::is_const_v<typename fcg::PixelView<SDL_PIXELFORMAT_RGBA32, false>::Byte>);
static_assert(BorrowableImage<fcg::Image&> && BorrowableImage<const fcg::Image&>);
static_assert(!BorrowableImage<fcg::Image> && !BorrowableImage<const fcg::Image>);
static_assert(BorrowableGuard<fcg::PixelAccessGuard<SDL_PIXELFORMAT_RGBA32, true>&>);
static_assert(!BorrowableGuard<fcg::PixelAccessGuard<SDL_PIXELFORMAT_RGBA32, true>>);
static_assert(!std::is_copy_constructible_v<fcg::PixelAccessGuard<SDL_PIXELFORMAT_RGBA32, true>>);
static_assert(std::forward_iterator<fcg::PixelView<SDL_PIXELFORMAT_RGBA32, true>::Iterator>);

/// Verify descriptions distinguish logical precision, physical storage and unavailable byte sizes.
void metadataTests ()
{
	const auto packed = take(fcg::pixelFormatInfo(SDL_PIXELFORMAT_XRGB1555));
	require(packed.channelBits == std::array<unsigned, 4>{5, 5, 5, 0}
		&& packed.bytesPerPixel == 2 && packed.unusedBits == 1 && !packed.directAccess, "Packed metadata");
	const auto ten = take(fcg::pixelFormatInfo(SDL_PIXELFORMAT_XRGB2101010));
	require(ten.channelBits[0] == 10 && ten.unusedBits == 2, "Ten-bit sample precision");
	const auto half = take(fcg::pixelFormatInfo(SDL_PIXELFORMAT_RGBA64_FLOAT));
	require(half.bytesPerPixel == 8 && half.channelBits[0] == 16
		&& half.representation == fcg::PixelRepresentation::Binary16 && half.directAccess, "Half metadata");
	const auto bitmap = take(fcg::pixelFormatInfo(SDL_PIXELFORMAT_INDEX2MSB));
	require(!bitmap.bytesPerPixel && bitmap.bitsPerPixel == 2 && bitmap.hasAlpha, "Sub-byte metadata");
	const auto index = take(fcg::pixelFormatInfo(SDL_PIXELFORMAT_INDEX8));
	require(index.bytesPerPixel == 1 && !index.directAccess, "Indexed metadata");
	for (auto format : {SDL_PIXELFORMAT_IYUV, SDL_PIXELFORMAT_NV12, SDL_PIXELFORMAT_P010, SDL_PIXELFORMAT_YUY2}) {
		const auto description = take(fcg::pixelFormatInfo(format));
		require(description.storage == fcg::PixelStorage::YUV && !description.bytesPerPixel && !description.bitsPerPixel,
			"YUV must not claim ordinary pixel byte size");
	}
	require(take(fcg::pixelFormatInfo(SDL_PIXELFORMAT_MJPG)).storage == fcg::PixelStorage::Compressed, "Compressed metadata");
	require(take(fcg::pixelFormatInfo(SDL_PIXELFORMAT_EXTERNAL_OES)).storage == fcg::PixelStorage::External, "External metadata");
	fails(fcg::pixelFormatInfo(SDL_PIXELFORMAT_UNKNOWN), fcg::ImageErrorCode::UnsupportedFormat);
	fails(fcg::pixelFormatInfo(SDL_PixelFormat(0xdeadbeef)), fcg::ImageErrorCode::UnsupportedFormat);
	const auto alias = take(fcg::pixelFormatInfo(SDL_PIXELFORMAT_RGBA32));
	require(alias.format == (SDL_BYTEORDER == SDL_LIL_ENDIAN ? SDL_PIXELFORMAT_ABGR8888 : SDL_PIXELFORMAT_RGBA8888), "Endian alias");
	auto owned = image(SDL_PIXELFORMAT_RGBA32);
	require(SDL_SetSurfaceColorspace(owned.handle(), SDL_COLORSPACE_SRGB_LINEAR), "Set colorspace");
	require(SDL_SetFloatProperty(SDL_GetSurfaceProperties(owned.handle()), SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, 4.f), "Set HDR metadata");
	auto snapshot = take(owned.pixelInfo());
	require(snapshot.colorspace == SDL_COLORSPACE_SRGB_LINEAR && snapshot.hdrHeadroom == 4.f && !snapshot.sdrWhitePoint,
		"Surface metadata snapshot");
	require(SDL_SetSurfaceColorKey(owned.handle(), true, 42), "Set key");
	require(!snapshot.colorKey && take(owned.pixelInfo()).colorKey == 42, "Metadata must be independent and current");
}

/// Verify explicit loss approval, independent output, source preservation and HDR range.
void conversionTests ()
{
	auto source = image(SDL_PIXELFORMAT_RGBA64);
	take(source.writePixel<SDL_PIXELFORMAT_RGBA64>(0, 0, {10000, 20000, 30000, 40000}));
	const auto report = take(source.conversionInfo(SDL_PIXELFORMAT_RGBA32));
	require(report.supported && !report.permitted && report.precisionReduction && report.allocatesStorage, "Narrowing analysis");
	fails(source.converted(SDL_PIXELFORMAT_RGBA32), fcg::ImageErrorCode::LossyConversion);
	fcg::PixelConversionOptions allow;
	allow.lossPolicy = fcg::PixelLossPolicy::Allow;
	auto converted = take(source.converted(SDL_PIXELFORMAT_RGBA32, allow));
	require(converted.handle()->pixels != source.handle()->pixels && take(source.readPixel<SDL_PIXELFORMAT_RGBA64>(0, 0)).r == 10000,
		"Converted output must not change source");
	take(converted.writePixel<SDL_PIXELFORMAT_RGBA32>(0, 0, {0, 0, 0, 0}));
	require(take(source.readPixel<SDL_PIXELFORMAT_RGBA64>(0, 0)).r == 10000, "Output storage must be independent");
	auto rgb = image(SDL_PIXELFORMAT_RGB24);
	take(rgb.writePixel<SDL_PIXELFORMAT_RGB24>(0, 0, {1, 2, 3}));
	const auto add = take(rgb.conversionInfo(SDL_PIXELFORMAT_RGBA32));
	require(add.permitted && add.alphaAddition && add.channelOrderChange, "Opaque alpha addition");
	auto rgba = take(rgb.converted(SDL_PIXELFORMAT_RGBA32));
	require(take(rgba.readPixel<SDL_PIXELFORMAT_RGBA32>(0, 0)).a == 255, "Added alpha is opaque");
	require(take(rgba.conversionInfo(SDL_PIXELFORMAT_RGB24)).alphaRemoval, "Removed alpha report");
	fails(rgba.converted(SDL_PIXELFORMAT_RGB24), fcg::ImageErrorCode::LossyConversion);
	require(!take(rgb.conversionInfo(SDL_PIXELFORMAT_INDEX8)).supported, "Indexed output unsupported");
	fails(rgb.converted(SDL_PIXELFORMAT_INDEX8, allow), fcg::ImageErrorCode::UnsupportedFormat);
	allow.colorspace = SDL_COLORSPACE_SRGB_LINEAR;
	const auto encoding = take(rgb.conversionInfo(SDL_PIXELFORMAT_RGBA128_FLOAT, allow));
	require(encoding.colorEncodingChange && encoding.colorTransformLoss && encoding.permitted, "Encoding loss analysis");
	fcg::PixelConversionOptions rejectEncoding;
	rejectEncoding.colorspace = SDL_COLORSPACE_SRGB_LINEAR;
	fails(rgb.converted(SDL_PIXELFORMAT_RGBA128_FLOAT, rejectEncoding), fcg::ImageErrorCode::LossyConversion);
	auto floating = image(SDL_PIXELFORMAT_RGBA128_FLOAT);
	take(floating.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(0, 0, {5.f, 2.f, -1.f, 0.75f}));
	require(SDL_SetFloatProperty(SDL_GetSurfaceProperties(floating.handle()), SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, 8.f), "Set HDR");
	require(SDL_SetStringProperty(SDL_GetSurfaceProperties(floating.handle()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "chrome"), "Set tone mapping");
	require(SDL_SetFloatProperty(SDL_GetSurfaceProperties(floating.handle()), SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, 2.f), "Set explicit white point");
	const auto narrow = take(floating.conversionInfo(SDL_PIXELFORMAT_RGBA64_FLOAT));
	require(narrow.precisionReduction && narrow.rangeClipping && !narrow.permitted, "Float narrowing reports precision and range");
	allow.colorspace.reset();
	auto reordered = take(floating.converted(SDL_PIXELFORMAT_BGRA128_FLOAT));
	const auto hdr = take(reordered.readPixel<SDL_PIXELFORMAT_BGRA128_FLOAT>(0, 0));
	require(hdr.r == 5.f && hdr.g == 2.f && hdr.b == -1.f && hdr.a == 0.75f, "HDR survives reorder without tone mapping");
	require(std::string(SDL_GetStringProperty(SDL_GetSurfaceProperties(floating.handle()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "")) == "chrome",
		"Source tone mapping property changed");
	auto halfOutput = take(floating.converted(SDL_PIXELFORMAT_RGBA64_FLOAT, allow));
	require(float(take(halfOutput.readPixel<SDL_PIXELFORMAT_RGBA64_FLOAT>(0, 0)).r) == 5.f, "HDR survives half output");
	require(take(halfOutput.pixelInfo()).sdrWhitePoint == 2.f, "Conversion white-point metadata changed");
	auto yuv = image(SDL_PIXELFORMAT_NV12, 4, 2);
	std::memset(yuv.handle()->pixels, 128, std::size_t(yuv.pitch()) * 3);
	require(!take(yuv.conversionInfo(SDL_PIXELFORMAT_RGBA32)).supported, "YUV needs explicit encoding");
	allow.colorspace = SDL_COLORSPACE_SRGB;
	require(take(yuv.conversionInfo(SDL_PIXELFORMAT_RGBA32, allow)).permitted, "YUV opt-in analysis");
	auto yuvRgb = take(yuv.converted(SDL_PIXELFORMAT_RGBA32, allow));
	require(yuvRgb.format() == SDL_PIXELFORMAT_RGBA32, "YUV conversion");
	fcg::PixelConversionOptions invalid;
	invalid.lossPolicy = fcg::PixelLossPolicy(200);
	fails(rgb.conversionInfo(SDL_PIXELFORMAT_RGBA32, invalid), fcg::ImageErrorCode::InvalidArgument);
}

/// Verify keys use original sample identity and palette alpha remains independent from key metadata.
void keyAndPaletteTests ()
{
	for (auto format : {SDL_PIXELFORMAT_INDEX1LSB, SDL_PIXELFORMAT_INDEX1MSB,
		SDL_PIXELFORMAT_INDEX2LSB, SDL_PIXELFORMAT_INDEX2MSB, SDL_PIXELFORMAT_INDEX4LSB,
		SDL_PIXELFORMAT_INDEX4MSB, SDL_PIXELFORMAT_INDEX8})
	{
		auto source = image(format, 5, 1);
		const int bits = SDL_BITSPERPIXEL(format);
		const SDL_Color paletteColors[]{{21, 43, 65, 255}, {21, 43, 65, 255}};
		auto *palette = SDL_CreateSurfacePalette(source.handle());
		require(palette && SDL_SetPaletteColors(palette, paletteColors, 0, 2), "Create palette");
		std::memset(source.handle()->pixels, 0, source.pitch());
		for (int x = 0; x < 5; ++x) {
			const int shift = SDL_PIXELORDER(format) == SDL_BITMAPORDER_4321 ? (x * bits) % 8 : 8 - bits - (x * bits) % 8;
			static_cast<Uint8*>(source.handle()->pixels)[x * bits / 8] |= (x % 2) << shift;
		}
		require(SDL_SetSurfaceColorKey(source.handle(), true, 1), "Set indexed key");
		for (int x = 0; x < 5; ++x) {
			const auto color = take(source.readColor8(x, 0));
			require(color.r == 21 && color.g == 43 && color.b == 65 && color.a == 255, "Indexed read and packing");
		}
		const auto report = take(source.conversionInfo(SDL_PIXELFORMAT_RGBA32));
		require(report.paletteExpansion && report.colorKeyBaking && report.permitted, "Palette expansion and key bake are lossless");
		for (auto target : {SDL_PIXELFORMAT_RGBA32, SDL_PIXELFORMAT_ARGB64, SDL_PIXELFORMAT_BGRA64_FLOAT, SDL_PIXELFORMAT_ABGR128_FLOAT}) {
			auto output = take(source.converted(target));
			for (int x = 0; x < 5; ++x)
				require(take(output.readColor8(x, 0)).a == (x % 2 ? 0 : 255), "Key identity across every precision");
		}
		require(take(source.pixelInfo()).colorKey == 1 && palette->colors[1].a == 255, "Source key or palette modified");
		fails(source.converted(SDL_PIXELFORMAT_RGB24), fcg::ImageErrorCode::InvalidArgument);
		fcg::PixelConversionOptions discard;
		discard.colorKeyPolicy = fcg::PixelColorKeyPolicy::Discard;
		fails(source.converted(SDL_PIXELFORMAT_RGB24, discard), fcg::ImageErrorCode::LossyConversion);
		discard.lossPolicy = fcg::PixelLossPolicy::Allow;
		auto discarded = take(source.converted(SDL_PIXELFORMAT_RGB24, discard));
		require(!SDL_SurfaceHasColorKey(discarded.handle()), "Discard retains a key");
		fails(source.writeColor8(0, 0, {1, 2, 3, 255}, fcg::PixelLossPolicy::Allow), fcg::ImageErrorCode::UnsupportedFormat);
		palette->ncolors = 1;
		fails(source.readColor8(1, 0), fcg::ImageErrorCode::InvalidState);
		palette->ncolors = 0;
		fails(source.readColorFloat(0, 0), fcg::ImageErrorCode::InvalidState);
		palette->ncolors = 1 << bits;
	}
	auto transparent = image(SDL_PIXELFORMAT_INDEX8, 1, 1);
	auto *transparentPalette = SDL_CreatePalette(1);
	const SDL_Color transparentEntry{100, 120, 140, 0};
	require(transparentPalette && SDL_SetPaletteColors(transparentPalette, &transparentEntry, 0, 1)
		&& SDL_SetSurfacePalette(transparent.handle(), transparentPalette), "Attach all-transparent palette");
	SDL_DestroyPalette(transparentPalette);
	*static_cast<Uint8*>(transparent.handle()->pixels) = 0;
	auto transparentOutput = take(transparent.converted(SDL_PIXELFORMAT_RGBA32));
	require(take(transparentOutput.readColor8(0, 0)).a == 0, "Palette expansion lost alpha from an all-transparent palette");
	*static_cast<Uint8*>(transparent.handle()->pixels) = 1;
	fails(transparent.converted(SDL_PIXELFORMAT_RGBA32), fcg::ImageErrorCode::InvalidState);
	auto keyed = image(SDL_PIXELFORMAT_ARGB2101010);
	require(SDL_SetSurfaceColorspace(keyed.handle(), SDL_COLORSPACE_SRGB), "Set packed encoding");
	const auto *details = SDL_GetPixelFormatDetails(keyed.format());
	const Uint32 key = (123u << details->Rshift) | (456u << details->Gshift) | 789u | details->Amask;
	std::memcpy(keyed.handle()->pixels, &key, 4);
	require(SDL_SetSurfaceColorKey(keyed.handle(), true, key), "Set high precision key");
	auto output = take(keyed.converted(SDL_PIXELFORMAT_RGBA128_FLOAT));
	require(take(output.readColorFloat(0, 0)).a == 0.f, "Packed key matched exactly");
}

/// Create distinctive native sample values without reducing native precision.
template<class Component>
auto sample (int component) -> Component
{
	if constexpr (std::is_same_v<Component, fcg::Half>)
		return fcg::Half::fromBits(Uint16(0x8001 + component * 0x100));
	else if constexpr (std::is_same_v<Component, float>)
		return std::bit_cast<float>(0x3f800001u + unsigned(component) * 71);
	else
		return Component(std::is_same_v<Component, Uint16> ? 12345 + component * 5432 : 21 + component * 31);
}

/// Verify every supported layout in deliberately unaligned padded external storage.
template<SDL_PixelFormat Format>
void nativeRoundTrip ()
{
	using Traits = fcg::PixelFormatTraits<Format>;
	using Component = typename Traits::Component;
	constexpr int width = 3;
	constexpr int height = 2;
	constexpr int rowBytes = width * Traits::bytesPerPixel;
	constexpr int pitch = rowBytes + 7;
	std::vector<std::byte> storage(1 + height * pitch + 1, std::byte{0x5a});
	auto owned = take(fcg::Image::adopt(SDL_CreateSurfaceFrom(width, height, Format, storage.data() + 1, pitch)));
	typename Traits::Value value{sample<Component>(0), sample<Component>(1), sample<Component>(2)};
	if constexpr (Traits::hasAlpha)
		value.a = sample<Component>(3);
	take(owned.writePixel<Format>(1, 1, value));
	const auto result = take(owned.readPixel<Format>(1, 1));
	require(std::memcmp(&result.r, &value.r, sizeof(Component)) == 0
		&& std::memcmp(&result.g, &value.g, sizeof(Component)) == 0
		&& std::memcmp(&result.b, &value.b, sizeof(Component)) == 0, "Native RGB precision changed");
	if constexpr (Traits::hasAlpha)
		require(std::memcmp(&result.a, &value.a, sizeof(Component)) == 0, "Native alpha precision changed");
	take(owned.withPixels<Format>([value](auto view) {
		for (auto row : view) {
			require(row.size() == width && row.bytes().size() == rowBytes, "Row includes padding");
			for (int x = 0; x < row.size(); ++x)
				row.writeUnchecked(x, value);
		}
		fails(view.read(-1, 0), fcg::ImageErrorCode::InvalidArgument);
		fails(view.row(height), fcg::ImageErrorCode::InvalidArgument);
		fails(view.write(width, 0, value), fcg::ImageErrorCode::InvalidArgument);
	}));
	for (int y = 0; y < height; ++y)
		for (int b = rowBytes; b < pitch; ++b)
			require(storage[1 + y * pitch + b] == std::byte{0x5a}, "Padding overwritten");
	require(storage.front() == std::byte{0x5a} && storage.back() == std::byte{0x5a}, "External storage sentinel overwritten");
	if constexpr (!Traits::hasAlpha && Traits::bytesPerPixel == 4)
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x)
				for (int component = 0; component < 4; ++component)
					if (std::find(Traits::offsets.begin(), Traits::offsets.end(), component) == Traits::offsets.end())
						require(storage[1 + y * pitch + x * 4 + component] == std::byte{0x5a}, "Unused byte overwritten");
}

/// Verify exact binary16 bits for every encoding, including signed zero, infinity and NaN payloads.
void halfBitsTests ()
{
	auto owned = image(SDL_PIXELFORMAT_RGBA64_FLOAT, 1, 1);
	auto guard = take(owned.mapPixels<SDL_PIXELFORMAT_RGBA64_FLOAT>());
	auto row = take(guard.view()).rowUnchecked(0);
	for (unsigned bits = 0; bits <= 65535; ++bits) {
		const auto value = fcg::Half::fromBits(Uint16(bits));
		row.writeUnchecked(0, {value, value, value, value});
		const auto read = row.readUnchecked(0);
		require(read.r.bits() == bits && read.a.bits() == bits, "Binary16 bits changed");
	}
	require(float(fcg::Half(1.5f)) == 1.5f && fcg::Half(-0.f).bits() == 0x8000, "Half conversion");
}

/// Verify native-order aliases independently from the format traits.
void endianTests ()
{
	auto owned = image(SDL_PIXELFORMAT_RGBA32, 1, 1);
	take(owned.writePixel<SDL_PIXELFORMAT_RGBA32>(0, 0, {1, 2, 3, 4}));
	const Uint8 expected[]{1, 2, 3, 4};
	require(std::memcmp(owned.handle()->pixels, expected, 4) == 0, "RGBA32 byte order");
	auto bgra = image(SDL_PIXELFORMAT_BGRA32, 1, 1);
	take(bgra.writePixel<SDL_PIXELFORMAT_BGRA32>(0, 0, {1, 2, 3, 4}));
	const Uint8 reverse[]{3, 2, 1, 4};
	require(std::memcmp(bgra.handle()->pixels, reverse, 4) == 0, "BGRA32 byte order");
}

/// Verify normalized color operations preserve encoding, loss policy and unused packed bits.
void colorTests ()
{
	auto rgba = image(SDL_PIXELFORMAT_RGBA32);
	take(rgba.writeColor8(0, 0, {12, 34, 56, 78}));
	const auto color = take(rgba.readColor8(0, 0));
	require(color.r == 12 && color.g == 34 && color.b == 56 && color.a == 78, "Byte color roundtrip");
	fails(rgba.writeColorFloat(0, 0, {0.1f, 0.2f, 0.3f, 1}), fcg::ImageErrorCode::LossyConversion);
	take(rgba.writeColorFloat(0, 0, {-1, 2, 0.5f, 1}, fcg::PixelLossPolicy::Allow));
	const auto clipped = take(rgba.readColor8(0, 0));
	require(clipped.r == 0 && clipped.g == 255 && clipped.b == 128, "Clamp and quantize");
	fails(rgba.writeColorFloat(0, 0, {std::numeric_limits<float>::infinity(), 0, 0, 1}, fcg::PixelLossPolicy::Allow),
		fcg::ImageErrorCode::InvalidArgument);
	auto packed = image(SDL_PIXELFORMAT_XRGB1555);
	const Uint16 sentinel = 0x8000;
	std::memcpy(packed.handle()->pixels, &sentinel, 2);
	fails(packed.writeColor8(0, 0, {123, 45, 67, 255}), fcg::ImageErrorCode::LossyConversion);
	take(packed.writeColor8(0, 0, {255, 0, 0, 255}, fcg::PixelLossPolicy::Allow));
	require((static_cast<Uint16*>(packed.handle()->pixels)[0] & 0x8000) != 0, "Unused packed bit lost");
	require(take(packed.readColorFloat(0, 0)).r == 1.f && take(packed.readColor8(0, 0)).a == 255, "Packed read");
	auto ten = image(SDL_PIXELFORMAT_ARGB2101010);
	take(ten.writeColorFloat(0, 0, {0.501f, 0, 0, 1}, fcg::PixelLossPolicy::Allow));
	require(std::abs(take(ten.readColorFloat(0, 0)).r - 0.501f) < 0.0005f, "Packed 10-bit read lost precision");
	auto floating = image(SDL_PIXELFORMAT_RGBA128_FLOAT);
	take(floating.writeColorFloat(0, 0, {0.5f, 4.f, -2.f, 0.75f}));
	const auto native = take(floating.readColorFloat(0, 0));
	require(native.r == 0.5f && native.g == 4.f && native.b == -2.f, "Float color encoding or HDR range changed");
	require(take(floating.readColor8(0, 0)).r == 128, "Read must preserve linear encoding");
	auto rgb = image(SDL_PIXELFORMAT_RGB24);
	fails(rgb.writeColor8(0, 0, {1, 2, 3, 255}), fcg::ImageErrorCode::LossyConversion);
	take(rgb.writeColor8(0, 0, {1, 2, 3, 255}, fcg::PixelLossPolicy::Allow));
	require(take(rgb.readColor8(0, 0)).a == 255, "Missing alpha must be opaque");
	auto yuv = image(SDL_PIXELFORMAT_NV12, 4, 2);
	fails(yuv.readColorFloat(0, 0), fcg::ImageErrorCode::UnsupportedFormat);
	fails(yuv.writeColorFloat(0, 0, {0, 0, 0, 1}, fcg::PixelLossPolicy::Allow), fcg::ImageErrorCode::UnsupportedFormat);
	fails(rgba.readColor8(-1, 0), fcg::ImageErrorCode::InvalidArgument);
	fails(rgba.writeColor8(0, rgba.height(), {0, 0, 0, 0}), fcg::ImageErrorCode::InvalidArgument);
	fails(rgba.readPixel<SDL_PIXELFORMAT_RGB24>(0, 0), fcg::ImageErrorCode::UnsupportedFormat);
}

/// Verify guard lifetime, lock release, conflicts, error propagation and malformed storage acquisition.
void guardTests ()
{
	auto owned = image(SDL_PIXELFORMAT_RGBA32);
	auto guard = take(owned.mapPixels<SDL_PIXELFORMAT_RGBA32>());
	fails(owned.mapPixels<SDL_PIXELFORMAT_RGBA32>(), fcg::ImageErrorCode::InvalidState);
	fails(owned.readColor8(0, 0), fcg::ImageErrorCode::InvalidState);
	fails(owned.writePixel<SDL_PIXELFORMAT_RGBA32>(0, 0, {1, 2, 3, 4}), fcg::ImageErrorCode::InvalidState);
	fails(owned.converted(SDL_PIXELFORMAT_RGBA32), fcg::ImageErrorCode::InvalidState);
	require(bool(owned.pixelInfo()) && bool(owned.conversionInfo(SDL_PIXELFORMAT_RGBA32)), "Metadata unavailable during access");
	auto moved = std::move(guard);
	fails(guard.view(), fcg::ImageErrorCode::InvalidState);
	require(bool(moved.view()), "Guard move lost access");
	moved.release();
	moved.release();
	fails(moved.view(), fcg::ImageErrorCode::InvalidState);
	take(owned.writeColor8(0, 0, {1, 2, 3, 4}));
	const auto callbackError = owned.withPixels<SDL_PIXELFORMAT_RGBA32>([](auto) -> std::expected<void, fcg::ImageError> {
		return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::IOFailure, "callback sentinel"});
	});
	fails(callbackError, fcg::ImageErrorCode::IOFailure);
	require(callbackError.error().message == "callback sentinel", "Callback error changed");
	try {
		(void)owned.withPixels<SDL_PIXELFORMAT_RGBA32>([](auto) { throw std::runtime_error("callback exception"); });
		require(false, "Exception swallowed");
	} catch (const std::runtime_error &error) {
		require(std::string(error.what()) == "callback exception", "Exception changed");
	}
	take(std::as_const(owned).withPixels<SDL_PIXELFORMAT_RGBA32>([](auto view) {
		require(view.readUnchecked(0, 0).r == 1, "Read-only callback");
	}));
	const int pitch = owned.handle()->pitch;
	owned.handle()->pitch = 1;
	fails(owned.mapPixels<SDL_PIXELFORMAT_RGBA32>(), fcg::ImageErrorCode::InvalidState);
	owned.handle()->pitch = pitch;
	const auto pixels = owned.handle()->pixels;
	owned.handle()->pixels = nullptr;
	fails(owned.mapPixels<SDL_PIXELFORMAT_RGBA32>(), fcg::ImageErrorCode::InvalidState);
	owned.handle()->pixels = pixels;
	auto other = image(SDL_PIXELFORMAT_RGBA32);
	auto first = take(owned.mapPixels<SDL_PIXELFORMAT_RGBA32>());
	auto second = take(other.mapPixels<SDL_PIXELFORMAT_RGBA32>());
	first = std::move(second);
	take(owned.writeColor8(0, 0, {0, 0, 0, 0}));
	fails(other.readColor8(0, 0), fcg::ImageErrorCode::InvalidState);
	first.release();
	const auto movedImage = std::move(other);
	fails(other.pixelInfo(), fcg::ImageErrorCode::InvalidState);
	fails(other.mapPixels<SDL_PIXELFORMAT_RGBA32>(), fcg::ImageErrorCode::InvalidState);
	fails(other.converted(SDL_PIXELFORMAT_RGBA32), fcg::ImageErrorCode::InvalidState);
}

/// Verify pointwise and neighborhood traversal, dimension and storage checks, and partial-output exceptions.
void kernelTests ()
{
	auto source = image(SDL_PIXELFORMAT_RGBA32);
	take(source.withPixels<SDL_PIXELFORMAT_RGBA32>([](auto view) {
		for (int y = 0; y < view.height(); ++y)
			for (int x = 0; x < view.width(); ++x)
				view.writeUnchecked(x, y, {Uint8(x * 30), 0, 0, 255});
	}));
	take(source.transformPixels<SDL_PIXELFORMAT_RGBA32>([](auto value) { ++value.r; return value; }));
	auto destination = image(SDL_PIXELFORMAT_RGBA32);
	take(source.transformPixelsTo<SDL_PIXELFORMAT_RGBA32>(destination, [](int x, int y, auto input) {
		return input.readUnchecked(std::max(0, x - 1), y);
	}));
	require(take(destination.readPixel<SDL_PIXELFORMAT_RGBA32>(2, 0)).r == 31, "Neighborhood traversal or border");
	auto mismatch = image(SDL_PIXELFORMAT_RGBA32, 2, 2);
	const auto identity = [](int x, int y, auto input) { return input.readUnchecked(x, y); };
	fails(source.transformPixelsTo<SDL_PIXELFORMAT_RGBA32>(mismatch, identity), fcg::ImageErrorCode::InvalidArgument);
	fails(source.transformPixelsTo<SDL_PIXELFORMAT_RGBA32>(source, identity), fcg::ImageErrorCode::InvalidArgument);
	std::vector<std::byte> shared(100);
	auto aliasA = take(fcg::Image::adopt(SDL_CreateSurfaceFrom(3, 2, SDL_PIXELFORMAT_RGBA32, shared.data(), 16)));
	auto aliasB = take(fcg::Image::adopt(SDL_CreateSurfaceFrom(3, 2, SDL_PIXELFORMAT_RGBA32, shared.data() + 4, 16)));
	fails(aliasA.transformPixelsTo<SDL_PIXELFORMAT_RGBA32>(aliasB, identity), fcg::ImageErrorCode::InvalidArgument);
	int calls = 0;
	try {
		(void)source.transformPixels<SDL_PIXELFORMAT_RGBA32>([&calls](auto value) {
			if (++calls == 3)
				throw std::runtime_error("kernel exception");
			value.r = 200;
			return value;
		});
		require(false, "Kernel exception swallowed");
	} catch (const std::runtime_error &error) {
		require(std::string(error.what()) == "kernel exception", "Kernel exception changed");
	}
	require(take(source.readPixel<SDL_PIXELFORMAT_RGBA32>(0, 0)).r == 200
		&& take(source.readPixel<SDL_PIXELFORMAT_RGBA32>(2, 0)).r == 61, "Partial output guarantee");
	calls = 0;
	try {
		(void)source.transformPixelsTo<SDL_PIXELFORMAT_RGBA32>(destination, [&calls](int x, int y, auto input) {
			if (++calls == 2)
				throw std::runtime_error("neighborhood exception");
			return input.readUnchecked(x, y);
		});
	} catch (const std::runtime_error&) {}
	require(bool(source.readColor8(0, 0)) && bool(destination.readColor8(0, 0)), "Neighborhood exception leaked access");
}

/// Verify required locks are acquired once and released after values, callbacks, moves and exceptions.
void lockingTests ()
{
	auto owned = image(SDL_PIXELFORMAT_RGBA32);
	// Model a lock-required surface without requesting RLE compression; SDL maintains its actual lock state.
	owned.handle()->flags |= SDL_SURFACE_LOCK_NEEDED;
	{
		auto guard = take(owned.mapPixels<SDL_PIXELFORMAT_RGBA32>());
		require((owned.handle()->flags & SDL_SURFACE_LOCKED) != 0, "Required lock was not acquired");
		auto moved = std::move(guard);
		moved.release();
		require((owned.handle()->flags & SDL_SURFACE_LOCKED) == 0, "Moved guard leaked lock");
	}
	take(owned.writeColor8(0, 0, {1, 2, 3, 4}));
	require((owned.handle()->flags & SDL_SURFACE_LOCKED) == 0, "Single-pixel write leaked lock");
	try {
		(void)owned.withPixels<SDL_PIXELFORMAT_RGBA32>([](auto) { throw std::runtime_error("locked callback"); });
	} catch (const std::runtime_error&) {}
	require((owned.handle()->flags & SDL_SURFACE_LOCKED) == 0, "Callback exception leaked lock");
	owned.handle()->flags &= ~SDL_SURFACE_LOCK_NEEDED;
	// Real SDL RLE encoding may release the CPU pixel pointer before the access lock recreates it.
	take(owned.writeColor8(0, 0, {40, 50, 60, 255}));
	require(SDL_SetSurfaceColorKey(owned.handle(), true, 0) && SDL_SetSurfaceRLE(owned.handle(), true), "Enable RLE");
	auto output = image(SDL_PIXELFORMAT_RGBA32);
	require(SDL_BlitSurface(owned.handle(), nullptr, output.handle(), nullptr), "Encode RLE surface");
	require(SDL_MUSTLOCK(owned.handle()), "RLE did not require locking");
	require(take(owned.readPixel<SDL_PIXELFORMAT_RGBA32>(0, 0)).r == 40, "RLE read");
	require((owned.handle()->flags & SDL_SURFACE_LOCKED) == 0, "RLE read leaked lock");
}

/// Verify full native conversion retains bits even with non-linear encoding, including NaN payloads.
void exactConversionTests ()
{
	auto source = image(SDL_PIXELFORMAT_RGBA128_FLOAT);
	require(SDL_SetSurfaceColorspace(source.handle(), SDL_COLORSPACE_SRGB), "Set nonlinear float encoding");
	const fcg::NativeRGBA<float> value{std::bit_cast<float>(0x3eabcdefu), std::bit_cast<float>(0x7fc01234u), -0.f, 1.f};
	take(source.writePixel<SDL_PIXELFORMAT_RGBA128_FLOAT>(0, 0, value));
	auto output = take(source.converted(SDL_PIXELFORMAT_ARGB128_FLOAT));
	const auto result = take(output.readPixel<SDL_PIXELFORMAT_ARGB128_FLOAT>(0, 0));
	require(std::bit_cast<Uint32>(result.r) == std::bit_cast<Uint32>(value.r)
		&& std::bit_cast<Uint32>(result.g) == std::bit_cast<Uint32>(value.g)
		&& std::bit_cast<Uint32>(result.b) == std::bit_cast<Uint32>(value.b), "Reorder changed nonlinear native bits");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Headless pixel API verification entry point.
auto main () -> int
{
	try {
		metadataTests();
		conversionTests();
		keyAndPaletteTests();
		nativeRoundTrip<SDL_PIXELFORMAT_RGB24>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGR24>();
		nativeRoundTrip<SDL_PIXELFORMAT_XRGB8888>();
		nativeRoundTrip<SDL_PIXELFORMAT_RGBX8888>();
		nativeRoundTrip<SDL_PIXELFORMAT_XBGR8888>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGRX8888>();
		nativeRoundTrip<SDL_PIXELFORMAT_ARGB8888>();
		nativeRoundTrip<SDL_PIXELFORMAT_RGBA8888>();
		nativeRoundTrip<SDL_PIXELFORMAT_ABGR8888>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGRA8888>();
		nativeRoundTrip<SDL_PIXELFORMAT_RGB48>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGR48>();
		nativeRoundTrip<SDL_PIXELFORMAT_RGBA64>();
		nativeRoundTrip<SDL_PIXELFORMAT_ARGB64>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGRA64>();
		nativeRoundTrip<SDL_PIXELFORMAT_ABGR64>();
		nativeRoundTrip<SDL_PIXELFORMAT_RGB48_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGR48_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_RGBA64_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_ARGB64_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGRA64_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_ABGR64_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_RGB96_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGR96_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_RGBA128_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_ARGB128_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_BGRA128_FLOAT>();
		nativeRoundTrip<SDL_PIXELFORMAT_ABGR128_FLOAT>();
		halfBitsTests();
		endianTests();
		colorTests();
		guardTests();
		lockingTests();
		exactConversionTests();
		kernelTests();
		std::cout << "Pixel metadata, explicit conversion, native access and kernels passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
