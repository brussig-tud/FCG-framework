
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

// Local includes
#include "FCG/Image/image.h"
#include "image_internal.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Decode a native packed word without imposing an alignment requirement.
[[nodiscard]] auto packedPixel (const std::byte *bytes, unsigned size) -> Uint32
{
	Uint32 value = 0;
	if (size == 1)
		value = std::to_integer<Uint8>(*bytes);
	else if (size == 2) {
		Uint16 word;
		std::memcpy(&word, bytes, 2);
		value = word;
	}
	else if (size == 3) {
		const auto *p = reinterpret_cast<const Uint8*>(bytes);
		value = SDL_BYTEORDER == SDL_LIL_ENDIAN ? p[0] | (p[1] << 8) | (p[2] << 16)
			: (p[0] << 16) | (p[1] << 8) | p[2];
	}
	else
		std::memcpy(&value, bytes, 4);
	return value;
}

/// Copy a native component without alignment or typed storage lifetime assumptions.
template<class T>
[[nodiscard]] auto loadComponent (const std::byte *bytes) -> T {
	T value;
	std::memcpy(&value, bytes, sizeof(T));
	return value;
}

/// Decode one locked pixel into logical source-encoded RGBA floats.
[[nodiscard]] auto readLocked (SDL_Surface &surface, int x, int y) -> std::expected<SDL_FColor, fcg::ImageError>
{
	const auto format = surface.format;
	const auto *row = static_cast<const std::byte*>(surface.pixels) + std::size_t(y) * surface.pitch;
	if (SDL_ISPIXELFORMAT_INDEXED(format))
	{
		const auto bits = SDL_BITSPERPIXEL(format);
		const auto *palette = SDL_GetSurfacePalette(&surface);
		if (!palette || !palette->colors || palette->ncolors <= 0 || palette->ncolors > (1 << bits))
			return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::InvalidState, "Indexed image requires a valid palette"});
		const auto bitOffset = std::size_t(x) * bits;
		const unsigned shift = SDL_PIXELORDER(format) == SDL_BITMAPORDER_4321 ? bitOffset % 8 : 8 - bits - bitOffset % 8;
		const unsigned index = (std::to_integer<Uint8>(row[bitOffset / 8]) >> shift) & ((1u << bits) - 1);
		if (index >= unsigned(palette->ncolors))
			return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::InvalidState, "Pixel index is outside the attached palette"});
		const auto color = palette->colors[index];
		return SDL_FColor{color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f};
	}
	if (SDL_ISPIXELFORMAT_FOURCC(format))
		return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::UnsupportedFormat, "Convert YUV or compressed pixels to RGB before color access"});
	const auto *pixel = row + std::size_t(x) * SDL_BYTESPERPIXEL(format);
	std::array<float, 4> rgba{0, 0, 0, 1};
	if (fcg::detail::nativePixelFormat(format))
	{
		const auto offsets = fcg::detail::nativeOffsets(format);
		const auto type = SDL_PIXELTYPE(format);
		for (std::size_t c = 0; c < 4; ++c)
		{
			if (offsets[c] < 0)
				continue;
			const auto offset = std::size_t(offsets[c]);
			if (type == SDL_PIXELTYPE_ARRAYF32)
				rgba[c] = loadComponent<float>(pixel + offset * 4);
			else if (type == SDL_PIXELTYPE_ARRAYF16)
				rgba[c] = float(fcg::Half::fromBits(loadComponent<Uint16>(pixel + offset * 2)));
			else if (type == SDL_PIXELTYPE_ARRAYU16)
				rgba[c] = loadComponent<Uint16>(pixel + offset * 2) / 65535.f;
			else
				rgba[c] = std::to_integer<Uint8>(pixel[offset]) / 255.f;
		}
	}
	else
	{
		const auto *details = SDL_GetPixelFormatDetails(format);
		const auto word = packedPixel(pixel, details->bytes_per_pixel);
		const std::array<Uint32, 4> masks{details->Rmask, details->Gmask, details->Bmask, details->Amask};
		const std::array<unsigned, 4> shifts{details->Rshift, details->Gshift, details->Bshift, details->Ashift};
		for (std::size_t c = 0; c < 4; ++c)
			if (masks[c])
				rgba[c] = float((word & masks[c]) >> shifts[c]) / float(masks[c] >> shifts[c]);
	}
	return SDL_FColor{rgba[0], rgba[1], rgba[2], rgba[3]};
}

