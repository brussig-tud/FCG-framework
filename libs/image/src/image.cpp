
//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>
#include <memory>
#include <limits>

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "FCG/Image/image.h"
#include "image_internal.h"
#include "image_upload_internal.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// \brief Copy an indexed surface and its palette into independent conversion storage.
///
/// SDL's surface duplication cannot blit these formats to themselves. Copy their packed rows and palette directly;
/// the upload path reads the color key from the original and applies it after conversion.
///
/// \param source The indexed surface to copy, with a valid attached palette.
///
/// \return An owned surface to destroy with \c SDL_DestroySurface, or \c nullptr with an SDL error on failure.
[[nodiscard]] auto copyIndexedSurface (SDL_Surface &source) -> SDL_Surface*
{
	const bool lock = SDL_MUSTLOCK(&source);
	if (lock && !SDL_LockSurface(&source))
		return nullptr;
	const std::unique_ptr<SDL_Surface, decltype(&SDL_UnlockSurface)> unlock(
		lock ? &source : nullptr, SDL_UnlockSurface
	);
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> copy(
		SDL_CreateSurface(source.w, source.h, source.format), SDL_DestroySurface
	);
	if (!copy)
		return nullptr;
	auto *sourcePalette = SDL_GetSurfacePalette(&source);
	if (!sourcePalette) {
		SDL_SetError("Indexed image has no palette");
		return nullptr;
	}
	const std::unique_ptr<SDL_Palette, decltype(&SDL_DestroyPalette)> palette(
		SDL_CreatePalette(sourcePalette->ncolors), SDL_DestroyPalette
	);
	if (!palette || !SDL_SetPaletteColors(palette.get(), sourcePalette->colors, 0, sourcePalette->ncolors)
		|| !SDL_SetSurfacePalette(copy.get(), palette.get())
		|| !SDL_SetSurfaceColorspace(copy.get(), SDL_GetSurfaceColorspace(&source))
		|| !SDL_CopyProperties(SDL_GetSurfaceProperties(&source), SDL_GetSurfaceProperties(copy.get())))
		return nullptr;
	const auto rowBytes = (std::size_t(source.w) * SDL_BITSPERPIXEL(source.format) + 7) / 8;
	for (int y = 0; y < source.h; ++y)
		std::memcpy(
			static_cast<Uint8*>(copy->pixels) + std::size_t(y) * copy->pitch,
			static_cast<const Uint8*>(source.pixels) + std::size_t(y) * source.pitch, rowBytes
		);
	return copy.release();
}

