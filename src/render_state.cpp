
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
	switch (stage)
	{
		case ShaderStage::VERTEX:
			SDL_PushGPUVertexUniformData(commandBuffer, slot, &modelviewProjectionMatrix(), sizeof(glm::mat4));
			//SDL_PushGPUVertexUniformData(commandBuffer, slot, &viewingUniforms(), sizeof(ViewingUniforms));
			break;
		case ShaderStage::FRAGMENT:
			SDL_PushGPUFragmentUniformData(commandBuffer, slot, &viewingUniforms(), sizeof(ViewingUniforms));
			break;
		case ShaderStage::COMPUTE:
			SDL_PushGPUComputeUniformData(commandBuffer, slot, &viewingUniforms(), sizeof(ViewingUniforms));
			break;
	}
}

void RenderState::pushViewingUniforms (SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, uint32_t slot) const
{
	switch (stage)
	{
		case ShaderStage::VERTEX:
			SDL_PushGPUVertexUniformData(commandBuffer, slot, &viewingUniforms(), sizeof(ViewingUniforms));
			break;
		case ShaderStage::FRAGMENT:
			SDL_PushGPUFragmentUniformData(commandBuffer, slot, &viewingUniforms(), sizeof(ViewingUniforms));
			break;
		case ShaderStage::COMPUTE:
			SDL_PushGPUComputeUniformData(commandBuffer, slot, &viewingUniforms(), sizeof(ViewingUniforms));
			break;
	}
}



//////
//
// Module namespace close
//

} // namespace fcg
