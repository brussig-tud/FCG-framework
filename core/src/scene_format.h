
#ifndef __FCG_INTERNAL_SCENE_FORMAT_H__
#define __FCG_INTERNAL_SCENE_FORMAT_H__


//////
//
// Includes
//

// C++ STL
#include <expected>

// Local includes
#include "FCG/device.h"
#include "FCG/texture.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Implementation helpers shared by the window and runtime.
namespace detail {



//////
//
// Functions
//

/// Validate a scene format, querying combined usage and sample support when a device is available.
[[nodiscard]] inline auto validateSceneFormat (Device *device, SDL_GPUTextureFormat format)
	-> std::expected<void, TextureError>
{
	// Sanity checks
	if (format <= SDL_GPU_TEXTUREFORMAT_INVALID || format > SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT)
		return std::unexpected(TextureError{TextureErrorCode::InvalidArgument, "Invalid scene texture format"});
	if ((format >= SDL_GPU_TEXTUREFORMAT_D16_UNORM && format <= SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT)
		|| (format >= SDL_GPU_TEXTUREFORMAT_R8_UINT && format <= SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT))
		return std::unexpected(TextureError{TextureErrorCode::UnsupportedFormat,
			"Scene textures require a non-integer color format"});

	// Check device support
	if (device && (
	       !SDL_GPUTextureSupportsFormat(
	       	device->handle(), format, SDL_GPU_TEXTURETYPE_2D,
	       	SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER
	       )
	    || !SDL_GPUTextureSupportsSampleCount(device->handle(), format, SDL_GPU_SAMPLECOUNT_1)))
		return std::unexpected(TextureError{
			TextureErrorCode::UnsupportedFormat, "Scene format does not support single-sample 2D"
			" color-target and sampler usage"
		});

	// Success
	return {};
}



//////
//
// Namespaces close
//

} // namespace detail

} // namespace fcg


#endif // ifndef __FCG_INTERNAL_SCENE_FORMAT_H__
