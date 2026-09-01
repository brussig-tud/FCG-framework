
//////
//
// Includes
//

// C++ STL
#include <sstream>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "FCG/frame.h"



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
// Frame

Frame::~Frame()
{
	if (m_commandBuffer || m_targetTexture)
	{
		// This is a logic bug
		std::stringstream msgstream;
		msgstream << "Frame is being destroyed while in flight";
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, msg.c_str());

		// Unrecoverable
		exit(EXIT_FAILURE);
	}
}

void Frame::end ()
{
	// Can't end the frame if a render pass is being recorded
	if (m_renderPass) {
		std::stringstream msgstream;
		msgstream << "Frame is being ended while a render pass is being recorded";
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, msg.c_str());
		throw std::logic_error(msg);
	}

	// Submit the frame's command buffer, which also presents the swapchain texture
	if (!SDL_SubmitGPUCommandBuffer(m_commandBuffer))
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Submitting the GPU command buffer failed: %s", SDL_GetError());
	m_commandBuffer = nullptr;
	m_targetTexture = nullptr;
}

auto Frame::beginRenderPass (const glm::fvec4 &clearColor) -> SDL_GPURenderPass*
{
	// Don't begin another render pass while one is still active
	if (m_renderPass) {
		std::stringstream msgstream;
		msgstream << "Trying to begin a new frame render pass to the frame target while one is already being recorded";
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, msg.c_str());
		throw std::logic_error(msg);
	}

	// Set up the render pass targeting the current swapchain texture, with a depth buffer using the most common
	// defaults: cleared to the far plane and no stencil testing.
	SDL_GPUColorTargetInfo colorTarget = { };
	colorTarget.texture = m_targetTexture;
	colorTarget.clear_color = { clearColor.r, clearColor.g, clearColor.b, clearColor.a };
	colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
	colorTarget.store_op = SDL_GPU_STOREOP_STORE;
	SDL_GPUDepthStencilTargetInfo depthStencilTarget = { };
	depthStencilTarget.texture = m_depthTexture;
	depthStencilTarget.clear_depth = 1.f;
	depthStencilTarget.load_op = SDL_GPU_LOADOP_CLEAR;
	depthStencilTarget.store_op = SDL_GPU_STOREOP_STORE;
	depthStencilTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
	depthStencilTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
	m_renderPass = SDL_BeginGPURenderPass(m_commandBuffer, &colorTarget, 1, &depthStencilTarget);
	if (!m_renderPass)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Beginning the frame render pass failed: %s", SDL_GetError());
	return m_renderPass;
}

void Frame::endRenderPass ()
{
	// Can't end the current render pass if there is none
	if (!m_renderPass) {
		std::stringstream msgstream;
		msgstream << "Trying to end a frame render pass when none is active";
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, msg.c_str());
		throw std::logic_error(msg);
	}
	SDL_EndGPURenderPass(m_renderPass);
	m_renderPass = nullptr;
}




//////
//
// Module namespace close
//

} // namespace fcg
