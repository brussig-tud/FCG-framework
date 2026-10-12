
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

/// \addtogroup fcg_windows
/// @{



//////
//
// Structs and enums
//

/// \brief
/// 	The attachment formats and sample count of a render target.
///
/// Use it to create graphics pipelines that are compatible with a render target, for example with the main window
/// as reported by <tt>\ref fcg::Window::renderTargetInfo "Window::renderTargetInfo"</tt> or
/// <tt>\ref fcg::Player::mainRenderTargetInfo "Player::mainRenderTargetInfo"</tt>. The values do not depend on the
/// size of the target, so they stay valid when a window is resized.
struct RenderTargetInfo
{
	/// The format of the single color attachment.
	SDL_GPUTextureFormat colorFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

	/// The format of the depth/stencil attachment, or \c SDL_GPU_TEXTUREFORMAT_INVALID if there is none.
	SDL_GPUTextureFormat depthStencilFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

	/// The sample count shared by all attachments.
	SDL_GPUSampleCount sampleCount = SDL_GPU_SAMPLECOUNT_1;
};



/// @}



//////
//
// Namespaces close
//

} // namespace fcg


#endif // ifndef __FCG_RENDER_TARGET_H__
