
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "FCG/device.h"
#include "FCG/buffer.h"
#include "FCG/render_state.h"



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
// RenderState

RenderState::RenderState(Device& /* right now we don't need access to the device yet */) {}

RenderState::~RenderState() = default;

void RenderState::pushViewingUniforms (SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, uint32_t slot)
{
	if (auto result = pushUniforms(commandBuffer, stage, slot, viewingUniforms()); !result)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Pushing viewing uniforms: %s", result.error().message.c_str());
}

void RenderState::pushViewingUniforms (SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, uint32_t slot) const
{
	if (auto result = pushUniforms(commandBuffer, stage, slot, viewingUniforms()); !result)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Pushing viewing uniforms: %s", result.error().message.c_str());
}



//////
//
// Module namespace close
//

} // namespace fcg
