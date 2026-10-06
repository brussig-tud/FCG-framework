
//////
//
// Includes
//

// C++ STL
#include <format>
#include <variant>
#include <limits>
#include <stdexcept>

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
	device.retireFence(std::move(fence));
}

template <class Texel>
auto Player::ReadbackController<Texel>::dispatch () -> PendingReadback
{
	if (!player.frame || !player.frame->depthTexture() || !player.depthReadbackBuffer.handle())
		throw std::runtime_error("Player: no depth texture or download storage is available");

	player.device.collectRetiredFences();
	auto fenceOwner = std::make_unique<Device::RetiredFence>();

	// Start copy pass. No invalid handle may reach SDL's validation assertions.
	SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(player.device.handle());
	if (!cmdBuf)
		throw std::runtime_error(std::string("Player: acquiring depth readback commands: ") + SDL_GetError());
	SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdBuf);
	if (!copyPass) {
		const auto message = std::string("Player: beginning depth copy pass: ") + SDL_GetError();
		SDL_CancelGPUCommandBuffer(cmdBuf);
		throw std::runtime_error(message);
	}
	const auto extent = player.viewportSize();
	const SDL_GPUTextureRegion source {
		.texture = player.frame->depthTexture(), .mip_level = 0, .layer = 0, .x = 0, .y = 0, .z = 0,
		.w = extent.x, .h = extent.y, .d = 1
	};

	// Describe copy geometry
	SDL_GPUTextureTransferInfo destination {
		.transfer_buffer = player.depthReadbackBuffer.handle(), .offset = 0, .pixels_per_row = extent.x,
		.rows_per_layer = extent.y
	};
	SDL_DownloadFromGPUTexture(copyPass, &source, &destination);
	SDL_EndGPUCopyPass(copyPass);

	// Submit and obtain a fence.
	SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuf);
	if (!fence)
		throw std::runtime_error(std::string("Player: submitting depth readback: ") + SDL_GetError());

	// Done – hand the resulting state (and its fence) to the readback state machine.
	fenceOwner->handle = fence;
	return {player.device, std::move(fenceOwner), extent, glm::uvec2(1, extent.x), player.readbackToken};
}

template <class Texel>
auto Player::ReadbackController<Texel>::completeReadback (PendingReadback &pending) -> OwningTextureView<Texel, 2>
{
	// Wait for the GPU to finish the readback operation and release the fence
	if (pending.fence) {
		if (!SDL_WaitForGPUFences(player.device.handle(), true, &pending.fence->handle, 1))
			throw std::runtime_error(std::string("Player: waiting for depth readback: ") + SDL_GetError());
		player.device.retireFence(std::move(pending.fence));
	}

	// Map the readback buffer for CPU access
	auto mapping = player.depthReadbackBuffer.map();
	if (!mapping)
		throw std::runtime_error("Player: mapping depth readback: " + mapping.error().message);
	return OwningTextureView<Texel, 2>(std::move(*mapping), pending.extent, pending.stride);
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

	// Release the previous CPU mapping before scheduling a GPU write into the reused transfer storage.
	fsm.template transition<std::monostate>();
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
	depthReadbackBuffer = {};
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
	depthReadbackBuffer = {};

	const auto vpSize = viewportSize();
	if (!vpSize.x || !vpSize.y)
		return;
	// D32_FLOAT depth data; reject overflow before creating the SDL transfer allocation.
	const auto maxTexels = std::numeric_limits<Uint32>::max() / sizeof(float);
	if (vpSize.x > maxTexels / vpSize.y) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Player: depth readback allocation exceeds SDL's size limit");
		return;
	}
	auto buffer = TransferBuffer::create(
		device, std::size_t(vpSize.x) * vpSize.y * sizeof(float), SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD
	);
	if (!buffer) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Player: creating depth readback buffer: %s", buffer.error().message.c_str());
		return;
	}
	depthReadbackBuffer = std::move(*buffer);
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
