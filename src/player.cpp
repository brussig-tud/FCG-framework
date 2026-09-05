
//////
//
// Includes
//

// SDL3
#include <SDL3/SDL_log.h>

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

Player::Player (Window *mainWindow) : m_window(mainWindow) {
	// Nothing else to do
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

auto Player::viewportSize() const -> glm::uvec2 {
	return m_window ? m_window->viewportSize() : glm::uvec2(0);
}

auto Player::swapchainFormat () const -> SDL_GPUTextureFormat {
	/// If the player has no main window or that window is not claimed by a
	/// device an \c SDL_GPU_TEXTUREFORMAT_INVALID is returned as sentinel value.
	return m_window ? m_window->swapchainFormat() : SDL_GPU_TEXTUREFORMAT_INVALID;
}

//////
//
// Module namespace close
//

} // namespace fcg