/// Clamp finite normalized samples and round them to the nearest integer code; halfway values round upward.
[[nodiscard]] auto quantize (float value, Uint32 maximum) -> Uint32 {
	return Uint32(std::floor(double(std::clamp(value, 0.f, 1.f)) * maximum + 0.5));
}

/// Store normalized or floating channels in locked RGB storage, retaining unused bits and components.
void writeLocked (SDL_Surface &surface, int x, int y, SDL_FColor color)
{
	const auto format = surface.format;
	auto *pixel = static_cast<std::byte*>(surface.pixels) + std::size_t(y) * surface.pitch
		+ std::size_t(x) * SDL_BYTESPERPIXEL(format);
	const std::array<float, 4> rgba{color.r, color.g, color.b, color.a};
	if (fcg::detail::nativePixelFormat(format))
	{
		const auto offsets = fcg::detail::nativeOffsets(format);
		const auto type = SDL_PIXELTYPE(format);
		for (std::size_t c = 0; c < 4; ++c)
		{
			if (offsets[c] < 0)
				continue;
			const auto offset = std::size_t(offsets[c]);
			if (type == SDL_PIXELTYPE_ARRAYF32)
				std::memcpy(pixel + offset * 4, &rgba[c], 4);
			else if (type == SDL_PIXELTYPE_ARRAYF16) {
				const auto half = fcg::Half(rgba[c]).bits();
				std::memcpy(pixel + offset * 2, &half, 2);
			}
			else if (type == SDL_PIXELTYPE_ARRAYU16) {
				const auto value = Uint16(quantize(rgba[c], 65535));
				std::memcpy(pixel + offset * 2, &value, 2);
			}
			else
				pixel[offset] = std::byte(quantize(rgba[c], 255));
		}
		return;
	}
	const auto *details = SDL_GetPixelFormatDetails(format);
	auto word = packedPixel(pixel, details->bytes_per_pixel);
	const std::array<Uint32, 4> masks{details->Rmask, details->Gmask, details->Bmask, details->Amask};
	const std::array<unsigned, 4> shifts{details->Rshift, details->Gshift, details->Bshift, details->Ashift};
	for (std::size_t c = 0; c < 4; ++c)
		if (masks[c])
			word = (word & ~masks[c]) | (quantize(rgba[c], masks[c] >> shifts[c]) << shifts[c]);
	if (details->bytes_per_pixel == 1)
		*pixel = std::byte(word);
	else if (details->bytes_per_pixel == 2) {
		const auto value = Uint16(word);
		std::memcpy(pixel, &value, 2);
	}
	else
		std::memcpy(pixel, &word, 4);
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
// Function implementations
//

[[nodiscard]] auto detail::expandIndexedPixels (SDL_Surface &source) -> std::expected<SDL_Surface*, ImageError>
{
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> output(
		SDL_CreateSurface(source.w, source.h, SDL_PIXELFORMAT_RGBA128_FLOAT), SDL_DestroySurface
	);
	if (!output || !SDL_SetSurfaceColorspace(output.get(), SDL_GetSurfaceColorspace(&source))
		|| !SDL_CopyProperties(SDL_GetSurfaceProperties(&source), SDL_GetSurfaceProperties(output.get()))
		|| !SDL_SetStringProperty(SDL_GetSurfaceProperties(output.get()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "none"))
		return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Expanding palette: ") + SDL_GetError()});
	for (int y = 0; y < source.h; ++y)
		for (int x = 0; x < source.w; ++x) {
			auto color = readLocked(source, x, y);
			if (!color)
				return std::unexpected(color.error());
			writeLocked(*output, x, y, *color);
		}
	return output.release();
}

void detail::releasePixels (SDL_Surface *surface, bool *active) noexcept {
	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
	*active = false;
}



//////
//
// Class implementations
//

////
// Image

[[nodiscard]] auto Image::acquirePixels (SDL_PixelFormat requiredFormat) const -> std::expected<SDL_Surface*, ImageError>
{
	if (!m_handle || m_accessActive)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Image is empty or already has an active access session"});
	if (requiredFormat != SDL_PIXELFORMAT_UNKNOWN && m_handle->format != requiredFormat)
		return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Image format does not match native access; explicitly convert first"});
	auto description = pixelFormatInfo(m_handle->format);
	if (!description)
		return std::unexpected(description.error());
	if (description->storage == PixelStorage::Compressed || description->storage == PixelStorage::External)
		return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Access requires CPU RGB, indexed or YUV storage"});
	if (m_handle->w <= 0 || m_handle->h <= 0 || m_handle->pitch <= 0)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Image storage requires positive dimensions and pitch"});
	const auto width = std::size_t(m_handle->w);
	const auto pitch = std::size_t(m_handle->pitch);
	const auto height = std::size_t(m_handle->h);
	const auto limit = std::size_t(std::numeric_limits<std::ptrdiff_t>::max());
	if (height > limit / pitch || width > (limit - 7) / 128)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Image byte offsets exceed addressable storage"});
	std::size_t rowBytes = description->bytesPerPixel ? width * *description->bytesPerPixel
		: description->bitsPerPixel ? (width * *description->bitsPerPixel + 7) / 8 : width;
	if (description->storage == PixelStorage::YUV) {
		if (m_handle->format == SDL_PIXELFORMAT_P010)
			rowBytes = ((width + 1) / 2) * 4;
		else if (m_handle->format == SDL_PIXELFORMAT_YUY2 || m_handle->format == SDL_PIXELFORMAT_UYVY
			|| m_handle->format == SDL_PIXELFORMAT_YVYU)
			rowBytes = ((width + 1) / 2) * 4;
		else if (m_handle->format == SDL_PIXELFORMAT_NV12 || m_handle->format == SDL_PIXELFORMAT_NV21)
			rowBytes = ((width + 1) / 2) * 2;
	}
	if (pitch < rowBytes)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Image pitch is shorter than its visible row storage"});
	const bool lock = SDL_MUSTLOCK(m_handle);
	if (lock && !SDL_LockSurface(m_handle))
		return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Locking image pixels: ") + SDL_GetError()});
	if (!m_handle->pixels) {
		if (lock)
			SDL_UnlockSurface(m_handle);
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Image has no accessible pixel storage"});
	}
	m_accessActive = true;
	return m_handle;
}

[[nodiscard]] auto Image::validateDestination (const Image &destination) const -> std::expected<void, ImageError>
{
	if (!m_handle || !destination.m_handle || m_accessActive || destination.m_accessActive)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Kernel requires two available images"});
	if (width() != destination.width() || height() != destination.height())
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Kernel images must have matching dimensions"});
	if (this == &destination)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Kernel source and destination storage overlap"});
	return {};
}

[[nodiscard]] auto Image::validateStorageOverlap (const Image &destination) const -> std::expected<void, ImageError>
{
	const auto sourceBegin = reinterpret_cast<std::uintptr_t>(m_handle->pixels);
	const auto targetBegin = reinterpret_cast<std::uintptr_t>(destination.m_handle->pixels);
	const auto sourceSize = std::size_t(pitch()) * height();
	const auto targetSize = std::size_t(destination.pitch()) * destination.height();
	if (sourceBegin <= targetBegin ? targetBegin - sourceBegin < sourceSize : sourceBegin - targetBegin < targetSize)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Kernel source and destination storage overlap"});
	return {};
}

[[nodiscard]] auto Image::readColorFloat (int x, int y) const -> std::expected<SDL_FColor, ImageError>
{
	auto acquired = acquirePixels(SDL_PIXELFORMAT_UNKNOWN);
	if (!acquired)
		return std::unexpected(acquired.error());
	const auto release = [this](SDL_Surface *surface) { detail::releasePixels(surface, &m_accessActive); };
	const std::unique_ptr<SDL_Surface, decltype(release)> session(*acquired, release);
	if (x < 0 || y < 0 || x >= width() || y >= height())
		return detail::pixelBoundsError();
	return readLocked(**acquired, x, y);
}

[[nodiscard]] auto Image::readColor8 (int x, int y) const -> std::expected<SDL_Color, ImageError>
{
	auto color = readColorFloat(x, y);
	if (!color)
		return std::unexpected(color.error());
	if (!std::isfinite(color->r) || !std::isfinite(color->g) || !std::isfinite(color->b) || !std::isfinite(color->a))
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Cannot quantize non-finite color components"});
	return SDL_Color{Uint8(quantize(color->r, 255)), Uint8(quantize(color->g, 255)),
		Uint8(quantize(color->b, 255)), Uint8(quantize(color->a, 255))};
}

[[nodiscard]] auto Image::writeColor8 (int x, int y, SDL_Color value, PixelLossPolicy lossPolicy)
	-> std::expected<void, ImageError> {
	return writeColor(x, y, SDL_FColor{value.r / 255.f, value.g / 255.f, value.b / 255.f, value.a / 255.f}, lossPolicy, true);
}

[[nodiscard]] auto Image::writeColorFloat (int x, int y, SDL_FColor value, PixelLossPolicy lossPolicy)
	-> std::expected<void, ImageError> {
	return writeColor(x, y, value, lossPolicy, false);
}

[[nodiscard]] auto Image::writeColor (int x, int y, SDL_FColor value, PixelLossPolicy policy, bool byteSource)
	-> std::expected<void, ImageError>
{
	if (policy != PixelLossPolicy::Allow && policy != PixelLossPolicy::Reject)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Invalid pixel write loss policy"});
	auto acquired = acquirePixels(SDL_PIXELFORMAT_UNKNOWN);
	if (!acquired)
		return std::unexpected(acquired.error());
	const auto release = [this](SDL_Surface *surface) { detail::releasePixels(surface, &m_accessActive); };
	const std::unique_ptr<SDL_Surface, decltype(release)> session(*acquired, release);
	if (x < 0 || y < 0 || x >= width() || y >= height())
		return detail::pixelBoundsError();
	auto description = pixelFormatInfo(format());
	if (description->storage != PixelStorage::Interleaved && description->storage != PixelStorage::Packed)
		return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Convert indexed or YUV images to RGB before color writes"});
	const bool integer = description->representation == PixelRepresentation::UnsignedInteger;
	if (integer && (!std::isfinite(value.r) || !std::isfinite(value.g) || !std::isfinite(value.b) || !std::isfinite(value.a)))
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Integer color writes require finite components"});
	bool loss = !description->hasAlpha;
	if (byteSource) {
		for (const auto bits : description->channelBits)
			loss |= integer && bits < 8;
	}
	else
		loss |= description->representation != PixelRepresentation::Binary32;
	if (loss && policy == PixelLossPolicy::Reject)
		return std::unexpected(ImageError{ImageErrorCode::LossyConversion, "Pixel narrowing or alpha removal requires PixelLossPolicy::Allow"});
	writeLocked(**acquired, x, y, value);
	return {};
}



//////
//
// Module namespace close
//

// namespace fcg
}
