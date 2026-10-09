
#ifndef __FCG_PLAYER_H__
#define __FCG_PLAYER_H__


//////
//
// Includes
//

// C++ STL
#include <string>
#include <vector>
#include <memory>
#include <span>
#include <atomic>
#include <optional>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/render_target.h"
#include "FCG/run.h"
#include "FCG/applet.h"
#include "FCG/texture.h"



//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class Window;
	class Frame;
	class Camera;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/** \addtogroup fcg_runtime
 * @{
 */



//////
//
// Classes
//

/// The central state of the <code>\ref fcg::run</code> main loop.
///
/// An instance of this class is owned by <code>\ref fcg::run</code> and passed to the endpoints of every running
/// <code>\ref Applet</code>, providing them with a way to interact with the main loop and other global application
/// state.
class FCG_FRAMEWORK_EXPORT Player
{
	////
	// Friend declarations

	/// The main loop needs to manipulate the player.
	friend auto fcg::run (std::vector<std::unique_ptr<Applet>>, PlayerSettings&&) -> int;



public:

	////
	// Object construction/destruction

	/// Create a player using the given device, connected to the given main window. Both device and window are only
	/// referenced, not owned, and must outlive the player.
	explicit Player (Device &device, Window *mainWindow, std::span<std::unique_ptr<Applet>> applets);

	/// The destructor.
	~Player();

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
	/// to <code>\ref popContinuousRedraw</code> once continuous redrawing is no longer needed.
	void pushContinuousRedraw ();

	/// Pop a continuous redraw request previously registered via <code>\ref pushContinuousRedraw</code>.
	void popContinuousRedraw ();

	/// Whether at least one continuous redraw request currently exists.
	[[nodiscard]] auto continuousRedrawRequested () const -> bool;

	/// Request the main loop to shut down. For a player connected to a main window this also forwards the request
	/// to that window; for a default-constructed player with no window the request is still recorded.
	void requestClose ();

	/// Check whether closing the application was requested, e.g. by a call to <code>\ref requestClose</code> or by the
	/// user closing the main window.
	[[nodiscard]] auto shouldClose () const -> bool;

	/// Find and reference the camera with the given name if it exists.
	[[nodiscard]] auto findCamera (std::string_view name) -> std::optional<Camera*>;


	////
	// Accessors

	/// Read-write reference the linear scene clear color.
	///
	/// \todo Right now there is no real reason to hide this property behind an accessor. This could change in the
	/// future though in case of multithreading, where we might want to wrap the reference in a scoped lock.
	[[nodiscard]] auto clearColor () -> glm::fvec4& { return m_clearColor; }

	/// Read-only access to the linear scene clear color.
	///
	/// \todo Right now there is no real reason to hide this property behind an accessor. This could change in the
	/// future though in case of multithreading, where we might want to wrap the reference in a scoped lock.
	[[nodiscard]] auto clearColor () const -> const glm::fvec4& { return m_clearColor; }

	/// Borrow the original device, including through a \c const player; it must outlive the player.
	[[nodiscard]] auto device () const -> Device& { return m_device; }

	/// Borrow the main window, or \c nullptr without one. The window must outlive the player and pending dialogs.
	[[nodiscard]] auto mainWindow () const -> Window* { return m_window; }

	/// Main-pass attachments; absent without a main window or valid window claim.
	[[nodiscard]] auto mainRenderTargetInfo () const -> std::optional<RenderTargetInfo>;

	/// The texture format of the main window's swapchain images, for presentation; scene pipelines use the main render-target metadata.
	[[nodiscard]] auto swapchainFormat () const -> SDL_GPUTextureFormat;

	/// Reference the current dimensions of the main window viewport.
	[[nodiscard]] auto viewportSize () const -> glm::uvec2;

	/// \brief Ask for a readback of the main viewport depth buffer.
	///
	/// Downloads the most recently submitted contents of the current depth texture. Commands recorded in an
	/// unsubmitted frame are not included.
	///
	/// \pre An active frame is available and its depth texture has been rendered and submitted since creation.
	///
	/// \note
	/// 	The readback result \em must be queried after being scheduled. Clients have exactly one frame to do so,
	/// 	failure to retrieve the result before it is overwritten is a logic error and can cause a crash.
	/// 	A viewport resize invalidates outstanding tokens and borrowed views before the \c Applet::onViewportResize
	/// 	callback; invalidated tokens must be discarded instead of queried.
	///
	/// \return A token that can be used to check for completion of the readback operation and to retrieve the results.
	[[nodiscard]] auto scheduleDepthReadback () -> uint64_t;

	/// \brief Ask for the result of a previously scheduled depth readback operation.
	///
	/// Blocks if the transfer is still pending. Unless invalidated by a viewport resize, the result is available at
	/// the beginning of the next frame after the one it was requested.
	///
	/// \pre The token has not been invalidated by a viewport resize or replaced by a newer readback.
	///
	/// \note The reference expires when a newer download is scheduled, the viewport is resized, or the player is destroyed.
	///
	/// \return Borrowed scoped depth access; copy floats with \c readTexel.
	[[nodiscard]] auto getDepthReadbackResult (uint64_t token) -> const TextureReadback::Mapping&;


private:

	////
	// Methods

	/// Destroy mappings before tickets and invalidate all outstanding tokens.
	void invalidateReadbacks ();

	/// Collect the results of any dispatched readback operations.
	void collectReadbackResults ();


	////
	// Fields

	/// The main rendering device.
	Device &m_device;

	/// The main window that applets can interact with through the player. Non-owning – the window is owned by whoever
	/// created the \c fcg::Player, (e.g., <code>\ref fcg::run</code>) and must outlive the player.
	Window *m_window = nullptr;

	/// The currently ongoing frame. Non-owning reference, managed externally.
	Frame *frame = nullptr;

	/// The current clear color of the main window viewport, in linear RGBA space.
	glm::fvec4 m_clearColor = {0.0097f, 0.0331f, 0.1329f, 1.0f};

	/// The number of currently active continuous redraw requests.
	std::atomic<unsigned> m_numContinuousRedrawRequests{0};

	/// Whether closing the application was requested on this player itself.
	std::atomic<bool> m_closeRequested{false};

	/// Stationary ticket allocation; mappings must end before resetting it.
	std::unique_ptr<TextureReadback> depthReadback;

	/// Scoped CPU access, destroyed before the ticket.
	std::optional<TextureReadback::Mapping> depthMapping;

	/// Token belonging to the current ticket.
	uint64_t depthToken = 0;

	/// The current frame's readback token.
	uint64_t readbackToken = 0;

	/// The applets we're driving
	std::span<std::unique_ptr<Applet>> applets;
};



/** @} */

//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_PLAYER_H__
