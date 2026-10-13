
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
#include "FCG/presentation.h"



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

/// \addtogroup fcg_runtime
/// @{



//////
//
// Classes
//

/// \brief
/// 	The application-wide state of the <tt>\ref fcg::run</tt> main loop, shared by all running applets.
///
/// \c fcg::run creates one player and passes it to every callback of every running <tt>\ref Applet</tt>. Through it,
/// applets can change the main window title, request continuous redrawing (e.g. while animating), ask the application
/// to close, look up cameras provided by other applets, query the main window's render-target formats and viewport
/// size, replace scene presentation with a recording callback, and read back the depth buffer of the main viewport.
///
/// All callbacks run on the main thread, so the player is meant to be used from there.
class FCG_FRAMEWORK_EXPORT Player
{
	////
	// Friend declarations

	/// Needs to drive the player through the frames of the main loop.
	friend auto fcg::run (std::vector<std::unique_ptr<Applet>>, PlayerSettings&&) -> int;



public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Create a player for the given device, main window and applets.
	///
	/// Called by \c fcg::run; applications normally don't call this.
	///
	/// \pre
	/// 	\c device, the window pointed to by \c mainWindow (if any) and the storage referenced by \c applets must
	/// 	outlive the player.
	///
	/// \param device
	/// 	The GPU device that applets render with. Only referenced, not owned.
	/// \param mainWindow
	/// 	The main window that applets interact with through the player, or \c nullptr for a player without a window.
	/// 	Only referenced, not owned.
	/// \param applets
	/// 	The applets driven by the main loop. Only the span is stored, so the referenced elements must stay at the
	/// 	same address.
	explicit Player (Device &device, Window *mainWindow, std::span<std::unique_ptr<Applet>> applets);

	/// Destroy the player. References returned by \c getDepthReadbackResult become invalid.
	~Player();

	/// Not copyable.
	Player(const Player&) = delete;

	/// Not copy-assignable.
	auto operator= (const Player&) -> Player& = delete;


	////
	// Accessors

	/// \brief
	/// 	Read-write reference to the linear RGBA clear color of the scene.
	///
	/// In every frame, \c fcg::run clears the scene texture to this color right before calling \c Applet::render, so
	/// changes made in \c Applet::gui or \c Applet::update affect the same frame. Defaults to a dark blue.
	///
	/// \return A reference to the clear color, valid for the lifetime of the player.
	[[nodiscard]] auto clearColor () -> glm::fvec4& { return m_clearColor; }

	/// \brief
	/// 	Read-only reference to the linear RGBA clear color of the scene.
	///
	/// \return A reference to the clear color, valid for the lifetime of the player.
	[[nodiscard]] auto clearColor () const -> const glm::fvec4& { return m_clearColor; }

	/// \brief
	/// 	The GPU device that applets render with.
	///
	/// \return The device, which outlives the player. Accessible through a \c const player as well.
	[[nodiscard]] auto device () const -> Device& { return m_device; }

	/// \brief
	/// 	The main window shared by all applets.
	///
	/// \return The main window, or \c nullptr if the player has no window. The window outlives the player.
	[[nodiscard]] auto mainWindow () const -> Window* { return m_window; }

	/// \brief
	/// 	Describe the color and depth attachments that \c Applet::render draws into.
	///
	/// Use this to create graphics pipelines for rendering the scene. The description is also available before the
	/// first frame (e.g. in \c Applet::init) and while the window is minimized.
	///
	/// \return
	/// 	The attachment formats and sample count of the scene, or \c std::nullopt if the player has no main window or
	/// 	the main window is not claimed by a device (see \c Device::claimWindow).
	[[nodiscard]] auto mainRenderTargetInfo () const -> std::optional<RenderTargetInfo>;

	/// \brief
	/// 	The texture format of the images shown in the main window.
	///
	/// Only pipelines that draw directly into the window image, such as GUI overlays, need this format. Pipelines
	/// rendering the scene use \c mainRenderTargetInfo instead.
	///
	/// \return
	/// 	The swapchain texture format (see \c Window::swapchainFormat), or \c SDL_GPU_TEXTUREFORMAT_INVALID if the
	/// 	player has no main window or the main window is not claimed by a device.
	[[nodiscard]] auto swapchainFormat () const -> SDL_GPUTextureFormat;

	/// \brief
	/// 	The current size of the main window viewport.
	///
	/// \return The viewport size in pixels, or <tt>(0, 0)</tt> if the player has no main window.
	[[nodiscard]] auto viewportSize () const -> glm::uvec2;

