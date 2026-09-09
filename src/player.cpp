
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

Player::PendingReadback::~PendingReadback() {
	if (fence) {
		SDL_ReleaseGPUFence(device.handle(), fence);
		fence = nullptr;
	}
}

template <class Texel>
auto Player::ReadbackController<Texel>::dispatch () -> PendingReadback
{
	// Start copy pass
	SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(player.device.handle());
	SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);
	const auto extent = player.viewportSize();
	const SDL_GPUTextureRegion source {
		.texture = player.frame->depthTexture(), .mip_level = 0, .layer = 0, .x = 0, .y = 0, .z = 0,
		.w = extent.x, .h = extent.y, .d = 1
	};

	// Describe copy geometry
	SDL_GPUTextureTransferInfo destination {
		.transfer_buffer = player.depthReadbackBuffer, .offset = 0, .pixels_per_row = extent.x,
		.rows_per_layer = extent.y
	};
	SDL_DownloadFromGPUTexture(copyPass, &source, &destination);
	SDL_EndGPUCopyPass(copyPass);

	// Submit and obtain a fence.
	SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuf);

	// Done – hand the resulting state (and its fence) to the readback state machine.
	return {player.device, fence, extent, glm::vec2(1, extent.x), player.readbackToken};
}

template <class Texel>
auto Player::ReadbackController<Texel>::completeReadback (PendingReadback &pending) -> OwningTextureView<Texel, 2>
{
	// Wait for the GPU to finish the readback operation and release the fence
	if (pending.fence) {
		SDL_WaitForGPUFences(player.device.handle(), true, &pending.fence, 1);
		SDL_ReleaseGPUFence(player.device.handle(), pending.fence);
		pending.fence = nullptr;
	}

	// Map the readback buffer for CPU access
	return OwningTextureView<Texel, 2>(player.device, player.depthReadbackBuffer, pending.extent, pending.stride);
}

template <class Texel>
void Player::ReadbackController<Texel>::on (
	const std::monostate&, const ScheduleReadback&, StateMachine &fsm
){
	fsm.template transition<PendingReadback>(dispatch());
}

template <class Texel>
void Player::ReadbackController<Texel>::on (
	PendingReadback &curState, const ScheduleReadback&, StateMachine &fsm
){
	if (curState.token == player.readbackToken)
		// Readback for this frame has already been scheduled.
		return;

	// A pending readback from an older frame was never collected – this is a logic error.
	const auto orphanedToken = curState.token;
	fsm.template transition<std::monostate>();
	const auto msg = std::format(
		"Player: orphaned depth readback operation detected while scheduling a new one\n"
		"  Current token: {} - orphaned token: {}", player.readbackToken, orphanedToken
	);
	SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
	throw std::logic_error(msg);
}

template <class Texel>
void Player::ReadbackController<Texel>::on (
	ReadyReadback<Texel> &curState, const ScheduleReadback&, StateMachine &fsm
){
	if (curState.token == player.readbackToken)
		// Readback for this frame has already been scheduled (and was collected).
		return;

	// The previous readback operation was completed and can be overwritten.
	fsm.template transition<PendingReadback>(dispatch());
}

template <class Texel>
void Player::ReadbackController<Texel>::on (
	const std::monostate&, const QueryReadback&, StateMachine &fsm
){
	constexpr auto msg = "Player: depth readback result queried but no readback was ever scheduled";
	SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, msg);
	throw std::logic_error(msg);
}

template <class Texel>
void Player::ReadbackController<Texel>::on (
	PendingReadback &curState, const QueryReadback &event, StateMachine &fsm
){
	if (event.token != curState.token) {
		const auto msg = std::format(
			"Player: depth readback result queried with invalid token\n"
			"  Requesting token: {}, current token: {}", event.token, curState.token
		);
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}

	// The requested readback operation is still pending, wait for it to complete.
	fsm.template transition<ReadyReadback<Texel>>(completeReadback(curState), curState.token);
}

template <class Texel>
void Player::ReadbackController<Texel>::on (
	ReadyReadback<Texel> &curState, const QueryReadback &event, StateMachine &fsm
){
	if (event.token != curState.token) {
		const auto msg = std::format(
			"Player: depth readback result queried with invalid token\n"
			"  Requesting token: {}, current token: {}", event.token, curState.token
		);
		SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "%s", msg.c_str());
		throw std::runtime_error(msg);
	}
}

template <class Texel>
void Player::ReadbackController<Texel>::on (
	PendingReadback &curState, const FrameBegin&, StateMachine &fsm
){
	// Wait for completion and transition into the mapped state.
	fsm.template transition<ReadyReadback<Texel>>(completeReadback(curState), curState.token);
}

Player::Player (Device &device, Window *mainWindow) : device(device), m_window(mainWindow) {
	// Nothing else to do here yet.
}

Player::~Player() {
	// Abandon any in-flight readback so its mapping and fence are released before we tear down the buffer below.
	depthReadback.fsm.transition<std::monostate>();
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
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: starting continuous redraw");
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
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: stopping continuous redraw");
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

auto Player::shouldClose () const -> bool {
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

[[nodiscard]] auto Player::scheduleDepthReadback () -> uint64_t {
	// Hand the request to the readback state machine
	depthReadback.fsm.handle(ScheduleReadback{});

	// Done! The current frame's token identifies the readback operation.
	return readbackToken;
}

[[nodiscard]] auto Player::getDepthReadbackResult (uint64_t token) -> TextureView<float> {
	// Hand the query to the readback state machine
	depthReadback.fsm.handle(QueryReadback{token});

	// At this point the state machine is guaranteed to hold the requested readback result.
	return depthReadback.fsm.get<ReadyReadback<float>>().view;
}

void Player::recreateReadbackBuffers ()
{
	// Abandon any in-flight readback – its mapping, if any, will not be valid anymore once the buffer is
	// recreated below.
	depthReadback.fsm.transition<std::monostate>();

	// Destroy old buffer if it exists
	if (depthReadbackBuffer) {
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

void Player::collectReadbackResults () {
	// Collect in-flight readback results, transitioning them into the mapped state.
	depthReadback.fsm.handle(FrameBegin{});

	// Create new frame token.
	++readbackToken;
}



//////
//
// Module namespace close
//

} // namespace fcg
