
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
#include <concepts>
#include <expected>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/render_target.h"
#include "FCG/run.h"
#include "FCG/applet.h"
#include "FCG/fullscreen.h"
#include "FCG/texture.h"
#include "FCG/frame.h"



//////
//
// Forward declarations
//

// Dear ImGui types
struct ImVec2;

// Framework types
namespace fcg {
	class Gui;
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

/// \brief The application-wide state of the <tt>\ref fcg::run</tt> main loop, shared by all running applets.
///
/// \c fcg::run creates one player and passes it to every callback of every running <tt>\ref Applet</tt>. Through it,
/// applets can change the main window title, request scene refreshes or continuous redrawing (e.g. while animating),
/// ask the application to close, look up cameras provided by other applets, query the main window's render-target
/// formats and viewport size, adjust gamma correction or replace scene presentation with a presentation recorder,
/// and read back the
/// depth buffer of the main viewport.
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

	/// \brief Create a player for the given device, main window and applets.
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

	/// \brief Read-write reference to the linear RGBA clear color of the scene.
	///
	/// When rendering the scene, \c fcg::run clears its texture to this color before calling \c Applet::render.
	/// Call \c requireSceneRedraw after changing the color. Defaults to a dark blue.
	///
	/// \return A reference to the clear color, valid for the lifetime of the player.
	[[nodiscard]] auto clearColor () -> glm::fvec4& {
		return m_clearColor;
	}

	/// \brief Read-only reference to the linear RGBA clear color of the scene.
	///
	/// \return A reference to the clear color, valid for the lifetime of the player.
	[[nodiscard]] auto clearColor () const -> const glm::fvec4& {
		return m_clearColor;
	}

	/// \brief Read-write reference to the gamma and RGB bounds used by built-in scene presentation.
	///
	/// Presentation converts linear scene RGB to sRGB, then raises each encoded channel to the power of its reciprocal
	/// <tt>\ref GammaUniforms::gamma component</tt>, in red, green, blue order. The final color is clamped to
	/// <tt>\ref GammaUniforms::clampRange</tt>. Gamma defaults to 1 in every channel and the bounds to [0, 1]. Gamma
	/// values above 1 brighten that channel's midtones; values below 1 darken them. Alpha and the GUI overlay are
	/// unchanged.
	///
	/// The settings are shared by all applets. Changes before presentation, including in <tt>Applet::init</tt>,
	/// <tt>Applet::gui</tt>, \c Applet::update or <tt>Applet::render</tt> affect the next presentation.
	/// \ref fcg::PresentationRecorder "Custom recorders" use their own parameters. Installing or clearing a recorder
	/// preserves these settings, and clearing resumes built-in presentation with their current values.
	///
	/// \pre
	/// 	Use on the main thread. Supply finite, strictly positive gamma in each channel and ordered clamp endpoints
	/// 	that are not NaN; infinite endpoints are allowed, as described by the \c GammaUniforms contract.
	///
	/// \return A reference to the parameters, valid for the lifetime of the player.
	[[nodiscard]] auto gammaCorrection () -> GammaUniforms& {
		return m_gammaCorrection;
	}

	/// \brief Read-only reference to the gamma and RGB bounds used by built-in scene presentation.
	///
	/// \pre Use on the main thread.
	///
	/// \return A reference to the parameters, valid for the lifetime of the player.
	[[nodiscard]] auto gammaCorrection () const -> const GammaUniforms& {
		return m_gammaCorrection;
	}

	/// \brief The GPU device that applets render with.
	///
	/// \return The device, which outlives the player. Accessible through a \c const player as well.
	[[nodiscard]] auto device () const -> Device& {
		return m_device;
	}

	/// \brief The main window shared by all applets.
	///
	/// \return The main window, or \c nullptr if the player has no window. The window outlives the player.
	[[nodiscard]] auto mainWindow () const -> Window* {
		return window;
	}

	/// \brief Describe the color and depth attachments that \c Applet::render draws into.
	///
	/// Use this to create graphics pipelines for rendering the scene. The description is also available before the
	/// first frame and while the window is minimized. Create pipelines in \c Applet::recreatePipelines using this
	/// effective description; accepted requests through \c setSceneFormat are applied at the next lifecycle boundary.
	///
	/// \return
	/// 	The attachment formats and sample count of the scene, or \c std::nullopt if the player has no main window or
	/// 	the main window is not claimed by a device (see \c Device::claimWindow).
	[[nodiscard]] auto mainRenderTargetInfo () const -> std::optional<RenderTargetInfo>;