	/// \brief
	/// 	Check whether at least one continuous redraw request is active.
	///
	/// \return \c true if the main loop currently runs continuously, \c false if it waits for events.
	[[nodiscard]] auto continuousRedrawRequested () const -> bool;

	/// \brief
	/// 	Check whether closing the application was requested.
	///
	/// \return
	/// 	\c true after a call to \c requestClose, or after the main window was asked to close (e.g. by the user
	/// 	clicking its close button), \c false otherwise.
	[[nodiscard]] auto shouldClose () const -> bool;


	////
	// Methods

	/// \brief
	/// 	Set the title of the main window.
	///
	/// The main window is shared by all applets, so the most recently set title wins.
	///
	/// \note
	/// 	SDL requires window titles to be set on the main thread. All applet callbacks run on the main thread. If the
	/// 	player has no main window, the call only logs a warning.
	///
	/// \param title
	/// 	The new window title.
	void setWindowTitle (const std::string &title);

	/// \brief Replace application-wide scene presentation with an owned recording callback.
	///
	/// The latest registration wins, destroying the previous captures. Changes before the next presentation point
	/// affect that frame, after all applet scene draws and before ImGui rendering. An empty callback restores default
	/// conversion. Move-only and mutable callables are supported. Owned captures live until replacement, clearing
	/// or shutdown; \c fcg::run clears them before applet destruction. Failed callable construction leaves the
	/// previous registration intact. See <tt>\ref fcg_runtime_workflows_presentation</tt> for recording requirements.
	///
	/// \pre Use on the main thread. Captured references must remain valid while registered.
	///
	/// \param callback The callback to move into ownership, or an empty wrapper to restore default presentation.
	///
	/// \throws std::logic_error If registration is attempted during callback invocation.
	void setPresentationCallback (PresentationCallback callback);

	/// \brief Restore default scene conversion, destroying the registered callback's captures.
	///
	/// Takes effect at the next presentation point and never restores an earlier registration. Clearing an empty
	/// registration has no effect. Registrations are also cleared before applets are destroyed by \c fcg::run.
	///
	/// \pre Use on the main thread; referenced capture state must stay alive until clearing finishes.
	///
	/// \throws std::logic_error If clearing is attempted during callback invocation.
	void clearPresentationCallback ();

	/// \brief
	/// 	Add a continuous redraw request.
	///
	/// While at least one request is active, the main loop starts the next frame as soon as possible instead of
	/// waiting for events. Use this e.g. while an applet is animating. Balance every call with a call to
	/// <tt>\ref popContinuousRedraw</tt> when continuous redrawing is not needed anymore.
	void pushContinuousRedraw ();

	/// \brief
	/// 	Remove a continuous redraw request previously added with <tt>\ref pushContinuousRedraw</tt>.
	///
	/// A call without a matching \c pushContinuousRedraw logs a warning and has no effect.
	void popContinuousRedraw ();

	/// \brief
	/// 	Request the application to close.
	///
	/// \c shouldClose returns \c true afterwards. If the player has a main window, the request is also forwarded to it
	/// (see \c Window::requestClose). \c fcg::run then leaves its main loop at the next opportunity and shuts down.
	void requestClose ();

	/// \brief
	/// 	Find a camera provided by one of the running applets.
	///
	/// Searches the running applets, in the order they were passed to \c fcg::run, for one that derives from
	/// <tt>\ref Camera</tt> and whose \c Applet::name equals \c name.
	///
	/// \param name
	/// 	The applet name of the camera to look for.
	///
	/// \return
	/// 	A pointer to the first matching camera, or \c std::nullopt if there is none. The camera is owned by
	/// 	\c fcg::run and stays valid until the applets are destroyed at shutdown.
	[[nodiscard]] auto findCamera (std::string_view name) -> std::optional<Camera*>;

	/// \brief
	/// 	Request a readback of the main viewport's depth buffer, i.e. a download of its contents to the CPU.
	///
	/// The readback captures the depth buffer as rendered by the most recently submitted frame. Commands recorded in
	/// the current frame are not included. Calling this again within the same frame returns the same token, so
	/// several applets can share one readback.
	///
	/// Retrieve the result with <tt>\ref getDepthReadbackResult</tt>. It is ready at the start of the next frame;
	/// querying it earlier blocks until the GPU has finished the download. The token stays valid until a newer
	/// readback is requested in a later frame or the viewport is resized. A resize invalidates all tokens before
	/// \c Applet::onViewportResize is called; discard them instead of querying them.
	///
	/// Can be called from \c Applet::onEvent, \c Applet::gui, \c Applet::update, \c Applet::onViewportResize and
	/// \c Applet::render while a frame is being rendered.
	///
	/// \pre
	/// 	At least one frame has been rendered since startup or the last viewport resize. Otherwise, the depth values
	/// 	are undefined.
	///
	/// \return
	/// 	A token for retrieving the result with \c getDepthReadbackResult.
	///
	/// \throws std::runtime_error
	/// 	If no frame is currently being rendered (e.g. in \c Applet::init or while the window is minimized), or if
	/// 	the readback could not be started.
	/// \throws std::logic_error
	/// 	If an earlier readback of this player is in an inconsistent state after a failed retrieval.
	[[nodiscard]] auto scheduleDepthReadback () -> uint64_t;

