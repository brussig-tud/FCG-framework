
#ifndef __FCG_PLAYER_H__
#define __FCG_PLAYER_H__


//////
//
// Includes
//

// C++ STL
#include <atomic>
#include <string>

// SDL3 library
#include <SDL3/SDL_gpu.h>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Forward declarations
//

/// Forward declaration of the framework window class.
class Window;



//////
//
// Classes
//

/// The central state of the \ref fcg::run main loop.
///
/// An instance of this class is owned by \ref fcg::run and passed to the endpoints of every running \ref Applet,
/// providing them with a way to interact with the main loop and other global application state.
class FCG_FRAMEWORK_EXPORT Player
{
public:

	////
	// Object construction/destruction

	/// Default constructor. Note that applets should normally interact with the framework-owned instance passed to
	/// their endpoints instead of creating new ones. A default-constructed \c fcg::Player is not connected to any main
	/// window – requests that target a window, like \ref setWindowTitle, are ignored.
	Player() = default;

	/// Create a player connected to the given main window. This is how \ref fcg::run creates the instance that it
	/// passes to the applet endpoints. The window is only referenced, not owned, and must outlive the player.
	explicit Player (Window *mainWindow);

	/// Players are not copyable.
	Player(const Player&) = delete;

	/// Players are not copy-assignable.
	auto operator= (const Player&) -> Player& = delete;


	////
	// Methods

	/// Set the title of the main window. Since the main window is shared by all applets, the most
	/// recently set title wins. Note that SDL requires window titles to be set on the main thread,
	/// which all applet endpoints run on.
	///
	/// \param title The new window title.
	void setWindowTitle (const std::string &title);

	/// Push a continuous redraw request. As long as at least one such request exists, the main loop runs continuously,
	/// i.e. another iteration is started as soon as possible after the current one, instead of blocking while waiting
	/// for events. This is useful e.g. for applets that are animating something. Every push must be balanced by a call
	/// to \ref popContinuousRedraw once continuous redrawing is no longer needed.
	void pushContinuousRedraw ();

	/// Pop a continuous redraw request previously registered via \ref pushContinuousRedraw.
	void popContinuousRedraw ();

	/// Whether at least one continuous redraw request currently exists.
	[[nodiscard]] auto continuousRedrawRequested () const -> bool;

	/// Request the main loop to shut down. For a player connected to a main window this also forwards the request
	/// to that window; for a default-constructed player with no window the request is still recorded.
	void requestClose ();

	/// Check whether closing the application was requested, e.g. by a call to \ref requestClose or by the user
	/// closing the main window.
	[[nodiscard]] auto shouldClose () const -> bool;


	////
	// Accessors

	/// Read-write reference the default main window clear color.
	///
	/// \todo Right now there is no real reason to hide this property behind an accessor. This could change in the
	/// future though in case of multithreading, where we might want to wrap the reference in a scoped lock.
	[[nodiscard]] auto clearColor () -> glm::fvec4& { return m_clearColor; }

	/// Read-only access to the main window clear color.
	///
	/// \todo Right now there is no real reason to hide this property behind an accessor. This could change in the
	/// future though in case of multithreading, where we might want to wrap the reference in a scoped lock.
	[[nodiscard]] auto clearColor () const -> const glm::fvec4& { return m_clearColor; }

	/// The texture format of the main window's swapchain images, as needed for pipeline render targets.
	[[nodiscard]] auto swapchainFormat () const -> SDL_GPUTextureFormat;

	/// Reference the current dimensions of the main window viewport.
	[[nodiscard]] auto viewportSize () const -> glm::uvec2;


private:

	////
	// Member variables

	/// The main window that applets can interact with through the player. Non-owning – the window is owned by whoever
	/// created the \c fcg::Player, (e.g., \ref fcg::run) and must outlive the player.
	Window *m_window = nullptr;

	/// The current clear color of the main window viewport.
	glm::fvec4 m_clearColor = { 0.1f, 0.2f, 0.4f, 1.0f };

	/// The number of currently active continuous redraw requests.
	std::atomic<unsigned> m_numContinuousRedrawRequests{0};

	/// Whether closing the application was requested on this player itself.
	std::atomic<bool> m_closeRequested{false};
};



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_PLAYER_H__
