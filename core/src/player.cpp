
//////
//
// Includes
//

// C++ STL
#include <format>
#include <span>
#include <stdexcept>

// SDL3
#include <SDL3/SDL.h>

// Local includes
#include "FCG/player.h"
#include "FCG/gui.h"
#include "FCG/window.h"
#include "FCG/viewing.h"



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
// Player

Player::Player(Device &device, Window *mainWindow, std::span<std::unique_ptr<Applet>> applets)
	: m_device(device), m_window(mainWindow), applets(applets)
{}

Player::~Player() {
	shutdownPresentation();
	depthMapping.reset();
	depthReadback.reset();
}

auto Player::guiUnitScale () const noexcept -> float {
	return m_gui ? m_gui->unitScale() : 1.f;
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

void Player::setPresentationRecorder (PresentationRecorder recorder) {
	if (m_presenting)
		throw std::logic_error("Cannot replace presentation during recorder invocation");
	m_presentationRecorder = std::move(recorder);
}

void Player::clearPresentationRecorder () {
	if (m_presenting)
		throw std::logic_error("Cannot clear presentation during recorder invocation");
	m_presentationRecorder = {};
}

auto Player::initializePresentation () -> std::expected<void, FullscreenError>
{
	// No presentation without window
	if (!m_window)
		return std::unexpected(FullscreenError{FullscreenErrorCode::InvalidState, "No main window to present"});

	// Create our canonical sRGB fullscreen pass
	auto pass = FullscreenPass::linearToSRGB(m_device, m_window->swapchainFormat());
	if (!pass)
		return std::unexpected(pass.error());

	// Define the default presentation recorder.
	m_defaultPresentationRecorder = [this, pass = std::move(*pass)](const PresentationContext &context)
	-> std::expected<void, FullscreenError> {
		const FullscreenTextureBinding binding{&context.scene, &context.sceneSampler};
		const std::span<const std::byte> uniforms[]{std::as_bytes(std::span(&m_gammaCorrection, 1))};
		return pass.record(
			context.cmdBuffer, context.target, {.samplers = {&binding, 1}, .uniforms = uniforms}
		);
	};
	return {};
}

void Player::shutdownPresentation () noexcept {
	m_presentationRecorder = {};
	m_defaultPresentationRecorder = {};
}

auto Player::presentFrame () -> std::expected<void, FullscreenError>
{
	if (!frame)
		return std::unexpected(FullscreenError{FullscreenErrorCode::InvalidState, "No acquired frame to present"});
	auto &recorder = m_presentationRecorder ? m_presentationRecorder : m_defaultPresentationRecorder;
	m_presenting = true;
	try {
		auto result = frame->present(recorder);
		m_presenting = false;
		return result;
	} catch (...) {
		m_presenting = false;
		throw;
	}
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

auto Player::findCamera (std::string_view name) -> std::optional<Camera*> {
	for (auto &applet : applets)
		if (Camera *cam = dynamic_cast<Camera*>(applet.get()); cam && applet->name() == name)
			return cam;
	return {};
}

auto Player::swapchainFormat () const -> SDL_GPUTextureFormat {
	return m_window ? m_window->swapchainFormat() : SDL_GPU_TEXTUREFORMAT_INVALID;
}

auto Player::mainRenderTargetInfo () const -> std::optional<RenderTargetInfo> {
	return m_window ? m_window->renderTargetInfo() : std::nullopt;
}

auto Player::viewportSize() const -> glm::uvec2 {
	return m_window ? m_window->viewportSize() : glm::uvec2(0);
}

auto Player::scheduleDepthReadback () -> uint64_t
{
	if (depthReadback && depthToken == readbackToken)
		return depthToken;
	if (depthReadback && !depthMapping)
		throw std::logic_error("Player: orphaned depth readback operation");
	if (!frame || !frame->depthTexture())
		throw std::runtime_error("Player: no depth texture is available");
	depthMapping.reset();
	depthReadback.reset();
	const auto extent = viewportSize();
	const SDL_GPUTextureRegion source{frame->depthTexture(), 0, 0, 0, 0, 0, extent.x, extent.y, 1};
	auto ticket = TextureReadback::create(m_device, source, SDL_GPU_TEXTUREFORMAT_D32_FLOAT);
	if (!ticket)
		throw std::runtime_error("Player: depth readback: " + ticket.error().message);
	depthReadback = std::make_unique<TextureReadback>(std::move(*ticket));
	depthToken = readbackToken;
	return depthToken;
}

auto Player::getDepthReadbackResult (uint64_t token) -> const TextureReadback::Mapping&
{
	if (!depthReadback)
		throw std::logic_error("Player: no depth readback was scheduled");
	if (token != depthToken)
		throw std::runtime_error("Player: depth readback result queried with invalid token");
	if (!depthMapping)
	{
		if (auto result = depthReadback->wait(); !result)
			throw std::runtime_error("Player: waiting for depth: " + result.error().message);
		auto mapping = depthReadback->map();
		if (!mapping)
			throw std::runtime_error("Player: mapping depth: " + mapping.error().message);
		depthMapping.emplace(std::move(*mapping));
	}
	return *depthMapping;
}

void Player::invalidateReadbacks () {
	depthMapping.reset();
	depthReadback.reset();
}

void Player::collectReadbackResults () {
	if (depthReadback && !depthMapping)
		(void)getDepthReadbackResult(depthToken);
	++readbackToken;
}



//////
//
// Module namespace close
//

} // namespace fcg
