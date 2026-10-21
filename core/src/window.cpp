
//////
//
// Includes
//

// C++ STL
#include <format>
#include <limits>
#include <memory>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "FCG/device.h"
#include "FCG/frame.h"
#include "FCG/window.h"
#include "window_scaling.h"



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// Window

Window::Window() = default;

auto Window::create (const WindowSettings &settings) -> std::unique_ptr<Window>
{
	if (!settings.width || !settings.height || settings.width > std::numeric_limits<int>::max()
		|| settings.height > std::numeric_limits<int>::max()) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window dimensions must be positive and fit in an int");
		return nullptr;
	}
	// Create the SDL window
	SDL_Window *handle = SDL_CreateWindow(
		settings.title.c_str(), (int)settings.width, (int)settings.height,
		(settings.resizable ? SDL_WINDOW_RESIZABLE : (SDL_WindowFlags)0)
			| SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
	);
	if (!handle) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating the window failed: %s", SDL_GetError());
		return nullptr;
	}

	// Assemble and return the object
	std::unique_ptr<Window> window(new Window());
	window->m_handle = handle;
	if (settings.scaleToDisplay)
		window->m_scaling = std::make_unique<detail::WindowScaling>(glm::ivec2(settings.width, settings.height));
	window->updateDisplayScale();
	if (!SDL_ShowWindow(handle)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Showing the window failed: %s", SDL_GetError());
		return nullptr;
	}
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

auto Window::swapchainFormat () const -> SDL_GPUTextureFormat
{
	if (!m_device)
		return SDL_GPU_TEXTUREFORMAT_INVALID;
	return SDL_GetGPUSwapchainTextureFormat(m_device->handle(), m_handle);
}

auto Window::renderTargetInfo () const -> std::optional<RenderTargetInfo> {
	const auto color = swapchainFormat();
	if (color == SDL_GPU_TEXTUREFORMAT_INVALID)
		return std::nullopt;
	return RenderTargetInfo{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB, depthFormat, samples};
}

auto Window::claim (Device &device) -> bool
{
	auto curClaim = m_device ? std::make_optional(m_device->handle()) : std::nullopt;
	if (curClaim)
	{
		const std::string msg = curClaim.value() == device.handle()
			? std::format(
				"Device {:p} tried claiming the window {:p} twice", (void*)device.handle(), (void*)m_handle
			)
			: std::format(
				"Device {:p} tried claiming the window {:p} while it is already claimed by device {:p}",
				(void*)device.handle(), (void*)m_handle, (void*)curClaim.value()
			);
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}
	if (!SDL_ClaimWindowForGPUDevice(device.handle(), m_handle)) {
		SDL_LogError(
			SDL_LOG_CATEGORY_ERROR, "Claiming the window for the GPU device failed: %s", SDL_GetError()
		);
		return false;
	}
	SDL_GPUSamplerCreateInfo samplerInfo{};
	samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
	samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
	samplerInfo.address_mode_u = samplerInfo.address_mode_v = samplerInfo.address_mode_w =
		SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	auto sampler = Sampler::create(device, samplerInfo);
	if (!sampler) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Creating the scene sampler failed: %s",
			sampler.error().message.c_str());
		SDL_ReleaseWindowFromGPUDevice(device.handle(), m_handle);
		return false;
	}
	presentationSampler = std::move(*sampler);
	m_device = &device;
	return true;
}

void Window::unclaim (Device &device)
{
	auto curClaim = m_device ? std::make_optional(m_device->handle()) : std::nullopt;
	if (!curClaim || curClaim.value() != device.handle())
	{
		const auto msg = std::format(
			"Trying to release nonexistent device claim to window {:p}:\nReleasing device: {:p}, actual"
			" claiming device: {:p}", (void*)m_handle, (void*)device.handle(), (void*)curClaim.value_or(nullptr)
		);
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}
	SDL_WaitForGPUIdle(device.handle());
	if (depthTexture) {
		SDL_ReleaseGPUTexture(device.handle(), depthTexture);
		depthTexture = nullptr;
		m_depthTextureSize = glm::uvec2(0);
	}
	sceneTexture.reset();
	presentationSampler.reset();
	SDL_ReleaseWindowFromGPUDevice(device.handle(), m_handle);
	m_device = nullptr;
}

