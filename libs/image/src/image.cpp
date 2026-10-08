
//////
//
// Includes
//

// C++ STL
#include <cstring>
#include <utility>
#include <memory>
#include <limits>

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "FCG/Image/image.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Bake an indexed or packed color key into the converted alpha channel, preserving original texel precision.
/// The source is an independent duplicate with its key disabled; both surfaces outlive this operation.
auto bakeColorKey (SDL_Surface &source, SDL_Surface &destination, Uint32 key) -> std::expected<void, fcg::ImageError>
{
	// Bake keys using original representations, preserving palette-index identity and high-bit RGB precision.
	// SDL's packed color-key conversion is limited to 32-bit destinations; floating-point expansion must
	// apply alpha separately after color conversion.
	const bool copyLock = SDL_MUSTLOCK(&source);
	if (copyLock && !SDL_LockSurface(&source))
		return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::SDLFailure, std::string("Locking color-key source: ") + SDL_GetError()});
	const std::unique_ptr<SDL_Surface, decltype(&SDL_UnlockSurface)> copyUnlock(copyLock ? &source : nullptr, SDL_UnlockSurface);
	const auto *details = SDL_GetPixelFormatDetails(source.format);
	const auto bits = SDL_BITSPERPIXEL(source.format);
	const auto mask = SDL_ISPIXELFORMAT_INDEXED(source.format) ? (1u << bits) - 1
		: details->Rmask | details->Gmask | details->Bmask;
	for (int y = 0; y < source.h; ++y)
		for (int x = 0; x < source.w; ++x)
		{
			const auto *row = (const Uint8*)source.pixels + y * source.pitch;
			Uint32 pixel = 0;
			if (bits < 8) {
				const auto shift = SDL_PIXELORDER(source.format) == SDL_BITMAPORDER_4321
					? 8 - bits - ((x * bits) % 8) : (x * bits) % 8;
				pixel = (row[x * bits / 8] >> shift) & mask;
			}
			else if (details->bytes_per_pixel == 1)
				pixel = row[x];
			else if (details->bytes_per_pixel == 2) {
				Uint16 packed;
				std::memcpy(&packed, row + x * 2, 2);
				pixel = packed;
			}
			else if (details->bytes_per_pixel == 3) {
				const auto *packed = row + x * 3;
				pixel = SDL_BYTEORDER == SDL_LIL_ENDIAN
					? packed[0] | (packed[1] << 8) | (packed[2] << 16)
					: (packed[0] << 16) | (packed[1] << 8) | packed[2];
			}
			else
				std::memcpy(&pixel, row + x * 4, 4);
			if ((pixel & mask) == (key & mask))
			{
				auto *output = (Uint8*)destination.pixels + y * destination.pitch;
				if (destination.format == SDL_PIXELFORMAT_RGBA128_FLOAT) {
					const float transparent = 0.f;
					std::memcpy(output + x * 16 + 12, &transparent, 4);
				}
				else
					output[x * 4 + 3] = 0;
			}
		}
	return {};
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

/// The library top-level namespace.
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

Image::~Image () {
	if (m_handle)
		SDL_DestroySurface(m_handle);
}

Image::Image (Image &&other) noexcept
	: m_handle(std::exchange(other.m_handle, nullptr))
{}

auto Image::operator= (Image &&other) noexcept -> Image&
{
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

auto Image::upload (Device &device) const -> std::expected<Texture, ImageError>
{
	if (!m_handle)
		return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Cannot upload an empty image"});
	auto *surface = m_handle;
	const auto colorspace = SDL_GetSurfaceColorspace(surface);
	const auto *details = SDL_GetPixelFormatDetails(surface->format);
	const bool srgb8 = colorspace == SDL_COLORSPACE_SRGB && details
		&& !SDL_ISPIXELFORMAT_FLOAT(surface->format) && !SDL_ISPIXELFORMAT_FOURCC(surface->format)
		&& details->Rbits <= 8 && details->Gbits <= 8 && details->Bbits <= 8 && details->Abits <= 8;
	if (SDL_SurfaceHasColorKey(surface) && (!details || details->bytes_per_pixel > 4))
		return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Color keys require an indexed or at most 32-bit pixel representation"});
	SDL_GPUTextureFormat gpuFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
	SDL_PixelFormat cpuFormat = surface->format;
	SDL_Colorspace outputColorspace = colorspace;
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
	if (gpuFormat == SDL_GPU_TEXTUREFORMAT_INVALID
		|| !SDL_GPUTextureSupportsFormat(device.handle(), gpuFormat, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
		gpuFormat = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
		cpuFormat = SDL_PIXELFORMAT_RGBA128_FLOAT;
		outputColorspace = SDL_COLORSPACE_SRGB_LINEAR;
	}
	if (!SDL_GPUTextureSupportsFormat(device.handle(), gpuFormat, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER))
		return std::unexpected(ImageError{ImageErrorCode::GPUFailure, "No precision-preserving sampled texture format is supported"});
	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> converted(nullptr, SDL_DestroySurface);
	if (cpuFormat != surface->format || outputColorspace != colorspace || SDL_SurfaceHasColorKey(surface))
	{
		// Conversion can temporarily change source blit state. Perform it on an independent duplicate and disable
		// its tone mapping, so HDR headroom and any source tone-map preference cannot clip uploaded values.
		std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> copy(SDL_DuplicateSurface(surface), SDL_DestroySurface);
		if (!copy || !SDL_SetStringProperty(SDL_GetSurfaceProperties(copy.get()), SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, "none"))
			return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Preparing image conversion: ") + SDL_GetError()});
		Uint32 key = 0;
		const bool keyed = SDL_SurfaceHasColorKey(surface);
		if (keyed) {
			if (!SDL_GetSurfaceColorKey(surface, &key) || !SDL_SetSurfaceColorKey(copy.get(), false, 0))
				return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Preparing color key: ") + SDL_GetError()});
		}
		converted.reset(SDL_ConvertSurfaceAndColorspace(copy.get(), cpuFormat, nullptr, outputColorspace, 0));
		if (!converted)
			return std::unexpected(ImageError{ImageErrorCode::SDLFailure, std::string("Converting image for upload: ") + SDL_GetError()});
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
	const std::unique_ptr<SDL_Surface, decltype(&SDL_UnlockSurface)> unlock(lock ? surface : nullptr, SDL_UnlockSurface);
	const auto bytes = std::span<const std::byte>((const std::byte*)surface->pixels, (std::size_t)surface->pitch * surface->h);
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
