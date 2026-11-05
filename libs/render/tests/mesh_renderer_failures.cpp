
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// Framework
#include <FCG/Render/mesh_renderer.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Index of the pipeline allocation to fail.
unsigned reject = 0;

/// Number of pipeline creation attempts.
unsigned created = 0;

/// Number of owned pipelines released.
unsigned released = 0;

/// Forward genuine allocation except at the selected boundary.
auto createPipeline (SDL_GPUDevice *device, const SDL_GPUGraphicsPipelineCreateInfo *info) -> SDL_GPUGraphicsPipeline* {
	if (++created == reject) { SDL_SetError("Injected pipeline failure"); return nullptr; }
	return SDL_CreateGPUGraphicsPipeline(device, info);
}

/// Count releases while forwarding production ownership cleanup.
void releasePipeline (SDL_GPUDevice *device, SDL_GPUGraphicsPipeline *pipeline) {
	++released;
	SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
}

// Anonymous namespace end
}



//////
//
// Includes
//

// Test production pipeline creation with injected allocation/release boundaries
#define SDL_CreateGPUGraphicsPipeline createPipeline
#define SDL_ReleaseGPUGraphicsPipeline releasePipeline
#include "../src/mesh_renderer.cpp"
#undef SDL_ReleaseGPUGraphicsPipeline
#undef SDL_CreateGPUGraphicsPipeline



//////
//
// Functions
//

/// Verify pipeline failure cleanup and renderer initialization before a mesh exists.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
	try {
		auto device = fcg::Device::create();
		if (!device) throw std::runtime_error(SDL_GetError());
		const fcg::RenderTargetInfo target{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
		for (unsigned failure : {1u, 2u}) {
			reject = failure;
			created = released = 0;
			if (fcg::MeshRenderer::create(*device, target, {.texCoords=true}))
				throw std::runtime_error("Pipeline failure published renderer");
			if (released != failure - 1) throw std::runtime_error("Pipeline failure leaked prior allocations");
		}
		reject = created = released = 0;
		/* Complete renderer lifetime */ {
			auto renderer = fcg::MeshRenderer::create(*device, target, {.texCoords=true});
			if (!renderer) throw std::runtime_error(renderer.error().message);
		}
		if (created != 2 || released != 2) throw std::runtime_error("Renderer leaked pipelines");
	} catch (const std::exception &error) { std::cerr << error.what() << '\n'; SDL_Quit(); return 1; }
	SDL_Quit();
	return 0;
}
