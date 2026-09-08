
//////
//
// Includes
//

// C++ STL
#include <format>
#include <variant>

// SDL3
#include <SDL3/SDL.h>

// Local includes
#include "FCG/player.h"
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
// Player

template <class Texel> requires (sizeof(Texel) > 0)
Player::ReadbackState<Texel>::~ReadbackState() {
	if (std::holds_alternative<SDL_GPUFence*>(state)) {
		auto fence = std::get<SDL_GPUFence*>(state);
		if (fence)
			SDL_ReleaseGPUFence(device.handle(), fence);
	}
}

template <class Texel> requires (sizeof(Texel) > 0)
void Player::ReadbackState<Texel>::transitionToMapped (SDL_GPUTransferBuffer* buffer)
{
	// Sanity check
	assert(std::holds_alternative<SDL_GPUFence*>(state));

	// Wait for the GPU to finish the readback operation
	auto fence = std::get<SDL_GPUFence*>(state);
	if (fence) {
		SDL_WaitForGPUFences(device.handle(), true, &fence, 1);
		SDL_ReleaseGPUFence(device.handle(), fence);
	}

	// Transition into mapped state
	state.template emplace<OwningTextureView<Texel, 2>>(device, buffer, extent, stride);
}

Player::Player (Device &device, Window *mainWindow) : device(device), m_window(mainWindow) {
	// Nothing else to do here yet.
}

Player::~Player() {
	if (depthReadbackBuffer) {
		SDL_ReleaseGPUTransferBuffer(device.handle(), depthReadbackBuffer);
		depthReadbackBuffer = nullptr;
	}
}

void Player::setWindowTitle (const std::string &title)
{
	// A player that was not created by fcg::run has no main window to talk to
	if (!m_window) {
		SDL_LogWarn(
			SDL_LOG_CATEGORY_APPLICATION,
			"fcg::Player::setWindowTitle() called on a player that is not connected to a main window"
		);
		return;
	}
	m_window->setTitle(title);
}

void Player::pushContinuousRedraw () {
	if (m_numContinuousRedrawRequests == 0)
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "fcg::Player: starting continuous redraw");
	++m_numContinuousRedrawRequests;
}

void Player::popContinuousRedraw ()
{
	// Clamp at zero – an unbalanced pop is a bug in the calling applet, but not fatal
	if (m_numContinuousRedrawRequests < 1) {
		SDL_LogWarn(
			SDL_LOG_CATEGORY_APPLICATION,
			"fcg::Player::popContinuousRedraw() called without a matching pushContinuousRedraw()"
		);
		return;
	}
	--m_numContinuousRedrawRequests;
	if (m_numContinuousRedrawRequests == 0)
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "fcg::Player: stopping continuous redraw");
}

auto Player::continuousRedrawRequested () const -> bool {
	return m_numContinuousRedrawRequests > 0;
}

void Player::requestClose ()
{
	m_closeRequested.store(true);

	if (!m_window) {
		// A player without a window still records the request so callers can observe it via shouldClose().
		return;
	}
	m_window->requestClose();
}

auto Player::shouldClose () const -> bool
{
	if (m_closeRequested.load())
		return true;

	return m_window && m_window->shouldClose();
}

auto Player::swapchainFormat () const -> SDL_GPUTextureFormat {
	/// If the player has no main window or that window is not claimed by a
	/// device an \c SDL_GPU_TEXTUREFORMAT_INVALID is returned as sentinel value.
	return m_window ? m_window->swapchainFormat() : SDL_GPU_TEXTUREFORMAT_INVALID;
}

auto Player::viewportSize() const -> glm::uvec2 {
	return m_window ? m_window->viewportSize() : glm::uvec2(0);
}