auto Window::beginFrame (Device &device) -> Frame*
{
	// Make sure no frame is already in flight
	if (m_frame) {
		const auto msg = std::format(
			"Trying to start new frame on window {:p} while another one is still in flight", (void*)m_handle);
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}

	// Make sure the window is claimed for the device the frame should be rendered with
	auto curClaim = m_device ? std::make_optional(m_device->handle()) : std::nullopt;
	if (!curClaim || curClaim.value() != device.handle())  {
		const auto msg = std::format(
			"Trying to start new frame on unclaimed window {:p}", (void*)m_handle
		);
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}

	updateDisplayScale();

	// Acquire (and potentially wait for) the swapchain texture and central command buffer for this frame
	SDL_GPUCommandBuffer *cmdBuffer = SDL_AcquireGPUCommandBuffer(device.handle());
	if (!cmdBuffer) {
		const auto msg = std::format("Acquiring a GPU command buffer failed: {}", SDL_GetError());
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
	SDL_GPUTexture *swapchainTexture = nullptr;
	// Cancel until an image is acquired; afterwards, submit even when attachment creation throws.
	const auto finishAcquisition = [&swapchainTexture] (SDL_GPUCommandBuffer *command) noexcept {
		const bool acquired = swapchainTexture != nullptr;
		if (!(acquired ? SDL_SubmitGPUCommandBuffer(command) : SDL_CancelGPUCommandBuffer(command)))
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cleaning up frame acquisition by %s failed: %s",
				acquired ? "submission" : "cancellation", SDL_GetError());
	};
	std::unique_ptr<SDL_GPUCommandBuffer, decltype(finishAcquisition)> acquisition(cmdBuffer, finishAcquisition);
	auto swapchainSize = glm::uvec2(0);
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(
		cmdBuffer, m_handle, &swapchainTexture, &swapchainSize.x,
		&swapchainSize.y
	)){
		const auto msg = std::format("Acquiring the swapchain texture failed: {}", SDL_GetError());
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// With no image, discard the unused commands and report ordinary unavailability only after successful cleanup.
	if (!swapchainTexture)
	{
		if (!SDL_CancelGPUCommandBuffer(acquisition.release())) {
			const auto msg = std::format("Cancelling the unused frame command buffer failed: {}", SDL_GetError());
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
		return nullptr;
	}

	// Make sure we have a depth buffer matching the swapchain texture in size. It only gets recreated when the size
	// actually changed (e.g. after a window resize).
	if (!depthTexture || !sceneTexture || swapchainSize != m_depthTextureSize)
	{
		// Releasing the old depth buffer is safe even if previously submitted frames are still using it, SDL defers
		// destruction until the GPU is done with it
		if (depthTexture)
			SDL_ReleaseGPUTexture(device.handle(), depthTexture);

		// Create the new depth buffer with the most common defaults: 32-bit float depth, no stencil, no MSAA
		SDL_GPUTextureCreateInfo depthTextureInfo = { };
		depthTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
		depthTextureInfo.format = depthFormat;
		depthTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
		depthTextureInfo.width = swapchainSize.x;
		depthTextureInfo.height = swapchainSize.y;
		depthTextureInfo.layer_count_or_depth = 1;
		depthTextureInfo.num_levels = 1;
		depthTextureInfo.sample_count = samples;
		depthTexture = SDL_CreateGPUTexture(device.handle(), &depthTextureInfo);
		if (!depthTexture) {
			const auto msg = std::format("Creating the depth buffer failed: {}", SDL_GetError());
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
		SDL_GPUTextureCreateInfo sceneInfo{};
		sceneInfo.type = SDL_GPU_TEXTURETYPE_2D;
		sceneInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
		sceneInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
		sceneInfo.width = swapchainSize.x;
		sceneInfo.height = swapchainSize.y;
		sceneInfo.layer_count_or_depth = sceneInfo.num_levels = 1;
		sceneInfo.sample_count = samples;
		auto scene = Texture::create(device, sceneInfo);
		if (!scene)
			throw std::runtime_error("Creating scene attachment: " + scene.error().message);
		sceneTexture.emplace(std::move(*scene));
		m_depthTextureSize = swapchainSize;
	}

	// The viewport dimensions always reflect the size of the most recently acquired swapchain texture
	m_viewportSize = swapchainSize;

	// Begin frame and return
	m_frame.emplace(
		Frame::PrivateConstructorKey{}, cmdBuffer, swapchainTexture, depthTexture, *sceneTexture,
		device, swapchainFormat(), *presentationSampler, swapchainSize
	);
	auto _notOwnedAnyLonger = acquisition.release();
	return &m_frame.value();
}

void Window::endFrame ()
{
	// Make sure a frame was actually in flight
	if (!m_frame) {
		const auto msg = std::format(
			"Trying to end a frame on window {:p} which currently has no frames in flight", (void*)m_handle
		);
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::logic_error(msg);
	}

	// Handle frame finalization
	try {
		m_frame->end();
	} catch (...) {
		if (!m_frame->commandBuffer())
			m_frame.reset();
		throw;
	}

	// The frame landed
	m_frame.reset();
}

void Window::updateDisplayScale ()
{
	if (!m_scaling)
		return;
	glm::ivec2 size;
	if (!SDL_GetWindowSize(m_handle, &size.x, &size.y))
		return;
	const bool suspended = SDL_GetWindowFlags(m_handle)
		& (SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN);
	m_scaling->refresh(
		SDL_GetWindowDisplayScale(m_handle), SDL_GetWindowPixelDensity(m_handle), size, suspended,
		[this, size] (glm::ivec2 requested)
		{
			if (!SDL_SetWindowSize(m_handle, requested.x, requested.y))
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Scaling the window failed: %s", SDL_GetError());
			else if (!SDL_SyncWindow(m_handle))
				SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Waiting for the scaled window timed out: %s", SDL_GetError());
			glm::ivec2 actual = size;
			if (!SDL_GetWindowSize(m_handle, &actual.x, &actual.y))
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Querying the scaled window size failed: %s", SDL_GetError());
			return actual;
		}
	);
}

auto Window::pollViewportSize (std::optional<glm::uvec2> &oldSize) -> bool
{
	updateDisplayScale();

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