/// \brief
/// 	Sets alpha to zero in \c destination for every pixel whose color in \c source matches the color key.
///
/// The key is compared in the original (indexed or packed) representation of \c source, so palette indices and
/// high-bit RGB values are matched exactly. \c source must be an independent duplicate of the image with its key
/// disabled, and \c destination its converted counterpart in \c RGBA32, \c BGRA32 or \c RGBA128_FLOAT format.
///
/// \param source
/// 	The unconverted pixels, with at most 32 bits per pixel.
/// \param destination
/// 	The converted pixels, of the same size as \c source.
/// \param key
/// 	The color key value of the original image.
///
/// \return Nothing on success, or \c fcg::ImageErrorCode::SDLFailure if \c source could not be locked.
auto bakeColorKey (SDL_Surface &source, SDL_Surface &destination, Uint32 key) -> std::expected<void, fcg::ImageError>
{
	// Bake keys using original representations, preserving palette-index identity and high-bit RGB precision.
	// SDL's packed color-key conversion is limited to 32-bit destinations; floating-point expansion must
	// apply alpha separately after color conversion.
	const bool copyLock = SDL_MUSTLOCK(&source);
	if (copyLock && !SDL_LockSurface(&source))
		return std::unexpected(fcg::ImageError{
			fcg::ImageErrorCode::SDLFailure, std::string("Locking color-key source: ") + SDL_GetError()
		});
	const std::unique_ptr<SDL_Surface, decltype(&SDL_UnlockSurface)> copyUnlock(
		copyLock ? &source : nullptr, SDL_UnlockSurface
	);
	const auto *details = SDL_GetPixelFormatDetails(source.format);
	const auto bits = SDL_BITSPERPIXEL(source.format);
	const auto mask = SDL_ISPIXELFORMAT_INDEXED(source.format) ? (1u << bits) - 1
		: details->Rmask | details->Gmask | details->Bmask;
	for (int y = 0; y < source.h; ++y)
		for (int x = 0; x < source.w; ++x)
		{
			const auto *row = (const Uint8*)source.pixels + std::size_t(y) * source.pitch;
			Uint32 pixel = 0;
			if (bits < 8) {
				// LSB formats (4321) store the first pixel in the lowest bits, MSB formats (1234) in the highest
				const auto shift = SDL_PIXELORDER(source.format) == SDL_BITMAPORDER_4321
					? (std::size_t(x) * bits) % 8 : 8 - bits - ((std::size_t(x) * bits) % 8);
				pixel = (row[std::size_t(x) * bits / 8] >> shift) & mask;
			}
			else if (details->bytes_per_pixel == 1)
				pixel = row[x];
			else if (details->bytes_per_pixel == 2) {
				Uint16 packed;
				std::memcpy(&packed, row + std::size_t(x) * 2, 2);
				pixel = packed;
			}
			else if (details->bytes_per_pixel == 3) {
				const auto *packed = row + std::size_t(x) * 3;
				pixel = SDL_BYTEORDER == SDL_LIL_ENDIAN
					? packed[0] | (packed[1] << 8) | (packed[2] << 16)
					: (packed[0] << 16) | (packed[1] << 8) | packed[2];
			}
			else
				std::memcpy(&pixel, row + std::size_t(x) * 4, 4);
			if ((pixel & mask) == (key & mask))
			{
				auto *output = (Uint8*)destination.pixels + std::size_t(y) * destination.pitch;
				if (destination.format == SDL_PIXELFORMAT_RGBA128_FLOAT) {
					const float transparent = 0.f;
					std::memcpy(output + std::size_t(x) * 16 + 12, &transparent, 4);
				}
				else
					output[std::size_t(x) * 4 + 3] = 0;
			}
		}
	return {};
}

/// Duplicate locked RGB pixel rows and color metadata without running a color conversion.
[[nodiscard]] auto copyRGBSurface (SDL_Surface &source) -> SDL_Surface*
{
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> copy(
		SDL_CreateSurface(source.w, source.h, source.format), SDL_DestroySurface
	);
	if (!copy || !SDL_SetSurfaceColorspace(copy.get(), SDL_GetSurfaceColorspace(&source))
		|| !SDL_CopyProperties(SDL_GetSurfaceProperties(&source), SDL_GetSurfaceProperties(copy.get())))
		return nullptr;
	const bool lock = SDL_MUSTLOCK(&source);
	if (lock && !SDL_LockSurface(&source))
		return nullptr;
	const std::unique_ptr<SDL_Surface, decltype(&SDL_UnlockSurface)> unlock(lock ? &source : nullptr, SDL_UnlockSurface);
	const auto rowBytes = std::size_t(source.w) * SDL_BYTESPERPIXEL(source.format);
	for (int y = 0; y < source.h; ++y)
		std::memcpy(static_cast<std::byte*>(copy->pixels) + std::size_t(y) * copy->pitch,
			static_cast<const std::byte*>(source.pixels) + std::size_t(y) * source.pitch, rowBytes);
	return copy.release();
}