	/// \brief The texture format of the images shown in the main window.
	///
	/// Only pipelines that draw directly into the window image, such as GUI overlays, need this format. Pipelines
	/// rendering the scene use \c mainRenderTargetInfo instead.
	///
	/// \return
	/// 	The swapchain texture format (see \c Window::swapchainFormat), or \c SDL_GPU_TEXTUREFORMAT_INVALID if the
	/// 	player has no main window or the main window is not claimed by a device.
	[[nodiscard]] auto swapchainFormat () const -> SDL_GPUTextureFormat;

	/// \brief The current size of the main window viewport.
	///
	/// \return The viewport size in physical pixels, or <tt>(0, 0)</tt> if the player has no main window.
	[[nodiscard]] auto viewportSize () const -> glm::uvec2;

	/// \brief Convert a distance or position authored at 100% display scale into ImGui coordinates.
	///
	/// Both position components are measured from the main window's top-left corner. The conversion scales each
	/// component uniformly without translating the origin or rounding. User font enlargement and temporary style
	/// overrides do not affect it. A player without an attached GUI uses a scale of one.
	///
	/// \pre With an attached GUI, call from \c Applet::gui, \c Applet::update or \c Applet::render on the main thread.
	///
	/// \tparam T
	/// 	The value type. Supports \c float, \c double, \c ImVec2, \c glm::vec2 and \c glm::dvec2. Other non-integral
	/// 	types must support <tt>value * float</tt> with a result implicitly convertible to \c T.
	///
	/// \param value The distance or position at 100% display scale; zero and negative components are allowed.
	///
	/// \return The converted value, preserving its type and double-precision components.
	///
	/// \throws ... Exceptions from user-defined multiplication or result conversion propagate to the caller.
	template <class T>
	requires
		   (!std::integral<T>)
		&& (   std::same_as<T, ImVec2> || std::same_as<T, glm::dvec2>
		    || requires (T value, float scale){{value*scale} -> std::convertible_to<T>;})
	[[nodiscard]] auto toGuiUnits (T value) const -> T
	{
		const float scale = guiUnitScale();
		if constexpr (std::same_as<T, ImVec2> || std::same_as<T, glm::dvec2>)
			return T{value.x * scale, value.y * scale};
		else
			return value * scale;
	}

	/// \brief Check whether at least one continuous redraw request is active.
	///
	/// \return \c true if the main loop currently runs continuously, \c false if it waits for events.
	[[nodiscard]] auto continuousRedrawRequested () const -> bool;

	/// \brief Check whether closing the application was requested.
	///
	/// \return
	/// 	\c true after a call to \c requestClose, or after the main window was asked to close (e.g. by the user
	/// 	clicking its close button), \c false otherwise.
	[[nodiscard]] auto shouldClose () const -> bool;


	////
	// Methods

	/// \brief Request a scene color format, including from any applet callback.
	///
	/// Success means the request was accepted. The latest valid request wins; requesting the effective format cancels
	/// a pending change. Rejected requests preserve existing state. Requests from \c Applet::init are applied after
	/// all applets initialize and before pipeline creation. Other requests are applied before the next frame begins,
	/// including while minimized. All applets then receive \c Applet::recreatePipelines before resize notifications,
	/// input, GUI, updates and rendering. Requests from that callback wait until the next iteration.
	///
	/// \pre Call on the main thread with a claimed main window.
	///
	/// \param format A non-integer color format supporting single-sample 2D color-target and sampler usage.
	///
	/// \return
	/// 	Nothing when accepted; \c TextureErrorCode::InvalidArgument for an invalid format value,
	/// 	\c TextureErrorCode::UnsupportedFormat for unsuitable or unsupported formats, or
	/// 	\c TextureErrorCode::InvalidState when no claimed main window is available.
	///
	/// \throws std::logic_error If called off the main thread.
	[[nodiscard]] auto setSceneFormat (SDL_GPUTextureFormat format) -> std::expected<void, TextureError>;

	/// \brief Set the title of the main window.
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