[[nodiscard]] auto Player::scheduleDepthReadback () -> uint64_t
{
	// Handle existing readback operation
	if (depthReadback)
	{
		auto &rb = depthReadback.value();
		if (readbackToken == rb.token)
			// Readback for this frame has already been scheduled, return the same token
			return readbackToken;
		if (std::holds_alternative<SDL_GPUFence*>(rb.state))
		{
			SDL_ReleaseGPUFence(device.handle(), std::get<SDL_GPUFence*>(rb.state));
			const auto msg = std::format(
				"Player: orphaned depth readback operation detected while scheduling a new one\n"
				"  Current token: {} - orphaned token: {}", readbackToken, rb.token
			);
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
			depthReadback.reset();
			throw std::logic_error(msg);
		}
		// The previous readback operation was completed and can be overwritten
		depthReadback.reset();
	}

	////
	// Dispatch the readback

	// Start copy pass
	SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(device.handle());
	SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);
	const auto extent = viewportSize();
	const SDL_GPUTextureRegion source {
		.texture = frame->depthTexture(), .mip_level = 0, .layer = 0, .x = 0, .y = 0, .z = 0,
		.w = extent.x, .h = extent.y, .d = 1
	};

	// Describe copy geometry
	SDL_GPUTextureTransferInfo destination {
		.transfer_buffer = depthReadbackBuffer, .offset = 0, .pixels_per_row = extent.x, .rows_per_layer = extent.y
	};
	SDL_DownloadFromGPUTexture(copyPass, &source, &destination);
	SDL_EndGPUCopyPass(copyPass);

	// Submit and obtain a fence.
	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuf);
	depthReadback.emplace(device, extent, glm::vec2(1, extent.x), fence, readbackToken);

	// Done!
	return readbackToken;
}

[[nodiscard]] auto Player::getDepthReadbackResult (uint64_t token) -> TextureView<float>
{
	if (depthReadback)
	{
		auto &rb = depthReadback.value();
		if (token == rb.token) {
			// The requested readback operation is still pending, wait for it to complete
			if (std::holds_alternative<SDL_GPUFence*>(rb.state))
				rb.transitionToMapped(depthReadbackBuffer);
			return std::get<OwningTextureView<float, 2>>(rb.state);
		}
		else {
			// The requested readback operation has not yet been scheduled
			const auto msg = std::format(
				"Player: depth readback result queried with invalid token\n"
				"  Requesting token: {}, current token: {}", token, rb.token
			);
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
			throw std::runtime_error(msg);
		}
	}
	// The requested readback operation has not yet been scheduled
	constexpr auto msg = "Player: depth readback result queried but no readback was ever scheduled";
	SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, msg);
	throw std::logic_error(msg);
}

void Player::recreateDepthReadbackBuffer ()
{
	// Destroy old buffer if it exists
	if (depthReadbackBuffer) {
		depthReadback.reset(); // any mapping, if it exists, will not be valid anymore
		SDL_ReleaseGPUTransferBuffer(device.handle(), depthReadbackBuffer);
		depthReadbackBuffer = nullptr;
	}

	// Query viewport size. FIXME: does not take into account headless players with no main window.
	const auto vpSize = m_window->viewportSize();

	// Determine buffer geometry
	SDL_GPUTransferBufferCreateInfo bi {
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
		.size = (Uint32)(vpSize.x * vpSize.y * sizeof(float)) // FIXME: Assumes the hardcoded D32_FLOAT depth format
	};

	// Create the buffer
	depthReadbackBuffer = SDL_CreateGPUTransferBuffer(device.handle(), &bi);
	if (!depthReadbackBuffer) {
		SDL_LogError(
			SDL_LOG_CATEGORY_ERROR, "Player: failed to create main viewport depth readback buffer: %s",
			SDL_GetError()
		);
	}
}

void Player::collectReadbackResults ()
{
	// Collect fences of ongoing readbacks
	std::vector<SDL_GPUFence*> pendingFences;
	if (depthReadback) {
		if (std::holds_alternative<SDL_GPUFence*>(depthReadback.value().state)) {
			pendingFences.emplace_back(std::get<SDL_GPUFence*>(depthReadback.value().state));
			depthReadback.value().state = nullptr; // prevent double-wait/release
		}
	}

	// Wait for completion and free resources
	if (!pendingFences.empty())
	{
		SDL_WaitForGPUFences(
			device.handle(), true, pendingFences.data(), pendingFences.size()
		);
		for (auto fence : pendingFences) {
			SDL_ReleaseGPUFence(device.handle(), fence);
		}

		// Transition into mapped state
		if (depthReadback) {
			if (std::holds_alternative<SDL_GPUFence*>(depthReadback.value().state))
				depthReadback.value().transitionToMapped(depthReadbackBuffer);
		}
	}

	// Create new frame token.
	++readbackToken;
}



//////
//
// Module namespace close
//

} // namespace fcg
