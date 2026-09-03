
//////
//
// Includes
//

// C++ STL
#include <sstream>

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "FCG/device.h"
#include "FCG/frame.h"
#include "FCG/window.h"


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
// Window

auto Window::create (const WindowSettings &settings) -> std::unique_ptr<Window>
{
	// Create the SDL window
	SDL_Window *handle = SDL_CreateWindow(
		settings.title.c_str(), (int)settings.width, (int)settings.height,
		(settings.resizable ? SDL_WINDOW_RESIZABLE : (SDL_WindowFlags)0) | SDL_WINDOW_HIGH_PIXEL_DENSITY
	);
	if (!handle) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating the window failed: %s", SDL_GetError());
		return nullptr;
	}

	// Assemble and return the object
	std::unique_ptr<Window> window(new Window());
	window->m_handle = handle;
	return window;
}

Window::~Window ()
{
	// Windows must always outlive devices that claim them.
	if (m_device) {
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Window %p destroyed while claimed by a device", (void*)m_handle);
		exit(EXIT_FAILURE); // unrecoverable
	}

	// Clean up
	if (m_handle)
		SDL_DestroyWindow(m_handle);
}

auto Window::handle () const -> SDL_Window* {
	return m_handle;
}

auto Window::id () const -> unsigned {
	return SDL_GetWindowID(m_handle);
}

auto Window::claim (Device &device) -> bool
{
	auto curClaim = m_device ? std::make_optional(m_device->handle()) : std::nullopt;
	if (curClaim) {
		std::stringstream msgstream;
		if (curClaim.value() == device.handle())
			msgstream << "Device "<<std::hex<<device.handle()<<" tried claiming the window "<<m_handle<<" twice";
		else
			msgstream << "Device "<<std::hex<<device.handle()<<" tried claiming the window "<<m_handle<<" while it is"
		                 " already claimed by device "<<curClaim.value();
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}
	if (!SDL_ClaimWindowForGPUDevice(device.handle(), m_handle)) {
		SDL_LogError(
			SDL_LOG_CATEGORY_ERROR, "Claiming the window for the GPU device failed: %s", SDL_GetError()
		);
		return false;
	}
	m_device = &device;
	return true;
}

void Window::unclaim (Device &device)
{
	auto curClaim = m_device ? std::make_optional(m_device->handle()) : std::nullopt;
	if (!curClaim || curClaim.value() != device.handle())
	{
		std::stringstream msgstream;
		msgstream << "Trying to release nonexistent device claim to window "<<std::hex<<m_handle<<":" << std::endl
		          << "Releasing device: "<<device.handle()<<", actual claiming device: "<<curClaim.value_or(nullptr);
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}
	SDL_WaitForGPUIdle(device.handle());
	if (depthTexture) {
		SDL_ReleaseGPUTexture(device.handle(), depthTexture);
		depthTexture = nullptr;
		m_depthTextureSize = glm::uvec2(0);
	}
	SDL_ReleaseWindowFromGPUDevice(device.handle(), m_handle);
	m_device = nullptr;
}

auto Window::beginFrame (Device &device) -> Frame*
{
	// Make sure no frame is already in flight
	if (m_frame) {
		std::stringstream msgstream;
		msgstream << "Trying to start new frame on window "<<std::hex<<m_handle<<" while another one is still in"
		             " flight";
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}

	// Make sure the window is claimed for the device the frame should be rendered with
	auto curClaim = m_device ? std::make_optional(m_device->handle()) : std::nullopt;
	if (!curClaim || curClaim.value() != device.handle())  {
		std::stringstream msgstream;
		msgstream << "Trying to start new frame on unclaimed window "<<std::hex<<m_handle;
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}

	// Acquire (and potentially wait for) the swapchain texture and central command buffer for this frame
	SDL_GPUCommandBuffer *cmdBuffer = SDL_AcquireGPUCommandBuffer(device.handle());
	if (!cmdBuffer) {
		std::stringstream msgstream;
		msgstream << "Acquiring a GPU command buffer failed: "<<SDL_GetError();
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	SDL_GPUTexture *swapchainTexture = nullptr;
	auto swapchainSize = glm::uvec2(0);
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(
		cmdBuffer, m_handle, &swapchainTexture, &swapchainSize.x,
		&swapchainSize.y
	)){
		std::stringstream msgstream;
		msgstream << "Acquiring the swapchain texture failed: "<<SDL_GetError();
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// A missing swapchain texture (e.g. because the window is minimized) is not an error, but the command buffer must
	// still be submitted. Nothing can be rendered this frame though.
	if (!swapchainTexture) {
		SDL_SubmitGPUCommandBuffer(cmdBuffer);
		return nullptr;
	}

	// Make sure we have a depth buffer matching the swapchain texture in size. It only gets recreated when the size
	// actually changed (e.g. after a window resize).
	if (!depthTexture || swapchainSize != m_depthTextureSize)
	{
		// Releasing the old depth buffer is safe even if previously submitted frames are still using it, SDL defers
		// destruction until the GPU is done with it
		if (depthTexture)
			SDL_ReleaseGPUTexture(device.handle(), depthTexture);

		// Create the new depth buffer with the most common defaults: 32-bit float depth, no stencil, no MSAA
		SDL_GPUTextureCreateInfo depthTextureInfo = { };
		depthTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
		depthTextureInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
		depthTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
		depthTextureInfo.width = swapchainSize.x;
		depthTextureInfo.height = swapchainSize.y;
		depthTextureInfo.layer_count_or_depth = 1;
		depthTextureInfo.num_levels = 1;
		depthTextureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
		depthTexture = SDL_CreateGPUTexture(device.handle(), &depthTextureInfo);
		if (!depthTexture) {
			std::stringstream msgstream;
			msgstream << "Creating the depth buffer failed: "<<SDL_GetError();
			auto msg = msgstream.str();
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
		m_depthTextureSize = swapchainSize;
	}

	// The viewport dimensions always reflect the size of the most recently acquired swapchain texture
	m_viewportSize = swapchainSize;

	// Begin frame and return
	m_frame.emplace(Frame::PrivateConstructorKey{}, cmdBuffer, swapchainTexture, depthTexture);
	return &m_frame.value();
}

void Window::endFrame ()
{
	// Make sure a frame was actually in flight
	if (!m_frame) {
		std::stringstream msgstream;
		msgstream << "Trying to end a frame on window "<<std::hex<<m_handle<<" which currently has no frames in flight";
		auto msg = msgstream.str();
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}

	// Handle frame finalization
	m_frame->end();

	// The frame landed
	m_frame.reset();
}

auto Window::pollViewportSize (std::optional<glm::uvec2> &oldSize) -> bool
{
	// Query the current drawable size – the size in pixels is what matches the swapchain and thus the viewport
	glm::uvec2 newSize(0);
	if (!SDL_GetWindowSizeInPixels(m_handle, (int*)&newSize.x, (int*)&newSize.y)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Querying the window size in pixels failed: %s", SDL_GetError());
		oldSize.reset();
		return false;
	}

	// Update on change only
	if (newSize == m_viewportSize) {
		oldSize.reset();
		return false;
	}
	oldSize = m_viewportSize;
	m_viewportSize = newSize;
	return true;
}

void Window::setTitle (const std::string &title) {
	if (!SDL_SetWindowTitle(m_handle, title.c_str()))
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Setting the window title failed: %s", SDL_GetError());
}



//////
//
// Module namespace close
//

} // namespace fcg
