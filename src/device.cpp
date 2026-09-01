
//////
//
// Includes
//

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "FCG/window.h"
#include "FCG/device.h"



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
// Device

auto Device::create () -> std::optional<Device>
{
	// Create the GPU device. All common shader formats are offered so that SDL picks the best available
	// backend for the platform. Debug builds enable the GPU debug mode, which performs additional
	// validation of SDL GPU API usage – particularly useful in a teaching context.
	SDL_GPUDevice *handle = SDL_CreateGPUDevice(
		static_cast<SDL_GPUShaderFormat>(
			SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL
		),
	#ifdef NDEBUG
		false,
	#else
		true,
	#endif
		nullptr
	);
	if (!handle) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating the GPU device failed: %s", SDL_GetError());
		return std::nullopt;
	}

	// Assemble and return the object
	return std::make_optional<Device>(PrivateConstructorKey{}, handle);
}

Device::~Device ()
{
	if (m_handle)
	{
		// Wait for the GPU to finish all pending work before destroying the device
		SDL_WaitForGPUIdle(m_handle);

		// Release all window claims
		for (auto *window : m_claimedWindows)
			window->unclaim(*this);

		// Clean up
		SDL_DestroyGPUDevice(m_handle);
	}
}

auto Device::handle () const -> SDL_GPUDevice* {
	return m_handle;
}

void Device::waitIdle () const {
	SDL_WaitForGPUIdle(m_handle);
}

auto Device::claimWindow (std::unique_ptr<Window> &window) -> bool {
	if (!window->claim(*this))
		return false;
	m_claimedWindows.insert(window.get());
	return true;
}

void Device::unclaimWindow (std::unique_ptr<Window> &window) {
	window->unclaim(*this);
	m_claimedWindows.erase(window.get());
}


//////
//
// Module namespace close
//

} // namespace fcg