	/// \brief Replace application-wide scene presentation with an owned presentation recorder.
	///
	/// The latest registration wins, destroying the previous captures. Changes before the next presentation point
	/// affect that frame, after all applet scene draws and before ImGui rendering. An empty recorder restores default
	/// conversion. Move-only and mutable callables are supported. Owned captures live until replacement, clearing
	/// or shutdown; \c fcg::run clears them before applet destruction. Failed callable construction leaves the
	/// previous registration intact. See <tt>\ref fcg_runtime_workflows_presentation</tt> for recording requirements.
	///
	/// \pre Use on the main thread. Captured references must remain valid while registered.
	///
	/// \param recorder The recorder to move into ownership, or an empty wrapper to restore default presentation.
	///
	/// \throws std::logic_error If registration is attempted during recorder invocation.
	void setPresentationRecorder (PresentationRecorder recorder);

	/// \brief Restore default scene conversion, destroying the registered recorder's captures.
	///
	/// Takes effect at the next presentation point and never restores an earlier registration. Clearing an empty
	/// registration has no effect. Built-in presentation uses the current \c gammaCorrection settings, which clearing
	/// preserves. Registrations are also cleared before applets are destroyed by \c fcg::run.
	///
	/// \pre Use on the main thread; referenced capture state must stay alive until clearing finishes.
	///
	/// \throws std::logic_error If clearing is attempted during recorder invocation.
	void clearPresentationRecorder ();

	/// \brief Request one refresh of the complete scene view.
	///
	/// Call after changing scene content, including the clear color, in an applet callback. Repeated requests before
	/// scene rendering are combined into one refresh. Requests before rendering affect the current drawable frame;
	/// requests during scene rendering, presentation or GUI overlay drawing affect the next drawable frame. Requests
	/// remain pending while no window image is available, for example while minimized.
	///
	/// The GUI and presentation run on every drawable iteration, reusing the scene until a refresh is required.
	/// Continuous redraw requests through \c pushContinuousRedraw also refresh the scene every iteration.
	///
	/// \pre Call on the main thread.
	void requireSceneRedraw () noexcept;

	/// \brief Add a continuous redraw request.
	///
	/// While at least one request is active, the main loop starts the next frame as soon as possible instead of
	/// waiting for events, and refreshes the scene on every drawable iteration. Use this e.g. while an applet is
	/// animating. Balance every call with a call to
	/// <tt>\ref popContinuousRedraw</tt> when continuous redrawing is not needed anymore.
	void pushContinuousRedraw ();

	/// \brief Remove a continuous redraw request previously added with <tt>\ref pushContinuousRedraw</tt>.
	///
	/// A call without a matching \c pushContinuousRedraw logs a warning and has no effect.
	void popContinuousRedraw ();

	/// \brief Request the application to close.
	///
	/// \c shouldClose returns \c true afterwards. If the player has a main window, the request is also forwarded to it
	/// (see \c Window::requestClose). \c fcg::run then leaves its main loop at the next opportunity and shuts down.
	void requestClose ();

	/// \brief Find a camera provided by one of the running applets.
	///
	/// Searches the running applets, in the order they were passed to \c fcg::run, for one that derives from
	/// <tt>\ref Camera</tt> and whose \c Applet::name equals \c name.
	///
	/// \param name The applet name of the camera to look for.
	///
	/// \return
	/// 	A pointer to the first matching camera, or \c std::nullopt if there is none. The camera is owned by
	/// 	\c fcg::run and stays valid until the applets are destroyed at shutdown.
	[[nodiscard]] auto findCamera (std::string_view name) -> std::optional<Camera*>;

	/// \brief Request a readback of the main viewport's depth buffer, i.e. a download of its contents to the CPU.
	///
	/// The readback captures the depth buffer from the most recently submitted scene refresh. Commands recorded in
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
	/// \return A token for retrieving the result with \c getDepthReadbackResult.
	///
	/// \throws std::runtime_error
	/// 	If no frame is currently being rendered (e.g. in \c Applet::init or while the window is minimized), or if
	/// 	the readback could not be started.
	/// \throws std::logic_error
	/// 	If an earlier readback of this player is in an inconsistent state after a failed retrieval.
	[[nodiscard]] auto scheduleDepthReadback () -> uint64_t;

