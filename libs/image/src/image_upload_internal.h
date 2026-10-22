
#ifndef __FCG_IMAGE_UPLOAD_INTERNAL_H__
#define __FCG_IMAGE_UPLOAD_INTERNAL_H__


//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <optional>

// Local includes
#include "FCG/Image/image.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Image upload representation selection shared with capability tests.
namespace detail {



//////
//
// Structs and enums
//

/// CPU and GPU representations for a numeric-sample-preserving upload.
struct StoredImageLayout
{
	/// CPU storage supplied to the transfer.
	SDL_PixelFormat cpu = SDL_PIXELFORMAT_RGBA128_FLOAT;

	/// Sampled GPU storage without automatic transfer decoding.
	SDL_GPUTextureFormat gpu = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
};



//////
//
// Functions
//

/// Select native precision or widening using the same capability predicate as the actual GPU upload.
template<class Supports>
[[nodiscard]] auto storedImageLayout (const PixelFormatInfo &source, Supports supports) -> std::optional<StoredImageLayout>
{
	if (source.storage != PixelStorage::Packed && source.storage != PixelStorage::Interleaved
		&& source.storage != PixelStorage::Indexed)
		return std::nullopt;
	StoredImageLayout result;
	const bool eight = source.storage == PixelStorage::Indexed
		|| std::ranges::all_of(source.channelBits, [] (unsigned bits) { return bits == 0 || bits == 8; });
	if (eight) {
		result.cpu = source.format == SDL_PIXELFORMAT_BGRA32 ? SDL_PIXELFORMAT_BGRA32 : SDL_PIXELFORMAT_RGBA32;
		result.gpu = result.cpu == SDL_PIXELFORMAT_BGRA32
			? SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	}
	else if (source.representation == PixelRepresentation::UnsignedInteger && source.channelBits[0] == 16) {
		result.cpu = SDL_PIXELFORMAT_RGBA64;
		result.gpu = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM;
	}
	else if (source.representation == PixelRepresentation::Binary16) {
		result.cpu = SDL_PIXELFORMAT_RGBA64_FLOAT;
		result.gpu = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
	}
	if (!supports(result.gpu))
		result = StoredImageLayout{};
	return supports(result.gpu) ? std::optional(result) : std::nullopt;
}



//////
//
// Namespaces close
//

// namespace detail
}

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_UPLOAD_INTERNAL_H__
