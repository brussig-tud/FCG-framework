
//////
//
// Includes
//

// C++ STL
#include <format>
#include <stdexcept>

// SDL3
#include <SDL3/SDL.h>

// Local includes
#include "FCG/player.h"
#include "FCG/window.h"
#include "FCG/viewing.h"



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

Player::Player(Device &device, Window *mainWindow, std::span<std::unique_ptr<Applet>> applets)
	: m_device(device), m_window(mainWindow), applets(applets)
{}

Player::~Player() {
	depthMapping.reset();
	depthReadback.reset();
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

auto Player::findCamera (std::string_view name) -> std::optional<Camera*>
{
	for (auto &applet : applets) {
		Camera *cam = dynamic_cast<Camera*>(applet.get());
		if (cam && applet->name() == name)
			return cam;
	}
	return {};
}

auto Player::swapchainFormat () const -> SDL_GPUTextureFormat {
	/// If the player has no main window or that window is not claimed by a
	/// device an \c SDL_GPU_TEXTUREFORMAT_INVALID is returned as sentinel value.
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