	/// \brief Retrieve the result of a depth readback requested with <tt>\ref scheduleDepthReadback</tt>.
	///
	/// Blocks until the download has finished if it is still pending. Results are guaranteed to be available (i.e.,
	/// without waiting) in the next frame after the one in which the readback was requested.
	///
	/// The result covers the whole viewport at the time of the request, one 32-bit floating-point depth value per pixel
	/// (format \c SDL_GPU_TEXTUREFORMAT_D32_FLOAT). Read individual values with <tt>readTexel<float>({x, y, 0})</tt>,
	/// where <tt>(x, y)</tt> are pixel coordinates with the origin in the top-left corner. Depth values range from 0 to
	/// 1. Pixels not covered by any geometry keep the that the scene pass clears the depth buffer to.
	///
	/// \pre \c token was returned by \c scheduleDepthReadback and has not been invalidated since (see there).
	///
	/// \param token The token returned by \c scheduleDepthReadback.
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

	/// Query the attached GUI's retained coordinate scale, or one when no GUI is attached.
	[[nodiscard]] auto guiUnitScale () const noexcept -> float;

	/// Create the default recorder for the claimed main window. Returns a resource-creation error on failure; recorder
	/// allocation exceptions propagate. Called before applet initialization.
	[[nodiscard]] auto initializePresentation () -> std::expected<void, FullscreenError>;

	/// Release both recorders before applet destruction and window unclaim. No recorder may be executing.
	void shutdownPresentation () noexcept;

	/// Select custom or default presentation for an acquired frame; guard registration during invocation.
	[[nodiscard]] auto presentFrame () -> std::expected<void, FullscreenError>;

	/// Apply a queued format at a boundary without an active frame. Consume only on success; throw on failure.
	/// Returns whether the effective format changed.
	[[nodiscard]] auto applySceneFormat () -> bool;

	/// Destroy the current depth readback and its mapping (mapping first), invalidating all outstanding tokens. Called
	/// by \c fcg::run at startup and whenever the viewport size changes.
	void invalidateReadbacks ();

	/// Wait for and map a depth readback that was requested in the previous frame, then advance \c readbackToken to
	/// start a new frame. Called by \c fcg::run once per frame right after beginning it.
	void collectReadbackResults ();


	////
	// Fields

	/// Built-in sRGB recorder, retained across override replacement and clearing.
	PresentationRecorder m_defaultPresentationRecorder;

	/// Application-wide owning presentation recorder, cleared before applet teardown by the runtime.
	PresentationRecorder m_presentationRecorder;

	/// Whether the recorder is currently invoked; prevents destruction of its executing callable.
	bool presenting = false;

	/// The main rendering device.
	Device &m_device;

	/// The main window that applets can interact with through the player. Non-owning – the window is owned by whoever
	/// created the \c fcg::Player, (e.g., <tt>\ref fcg::run</tt>) and must outlive the player.
	Window *window = nullptr;

	/// Borrowed GUI, attached by the runtime and detached before its destruction, including on unwinding.
	Gui *gui = nullptr;

	/// The currently ongoing frame. Non-owning reference, managed externally.
	Frame *frame = nullptr;

	/// Latest validated request, owned by the player until the window accepts its immediate configuration change.
	std::optional<SDL_GPUTextureFormat> pendingSceneFormat;

	/// Whether the complete scene must be rendered at the next drawable opportunity.
	bool sceneRedrawRequired = true;

	/// The current clear color of the main window viewport, in linear RGBA space.
	glm::fvec4 m_clearColor = {0.0097f, 0.0331f, 0.1329f, 1.0f};

	/// Gamma and final RGB bounds read by the built-in sRGB recorder on every invocation.
	GammaUniforms m_gammaCorrection{.gamma = glm::vec3(1)};

	/// The number of currently active continuous redraw requests.
	std::atomic<unsigned> continuousRedrawRequests{0};

	/// Whether closing the application was requested on this player itself.
	std::atomic<bool> m_closeRequested{false};

	/// The pending or completed depth readback, if any. Kept at a stable address on the heap because
	/// <tt>\ref depthMapping</tt> refers into it; the mapping must therefore always be reset before this pointer.
	std::unique_ptr<TextureReadback> depthReadback;

	/// CPU access to the downloaded depth values of \c depthReadback, created once the download has finished. Destroyed
	/// before \c depthReadback.
	std::optional<TextureReadback::Mapping> depthMapping;

	/// The token handed out for \c depthReadback, i.e. the value of \c readbackToken in the frame it was requested.
	uint64_t depthToken = 0;

	/// Frame counter serving as the token for readbacks requested in the current frame. Incremented once per frame.
	uint64_t readbackToken = 0;

	/// The applets driven by the main loop, searched by \c findCamera.
	std::span<std::unique_ptr<Applet>> applets;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif  // ifndef __FCG_PLAYER_H__