/// Duplicate source pixels and metadata for encoding-preserving conversion, disabling tone mapping and color keys.
[[nodiscard]] auto prepareConversion (SDL_Surface &source) -> std::expected<SDL_Surface*, fcg::ImageError>
{
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> copy(
		SDL_ISPIXELFORMAT_INDEXED(source.format)
			? copyIndexedSurface(source)
			: SDL_ISPIXELFORMAT_FOURCC(source.format) ? SDL_DuplicateSurface(&source) : copyRGBSurface(source), SDL_DestroySurface
	);
	if (!copy || !SDL_SetStringProperty(SDL_GetSurfaceProperties(copy.get()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "none")
		|| !SDL_SetSurfaceColorKey(copy.get(), false, 0))
		return std::unexpected(fcg::ImageError{
			fcg::ImageErrorCode::SDLFailure, std::string("Preparing image conversion: ") + SDL_GetError()
		});
	return copy.release();
}

/// Copy logical channels between equivalent native representations without decoding their color encoding.
[[nodiscard]] auto copyNativeConversion (SDL_Surface &source, SDL_PixelFormat format) -> SDL_Surface*
{
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> output(
		SDL_CreateSurface(source.w, source.h, format), SDL_DestroySurface
	);
	if (!output || !SDL_SetSurfaceColorspace(output.get(), SDL_GetSurfaceColorspace(&source))
		|| !SDL_CopyProperties(SDL_GetSurfaceProperties(&source), SDL_GetSurfaceProperties(output.get()))
		|| !SDL_SetStringProperty(SDL_GetSurfaceProperties(output.get()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "none"))
		return nullptr;
	const auto sourceOffsets = fcg::detail::nativeOffsets(source.format);
	const auto targetOffsets = fcg::detail::nativeOffsets(format);
	const auto type = SDL_PIXELTYPE(source.format);
	const unsigned componentSize = type == SDL_PIXELTYPE_ARRAYF32 ? 4
		: type == SDL_PIXELTYPE_ARRAYU16 || type == SDL_PIXELTYPE_ARRAYF16 ? 2 : 1;
	std::array<std::byte, 4> opaque{};
	if (type == SDL_PIXELTYPE_ARRAYF32) {
		const float value = 1.f;
		std::memcpy(opaque.data(), &value, 4);
	}
	else if (type == SDL_PIXELTYPE_ARRAYF16) {
		const auto value = fcg::Half(1.f).bits();
		std::memcpy(opaque.data(), &value, 2);
	}
	else
		opaque.fill(std::byte{0xff});
	for (int y = 0; y < source.h; ++y)
	{
		const auto *sourceRow = static_cast<const std::byte*>(source.pixels) + std::size_t(y) * source.pitch;
		auto *targetRow = static_cast<std::byte*>(output->pixels) + std::size_t(y) * output->pitch;
		for (int x = 0; x < source.w; ++x)
		{
			const auto *input = sourceRow + std::size_t(x) * SDL_BYTESPERPIXEL(source.format);
			auto *pixel = targetRow + std::size_t(x) * SDL_BYTESPERPIXEL(format);
			for (std::size_t channel = 0; channel < 4; ++channel)
				if (targetOffsets[channel] >= 0)
					std::memcpy(pixel + targetOffsets[channel] * componentSize,
						sourceOffsets[channel] >= 0 ? input + sourceOffsets[channel] * componentSize : opaque.data(), componentSize);
		}
	}
	return output.release();
}

/// Convert prepared RGB or YUV pixels with destination color properties established before conversion.
[[nodiscard]] auto convertPreparedPixels (SDL_Surface &source, SDL_PixelFormat format, SDL_Colorspace colorspace)
	-> SDL_Surface*
{
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> output(
		SDL_CreateSurface(source.w, source.h, format), SDL_DestroySurface
	);
	if (!output || !SDL_SetSurfaceColorspace(output.get(), colorspace)
		|| !SDL_CopyProperties(SDL_GetSurfaceProperties(&source), SDL_GetSurfaceProperties(output.get()))
		|| !SDL_SetStringProperty(SDL_GetSurfaceProperties(output.get()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "none"))
		return nullptr;
	const auto outputProperties = SDL_GetSurfaceProperties(output.get());
	if (SDL_GetSurfaceColorspace(&source) != colorspace)
		SDL_ClearProperty(outputProperties, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT);
	if (!SDL_ConvertPixelsAndColorspace(source.w, source.h,
		source.format, SDL_GetSurfaceColorspace(&source), SDL_GetSurfaceProperties(&source), source.pixels, source.pitch,
		format, colorspace, outputProperties, output->pixels, output->pitch))
		return nullptr;
	return output.release();
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// Image

auto Image::adopt (SDL_Surface *surface) -> std::expected<Image, ImageError> {
	if (!surface || surface->w <= 0 || surface->h <= 0 || surface->pitch <= 0 || !surface->pixels)
		return std::unexpected(ImageError{
			ImageErrorCode::InvalidArgument, "Image requires a nonempty pixel surface"
		});
	return Image(surface);
}

Image::~Image() {
	assert(!m_accessActive && "Image destroyed during pixel access");
	if (m_handle)
		SDL_DestroySurface(m_handle);
}

Image::Image(Image &&other) noexcept
	: m_handle(nullptr)
{
	assert(!other.m_accessActive && "Image moved during pixel access");
	m_handle = std::exchange(other.m_handle, nullptr);
}

auto Image::operator= (Image &&other) noexcept -> Image&
{
	assert(!m_accessActive && !other.m_accessActive && "Image move assignment during pixel access");
	if (this != &other) {
		if (m_handle)
			SDL_DestroySurface(m_handle);
		m_handle = std::exchange(other.m_handle, nullptr);
	}
	return *this;
}

[[nodiscard]] auto Image::width () const -> int {
	return m_handle ? m_handle->w : 0;
}

[[nodiscard]] auto Image::height () const -> int {
	return m_handle ? m_handle->h : 0;
}

[[nodiscard]] auto Image::pitch () const -> int {
	return m_handle ? m_handle->pitch : 0;
}

[[nodiscard]] auto Image::format () const -> SDL_PixelFormat {
	return m_handle ? m_handle->format : SDL_PIXELFORMAT_UNKNOWN;
}

[[nodiscard]] auto Image::pixelInfo () const -> std::expected<ImagePixelInfo, ImageError>
{
	if (!m_handle)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Cannot inspect an empty image"});
	auto formatInfo = pixelFormatInfo(m_handle->format);
	if (!formatInfo)
		return std::unexpected(formatInfo.error());
	ImagePixelInfo result;
	result.pixelFormat = std::move(*formatInfo);
	result.width = m_handle->w;
	result.height = m_handle->h;
	result.pitch = m_handle->pitch;
	result.colorspace = SDL_GetSurfaceColorspace(m_handle);
	const auto *palette = SDL_GetSurfacePalette(m_handle);
	result.hasPalette = palette != nullptr;
	result.paletteSize = palette ? palette->ncolors : 0;
	if (SDL_SurfaceHasColorKey(m_handle)) {
		Uint32 key;
		if (!SDL_GetSurfaceColorKey(m_handle, &key))
			return std::unexpected(ImageError{ImageErrorCode::SDLFailure, SDL_GetError()});
		result.colorKey = key;
	}
	const auto properties = SDL_GetSurfaceProperties(m_handle);
	if (SDL_HasProperty(properties, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT))
		result.sdrWhitePoint = SDL_GetFloatProperty(properties, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, 1.f);
	if (SDL_HasProperty(properties, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT))
		result.hdrHeadroom = SDL_GetFloatProperty(properties, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, 1.f);
	return result;
}

[[nodiscard]] auto Image::conversionInfo (SDL_PixelFormat targetFormat, PixelConversionOptions options) const
	-> std::expected<PixelConversionInfo, ImageError>
{
	if ((options.lossPolicy != PixelLossPolicy::Reject && options.lossPolicy != PixelLossPolicy::Allow)
		|| (options.colorKeyPolicy != PixelColorKeyPolicy::Bake && options.colorKeyPolicy != PixelColorKeyPolicy::Discard))
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Invalid conversion policy"});
	auto source = pixelInfo();
	if (!source)
		return std::unexpected(source.error());
	PixelConversionInfo result;
	result.source = *source;
	result.destinationColorspace = options.colorspace.value_or(source->colorspace);
	auto target = pixelFormatInfo(targetFormat);
	if (!target) {
		result.destination.format = targetFormat;
		result.diagnostic = "Choose a recognized non-indexed RGB destination format";
		return result;
	}
	result.destination = *target;
	const auto &from = source->pixelFormat;
	if (target->storage != PixelStorage::Interleaved && target->storage != PixelStorage::Packed) {
		result.diagnostic = "Destination must be non-indexed RGB; palette quantization, YUV and compressed output are unsupported";
		return result;
	}
	if (from.storage == PixelStorage::Compressed || from.storage == PixelStorage::External) {
		result.diagnostic = "Decode compressed or external pixels into CPU RGB storage first";
		return result;
	}
	if (from.storage == PixelStorage::YUV && !options.colorspace) {
		result.diagnostic = "YUV conversion requires an explicit RGB destination colorspace";
		return result;
	}
	if (SDL_COLORSPACETYPE(result.destinationColorspace) != SDL_COLOR_TYPE_RGB) {
		result.diagnostic = "Specify a known RGB destination colorspace";
		return result;
	}
	if (!detail::supportedImageEncoding(result.destinationColorspace)) {
		result.diagnostic = "Use supported full-range RGB with sRGB, linear or PQ transfer";
		return result;
	}
	if (from.storage != PixelStorage::YUV && !detail::supportedImageEncoding(source->colorspace)) {
		result.diagnostic = "Source RGB encoding is insufficiently described or unsupported; use loadAndGuess or assign supported encoding";
		return result;
	}
	if (from.storage == PixelStorage::YUV && source->colorspace != SDL_COLORSPACE_JPEG
		&& source->colorspace != SDL_COLORSPACE_BT601_LIMITED && source->colorspace != SDL_COLORSPACE_BT601_FULL
		&& source->colorspace != SDL_COLORSPACE_BT709_LIMITED && source->colorspace != SDL_COLORSPACE_BT709_FULL
		&& source->colorspace != SDL_COLORSPACE_BT2020_LIMITED && source->colorspace != SDL_COLORSPACE_BT2020_FULL) {
		result.diagnostic = "Set an explicit JPEG, BT.601, BT.709 or BT.2020 YUV source colorspace";
		return result;
	}
	const auto sourcePrimaries = SDL_COLORSPACEPRIMARIES(source->colorspace);
	const auto targetPrimaries = SDL_COLORSPACEPRIMARIES(result.destinationColorspace);
	if (sourcePrimaries != targetPrimaries && targetPrimaries != SDL_COLOR_PRIMARIES_BT709
		&& targetPrimaries != SDL_COLOR_PRIMARIES_BT2020
		&& !((targetPrimaries == SDL_COLOR_PRIMARIES_BT601 || targetPrimaries == SDL_COLOR_PRIMARIES_SMPTE240)
			&& (sourcePrimaries == SDL_COLOR_PRIMARIES_BT709 || sourcePrimaries == SDL_COLOR_PRIMARIES_BT2020))) {
		result.diagnostic = "SDL cannot transform the requested color primaries; choose BT.709 or BT.2020 output";
		return result;
	}
	if (from.storage == PixelStorage::Indexed) {
		const auto *palette = SDL_GetSurfacePalette(m_handle);
		if (!palette || !palette->colors || palette->ncolors <= 0
			|| palette->ncolors > (1 << SDL_BITSPERPIXEL(m_handle->format))) {
			result.diagnostic = "Indexed source requires a valid attached palette";
			return result;
		}
	}
	if (source->colorKey && (!from.bytesPerPixel || *from.bytesPerPixel > 4)
		&& from.storage != PixelStorage::Indexed) {
		result.diagnostic = "Color-key baking requires indexed or at most 32-bit RGB source storage";
		return result;
	}
	result.supported = true;
	result.layoutChange = from.storage != target->storage || from.bytesPerPixel != target->bytesPerPixel
		|| from.bitsPerPixel != target->bitsPerPixel;
	result.channelOrderChange = from.channelOrder != target->channelOrder;
	result.paletteExpansion = from.storage == PixelStorage::Indexed;
	result.alphaAddition = !from.hasAlpha && target->hasAlpha;
	result.alphaRemoval = from.hasAlpha && !target->hasAlpha;
	result.colorKeyBaking = source->colorKey.has_value() && options.colorKeyPolicy == PixelColorKeyPolicy::Bake;
	result.colorKeyDiscarding = source->colorKey.has_value() && options.colorKeyPolicy == PixelColorKeyPolicy::Discard;
	result.representationChange = from.representation != target->representation;
	result.colorEncodingChange = source->colorspace != result.destinationColorspace;
	result.colorTransformLoss = result.colorEncodingChange || from.storage == PixelStorage::YUV;
	const auto floatPrecision = [](PixelRepresentation representation) -> unsigned {
		return representation == PixelRepresentation::Binary16 ? 11 : representation == PixelRepresentation::Binary32 ? 24 : 0;
	};
	const bool sourceFloat = floatPrecision(from.representation) != 0;
	const bool targetFloat = floatPrecision(target->representation) != 0;
	for (std::size_t channel = 0; channel < 4; ++channel)
	{
		const auto sourceBits = from.channelBits[channel];
		const auto targetBits = target->channelBits[channel];
		if (!sourceBits)
			continue;
		const auto sourcePrecision = sourceFloat ? floatPrecision(from.representation) : sourceBits;
		const auto targetPrecision = targetFloat ? floatPrecision(target->representation) : targetBits;
		result.precisionReduction |= sourcePrecision > targetPrecision;
	}
	result.precisionReduction |= sourceFloat && !targetFloat;
	if (from.yuvSampleBits)
		result.precisionReduction |= *from.yuvSampleBits > (targetFloat ? floatPrecision(target->representation) : target->channelBits[0]);
	result.rangeClipping = (sourceFloat && !targetFloat)
		|| (from.representation == PixelRepresentation::Binary32 && target->representation == PixelRepresentation::Binary16)
		|| (!targetFloat && source->hdrHeadroom.value_or(1.f) > 1.f);
	result.allocatesStorage = true;
	if (result.colorKeyBaking && !target->hasAlpha) {
		result.diagnostic = "Color-key baking requires an alpha-capable destination; select RGBA or explicitly discard the key";
		return result;
	}
	const bool loss = result.precisionReduction || result.rangeClipping || result.alphaRemoval
		|| result.colorKeyDiscarding || result.colorTransformLoss;
	result.permitted = !loss || options.lossPolicy == PixelLossPolicy::Allow;
	if (!result.permitted)
		result.diagnostic = "Potential precision, range, transparency or encoding loss requires PixelLossPolicy::Allow";
	return result;
}

[[nodiscard]] auto Image::converted (SDL_PixelFormat targetFormat, PixelConversionOptions options) const
	-> std::expected<Image, ImageError>
{
	if (!m_handle || m_accessActive)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Image conversion requires an available image"});
	auto analysis = conversionInfo(targetFormat, options);
	if (!analysis)
		return std::unexpected(analysis.error());
	if (!analysis->supported)
		return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, analysis->diagnostic});
	if (analysis->colorKeyBaking && !analysis->destination.hasAlpha)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, analysis->diagnostic});
	if (!analysis->permitted)
		return std::unexpected(ImageError{ImageErrorCode::LossyConversion, analysis->diagnostic});
	auto access = acquirePixels(SDL_PIXELFORMAT_UNKNOWN);
	if (!access)
		return std::unexpected(access.error());
	const auto release = [this](SDL_Surface *surface) { detail::releasePixels(surface, &m_accessActive); };
	const std::unique_ptr<SDL_Surface, decltype(release)> session(*access, release);
	const auto &from = analysis->source.pixelFormat;
	const auto &to = analysis->destination;
	if (from.directAccess && to.directAccess && from.representation == to.representation
		&& from.channelBits[0] == to.channelBits[0] && !analysis->colorEncodingChange && !analysis->colorKeyBaking) {
		auto *output = copyNativeConversion(*m_handle, targetFormat);
		if (!output)
			return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Copying native channels: ") + SDL_GetError()});
		return Image::adopt(output);
	}
	auto prepared = prepareConversion(*m_handle);
	if (!prepared)
		return std::unexpected(prepared.error());
	const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> copy(*prepared, SDL_DestroySurface);
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> paletteExpansion(nullptr, SDL_DestroySurface);
	if (analysis->paletteExpansion) {
		auto expanded = detail::expandIndexedPixels(*copy);
		if (!expanded)
			return std::unexpected(expanded.error());
		paletteExpansion.reset(*expanded);
	}
	auto *input = paletteExpansion ? paletteExpansion.get() : copy.get();
	// Expand keys into a full-precision intermediate, then convert to any requested alpha layout.
	const auto intermediateFormat = analysis->colorKeyBaking ? SDL_PIXELFORMAT_RGBA128_FLOAT : targetFormat;
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> output(
		convertPreparedPixels(*input, intermediateFormat, analysis->destinationColorspace), SDL_DestroySurface
	);
	if (!output)
		return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Converting image: ") + SDL_GetError()});
	if (analysis->colorKeyBaking) {
		if (auto baked = bakeColorKey(*copy, *output, *analysis->source.colorKey); !baked)
			return std::unexpected(baked.error());
		if (targetFormat != intermediateFormat)
			output.reset(convertPreparedPixels(*output, targetFormat, analysis->destinationColorspace));
		if (!output)
			return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Converting keyed image: ") + SDL_GetError()});
	}
	if (!SDL_SetSurfaceColorKey(output.get(), false, 0))
		return std::unexpected(ImageError{ImageErrorCode::SDLFailure, SDL_GetError()});
	return Image::adopt(output.release());
}

