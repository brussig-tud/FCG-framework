
#ifndef __FCG_RENDER_TARGET_H__
#define __FCG_RENDER_TARGET_H__


//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// SDL3 library
#include <SDL3/SDL_gpu.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Structs and enums
//

/// \ingroup fcg_windows
/// Pipeline attachment formats and sample count, independent of viewport dimensions.
struct RenderTargetInfo
{
	////
	// Fields

	/// The single color attachment format.
	SDL_GPUTextureFormat colorFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

	/// Depth/stencil attachment format; \c SDL_GPU_TEXTUREFORMAT_INVALID means no attachment.
	SDL_GPUTextureFormat depthStencilFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

	/// Sample count shared by the attachments.
	SDL_GPUSampleCount sampleCount = SDL_GPU_SAMPLECOUNT_1;
};



//////
//
// Namespaces close
//

} // namespace fcg


#endif // ifndef __FCG_RENDER_TARGET_H__
