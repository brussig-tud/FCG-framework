
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/device.h>
#include <FCG/window.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Whether the next color allocation should fail.
bool failColor = false;

/// Whether the next depth allocation should fail.
bool failDepth = false;

/// Attachment handles whose ownership has not been released.
std::unordered_set<SDL_GPUTexture*> owned;

/// Whether every release referred to an owned texture.
bool validOwnership = true;

/// Preserve test diagnostics.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Inject color and depth allocation failures independently.
auto createAttachment (SDL_GPUDevice *device, const SDL_GPUTextureCreateInfo *info) -> SDL_GPUTexture*
{
	const bool depth = info->usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	if (std::exchange(depth ? failDepth : failColor, false)) {
		SDL_SetError("attachment retry sentinel");
		return nullptr;
	}
	auto *texture = SDL_CreateGPUTexture(device, info);
	if (texture)
		owned.insert(texture);
	return texture;
}

/// Observe ownership transitions, including teardown after failed acquisition.
void releaseAttachment (SDL_GPUDevice *device, SDL_GPUTexture *texture) {
	validOwnership &= owned.erase(texture) == 1;
	SDL_ReleaseGPUTexture(device, texture);
}

// Anonymous namespace end
}



//////
//
// Includes
//

// Substitute the allocation boundary in production code without public test hooks.
#define SDL_CreateGPUTexture createAttachment
#define SDL_ReleaseGPUTexture releaseAttachment
#include "../../src/texture.cpp"
#include "../../src/window.cpp"
#undef SDL_ReleaseGPUTexture
#undef SDL_CreateGPUTexture
#include "../../src/device.cpp"



//////
//
// Functions
//

/// Verify immediate configuration, allocation retry, depth preservation and attachment ownership.
auto main () -> int
{
	try
	{
		require(SDL_Init(SDL_INIT_VIDEO), "SDL initialization failed");
		const auto quit = [] (void*) { SDL_Quit(); };
		const std::unique_ptr<void, decltype(quit)> sdl(&failColor, quit);
		auto device = fcg::Device::create();
		require(device.has_value(), "Device creation failed");
		{
			auto window = fcg::Window::create({.width = 320, .height = 240});
			require(window && device->claimWindow(window), "Window creation or claim failed");
			const auto unclaim = [&] (fcg::Window*) { device->unclaimWindow(window); };
			const std::unique_ptr<fcg::Window, decltype(unclaim)> claim(window.get(), unclaim);
			auto *frame = window->beginFrame(*device);
			require(frame, "Initial acquisition unavailable");
			auto *originalDepth = frame->depthTexture();
			require(owned.size() == 2, "Window did not own two independent attachments");
			bool rejected = false;
			try { (void)window->setSceneFormat(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM); }
			catch (const std::logic_error&) { rejected = true; }
			require(rejected, "Window accepted active-frame format mutation");
			window->endFrame();

			for (const auto format : {SDL_GPU_TEXTUREFORMAT_INVALID, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
				SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT, SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM})
				require(!window->setSceneFormat(format), "Window accepted an unsuitable scene format");
			require(window->renderTargetInfo()->colorFormat == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
				&& owned.size() == 2, "Rejected configuration changed storage");

			require(window->setSceneFormat(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT).has_value(),
				"Window rejected float scene storage");
			require(owned.size() == 1 && owned.contains(originalDepth), "Format change released depth storage");
			failColor = true;
			bool failed = false;
			try { (void)window->beginFrame(*device); }
			catch (const std::runtime_error&) { failed = true; }
			require(failed && owned.size() == 1 && window->renderTargetInfo()->colorFormat
				== SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, "Color failure lost configuration or depth ownership");
			frame = window->beginFrame(*device);
			require(frame && frame->depthTexture() == originalDepth && frame->sceneTarget().info().format
				== SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, "Color allocation retry changed depth or format");
			window->endFrame();

			require(SDL_SetWindowSize(window->handle(), 480, 300) && SDL_SyncWindow(window->handle()), "Resize failed");
			failDepth = true;
			failed = false;
			try { (void)window->beginFrame(*device); }
			catch (const std::runtime_error&) { failed = true; }
			require(failed && owned.size() == 2 && owned.contains(originalDepth), "Depth failure lost old ownership");
			failColor = true;
			failed = false;
			try { (void)window->beginFrame(*device); }
			catch (const std::runtime_error&) { failed = true; }
			require(failed && owned.size() == 2 && !owned.contains(originalDepth), "Color failure lost resized depth");
			frame = window->beginFrame(*device);
			require(frame && frame->extent() == glm::uvec2(480, 300), "Resized allocation retry failed");
			window->endFrame();
		}
		require(validOwnership && owned.empty(), "Attachments were released twice or leaked");
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