	/// \brief
	/// 	Retrieve the result of a depth readback requested with <tt>\ref scheduleDepthReadback</tt>.
	///
	/// Blocks until the download has finished if it is still pending. Results are always available without waiting
	/// from the start of the frame after the one in which the readback was requested.
	///
	/// The result covers the whole viewport at the time of the request, one 32-bit floating-point depth value per
	/// pixel (format \c SDL_GPU_TEXTUREFORMAT_D32_FLOAT). Read individual values with
	/// <tt>readTexel<float>({x, y, 0})</tt>, where <tt>(x, y)</tt> are pixel coordinates with the origin in the
	/// top-left corner. Depth values range from 0 to 1. Pixels not covered by any geometry keep the value 1 that the
	/// scene pass clears the depth buffer to (the far plane).
	///
	/// \pre
	/// 	\c token was returned by \c scheduleDepthReadback and has not been invalidated since (see there).
	///
	/// \param token
	/// 	The token returned by \c scheduleDepthReadback.
	///
	/// \return
	/// 	A reference to the downloaded depth values. It stays valid until a newer readback is requested in a later
	/// 	frame, the viewport is resized, or the player is destroyed.
	///
	/// \throws std::logic_error
	/// 	If there is no current readback, e.g. if none was requested or if the viewport was resized since.
	/// \throws std::runtime_error
	/// 	If \c token does not belong to the current readback (e.g. after a newer one was requested), or if waiting
	/// 	for or accessing the downloaded data failed.
	[[nodiscard]] auto getDepthReadbackResult (uint64_t token) -> const TextureReadback::Mapping&;


private:

	////
	// Methods

	/// Select custom or default presentation for an acquired frame; guard registration during custom invocation.
	[[nodiscard]] auto presentFrame () -> std::expected<void, FullscreenError>;

	/// Destroy the current depth readback and its mapping (mapping first), invalidating all outstanding tokens. Called
	/// by \c fcg::run at startup and whenever the viewport size changes.
	void invalidateReadbacks ();

	/// Wait for and map a depth readback that was requested in the previous frame, then advance \c readbackToken to
	/// start a new frame. Called by \c fcg::run once per frame right after beginning it.
	void collectReadbackResults ();


	////
	// Fields

	/// Application-wide owning presentation callback, cleared before applet teardown by the runtime.
	PresentationCallback m_presentationCallback;

	/// Whether the callback is currently invoked; prevents destruction of its executing callable.
	bool m_presenting = false;

	/// The main rendering device.
	Device &m_device;

	/// The main window that applets can interact with through the player. Non-owning – the window is owned by whoever
	/// created the \c fcg::Player, (e.g., <tt>\ref fcg::run</tt>) and must outlive the player.
	Window *m_window = nullptr;

	/// The currently ongoing frame. Non-owning reference, managed externally.
	Frame *frame = nullptr;

	/// The current clear color of the main window viewport, in linear RGBA space.
	glm::fvec4 m_clearColor = {0.0097f, 0.0331f, 0.1329f, 1.0f};

	/// The number of currently active continuous redraw requests.
	std::atomic<unsigned> m_numContinuousRedrawRequests{0};

	/// Whether closing the application was requested on this player itself.
	std::atomic<bool> m_closeRequested{false};

	/// The pending or completed depth readback, if any. Kept at a stable address on the heap because
	/// \c depthMapping refers into it; the mapping must therefore always be reset before this pointer.
	std::unique_ptr<TextureReadback> depthReadback;

	/// CPU access to the downloaded depth values of \c depthReadback, created once the download has finished.
	/// Destroyed before \c depthReadback.
	std::optional<TextureReadback::Mapping> depthMapping;

	/// The token handed out for \c depthReadback, i.e. the value of \c readbackToken in the frame it was requested.
	uint64_t depthToken = 0;

	/// Frame counter serving as the token for readbacks requested in the current frame. Incremented once per frame.
	uint64_t readbackToken = 0;

	/// The applets driven by the main loop, searched by \c findCamera.
	std::span<std::unique_ptr<Applet>> applets;
};



/// @}



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_PLAYER_H__