auto Image::upload (Device &device, ImageUploadOptions options) const -> std::expected<Texture, ImageError>
{
	if (!m_handle || m_accessActive)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Upload requires an available image"});
	if (options.samplePolicy != ImageUploadSamplePolicy::ConvertToLinearSRGB
		&& options.samplePolicy != ImageUploadSamplePolicy::PreserveSamples
		&& options.samplePolicy != ImageUploadSamplePolicy::PreserveSamplesIfUnknown)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Invalid upload sample policy"});
	if (options.unidentifiedSourceColorspace && !detail::supportedImageEncoding(*options.unidentifiedSourceColorspace))
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Upload fallback requires supported full-range RGB"});

	// Select preservation against the original metadata, before considering an explicit fallback.
	auto *surface = m_handle;
	const auto originalColorspace = SDL_GetSurfaceColorspace(surface);
	auto colorspace = originalColorspace;
	auto description = pixelFormatInfo(surface->format);
	if (!description)
		return std::unexpected(description.error());
	const bool rgb = description->storage == PixelStorage::Packed || description->storage == PixelStorage::Interleaved
		|| description->storage == PixelStorage::Indexed;
	const bool preserve = options.samplePolicy == ImageUploadSamplePolicy::PreserveSamples
		|| (options.samplePolicy == ImageUploadSamplePolicy::PreserveSamplesIfUnknown
			&& detail::missingImageEncoding(originalColorspace));
	const auto supports = [&device] (SDL_GPUTextureFormat format) {
		return SDL_GPUTextureSupportsFormat(device.handle(), format, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
	};
	const auto *details = SDL_GetPixelFormatDetails(surface->format);
	const bool keyed = SDL_SurfaceHasColorKey(surface);
	if (keyed && (!details || details->bytes_per_pixel > 4))
		return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat,
			"Color keys require an indexed or at most 32-bit pixel representation"});
	SDL_GPUTextureFormat gpuFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
	SDL_PixelFormat cpuFormat = SDL_PIXELFORMAT_UNKNOWN;
	SDL_Colorspace outputColorspace = colorspace;
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> converted(nullptr, SDL_DestroySurface);

	// Stored integer channels use UNORM sampling; floating channels retain their numeric values and range.
	if (preserve)
		if (auto layout = detail::storedImageLayout(*description, supports)) {
			cpuFormat = layout->cpu;
			gpuFormat = layout->gpu;
		}
	if (preserve && gpuFormat == SDL_GPU_TEXTUREFORMAT_INVALID && !options.unidentifiedSourceColorspace)
		return std::unexpected(ImageError{rgb ? ImageErrorCode::GPUFailure : ImageErrorCode::UnsupportedFormat,
			"No supported sample-preserving RGB upload representation"});
	const bool preserving = preserve && gpuFormat != SDL_GPU_TEXTUREFORMAT_INVALID;

	// Color-managed upload uses only supported transforms and fills genuinely missing components independently.
	if (!preserving)
	{
		if (rgb && options.unidentifiedSourceColorspace)
			colorspace = detail::fillImageEncoding(colorspace, *options.unidentifiedSourceColorspace);
		if (rgb && !detail::supportedImageEncoding(colorspace))
			return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat,
				"Image upload requires supported RGB encoding; use loadAndGuess, an upload fallback, or uploadStoredSamples"});
		if (!rgb && (description->storage != PixelStorage::YUV
			|| (colorspace != SDL_COLORSPACE_JPEG && colorspace != SDL_COLORSPACE_BT601_LIMITED
				&& colorspace != SDL_COLORSPACE_BT601_FULL && colorspace != SDL_COLORSPACE_BT709_LIMITED
				&& colorspace != SDL_COLORSPACE_BT709_FULL && colorspace != SDL_COLORSPACE_BT2020_LIMITED
				&& colorspace != SDL_COLORSPACE_BT2020_FULL)))
			return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Upload requires supported CPU RGB or YUV storage"});
		const bool srgb8 = colorspace == SDL_COLORSPACE_SRGB && details
			&& !SDL_ISPIXELFORMAT_FLOAT(surface->format) && !SDL_ISPIXELFORMAT_FOURCC(surface->format)
			&& details->Rbits <= 8 && details->Gbits <= 8 && details->Bbits <= 8 && details->Abits <= 8;
		cpuFormat = surface->format;
		outputColorspace = colorspace;
		if (srgb8) {
			cpuFormat = surface->format == SDL_PIXELFORMAT_BGRA32 ? SDL_PIXELFORMAT_BGRA32 : SDL_PIXELFORMAT_RGBA32;
			gpuFormat = cpuFormat == SDL_PIXELFORMAT_BGRA32
				? SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
		}
		else if (colorspace == SDL_COLORSPACE_SRGB_LINEAR)
		{
			if (cpuFormat == SDL_PIXELFORMAT_RGBA32) gpuFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			else if (cpuFormat == SDL_PIXELFORMAT_BGRA32) gpuFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
			else if (cpuFormat == SDL_PIXELFORMAT_RGBA64) gpuFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM;
			else if (cpuFormat == SDL_PIXELFORMAT_RGBA64_FLOAT) gpuFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
			else if (cpuFormat == SDL_PIXELFORMAT_RGBA128_FLOAT) gpuFormat = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
		}
		if (gpuFormat == SDL_GPU_TEXTUREFORMAT_INVALID || !supports(gpuFormat)) {
			gpuFormat = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
			cpuFormat = SDL_PIXELFORMAT_RGBA128_FLOAT;
			outputColorspace = SDL_COLORSPACE_SRGB_LINEAR;
		}
		if (!supports(gpuFormat))
			return std::unexpected(ImageError{ImageErrorCode::GPUFailure, "No precision-preserving sampled texture format is supported"});
	}

	// Work on independent storage whenever layouts, encoding or separate transparency require preparation.
	if (cpuFormat != surface->format || outputColorspace != originalColorspace || keyed)
	{
		auto prepared = prepareConversion(*surface);
		if (!prepared)
			return std::unexpected(prepared.error());
		const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> copy(*prepared, SDL_DestroySurface);
		Uint32 key = 0;
		if (keyed && !SDL_GetSurfaceColorKey(surface, &key))
			return std::unexpected(ImageError{ImageErrorCode::SDLFailure, SDL_GetError()});
		if (preserving)
		{
			const auto target = pixelFormatInfo(cpuFormat);
			if (description->directAccess && target && target->directAccess
				&& description->representation == target->representation
				&& description->channelBits[0] == target->channelBits[0])
				converted.reset(copyNativeConversion(*copy, cpuFormat));
			else {
				auto output = detail::copyStoredPixels(*copy, cpuFormat);
				if (!output)
					return std::unexpected(output.error());
				converted.reset(*output);
			}
		}
		else {
			if (!SDL_SetSurfaceColorspace(copy.get(), colorspace))
				return detail::imageSDLError(ImageErrorCode::SDLFailure, "Assigning effective upload colorspace");
			std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> expanded(nullptr, SDL_DestroySurface);
			if (description->storage == PixelStorage::Indexed) {
				auto pixels = detail::expandIndexedPixels(*copy);
				if (!pixels)
					return std::unexpected(pixels.error());
				expanded.reset(*pixels);
			}
			converted.reset(convertPreparedPixels(expanded ? *expanded : *copy, cpuFormat, outputColorspace));
		}
		if (!converted)
			return detail::imageSDLError(ImageErrorCode::SDLFailure, "Preparing image upload");
		if (keyed)
			if (auto result = bakeColorKey(*copy, *converted, key); !result)
				return std::unexpected(result.error());
		surface = converted.get();
	}
	SDL_GPUTextureCreateInfo info{};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = gpuFormat;
	info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
	info.width = (Uint32)surface->w;
	info.height = (Uint32)surface->h;
	info.layer_count_or_depth = info.num_levels = 1;
	info.sample_count = SDL_GPU_SAMPLECOUNT_1;
	auto texture = Texture::create(device, info);
	if (!texture)
		return std::unexpected(ImageError{ImageErrorCode::GPUFailure, texture.error().message});
	const bool lock = SDL_MUSTLOCK(surface);
	if (lock && !SDL_LockSurface(surface))
		return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Locking image: ") + SDL_GetError()});
	const std::unique_ptr<SDL_Surface, decltype(&SDL_UnlockSurface)> unlock(
		lock ? surface : nullptr, SDL_UnlockSurface
	);
	const auto bytes = std::span<const std::byte>(
		(const std::byte*)surface->pixels, (std::size_t)surface->pitch * surface->h
	);
	auto uploaded = texture->upload(bytes, TextureTransferLayout{0, (std::size_t)surface->pitch, 0});
	if (!uploaded)
		return std::unexpected(ImageError{ImageErrorCode::GPUFailure, uploaded.error().message});
	return std::move(*texture);
}



//////
//
// Module namespace close
//

// namespace fcg
}
