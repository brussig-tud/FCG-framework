
#ifndef __FCG_WINDOW_H__
#define __FCG_WINDOW_H__


//////
//
// Module documentation
//

/// \defgroup fcg_windows Windows and frames
/// \ingroup fcg_modules
/// \brief The application window, and per-frame render-pass recording through \c Frame.
///
/// <tt>\ref fcg::Window "Window"</tt> wraps an OS window that the GPU device renders into, and
/// <tt>\ref fcg::Frame "Frame"</tt> represents one acquired frame: it hands out the command buffer, the scene render
/// pass, and the scene and presentation textures. Applications run by <tt>\ref fcg::run "run"</tt> can usually skip
/// this module: the main window is created, claimed, and driven automatically, and applets only record into the render
/// pass they receive in <tt>\ref fcg::Applet::render "Applet::render"</tt>. Read on when managing a loop yourself, or
/// to query window properties.
///
/// <tt>\ref fcg::PresentationRecorder "PresentationRecorder"</tt> records the final window colors using the resources
/// supplied in <tt>\ref fcg::PresentationContext "PresentationContext"</tt>. Both types are declared in
/// <tt>\#include &lt;FCG/frame.h&gt;</tt> and are used for standalone frame loops and custom presentation through
/// <tt>\ref fcg::Player::setPresentationRecorder "Player::setPresentationRecorder"</tt>.
///
/// \section fcg_windows_usage Getting started
/// Include the window, device and frame interfaces:
/// \snippet window_examples.cpp includes
/// Create a window and claim it for a device:
/// \snippet window_examples.cpp window_setup
/// Create a presentation pass recorder once after claiming the window, and retain it across frames. The recorder is
/// responsible for recording commands that should eventually present the rendered scene to the main window viewport.
/// Here, it uses a <tt>\ref fcg::FullscreenPass</tt> and delegates the actual presentation command recording to it; in
/// this case, linear-to-sRGB conversion:
/// \snippet window_examples.cpp presentation_setup
/// Pass the recorder to <tt>fcg::Frame::present</tt> at the end of each iteration of your frame loop:
/// \snippet window_examples.cpp frame_loop
/// The local guard ends the scene pass even if drawing throws. The exception handler attempts to submit the acquired
/// frame before rethrowing the original exception. If submission also fails during cleanup, its error is logged and
/// the original exception is preserved.
///
/// \section fcg_windows_workflows Common tasks
/// \subsection fcg_windows_workflows_frame Recording a frame
/// A displayed scene frame consists of a scene pass, explicit presentation, and an optional overlay pass. Start
/// the scene pass with <tt>\ref fcg::Frame::beginRenderPass "Frame::beginRenderPass"</tt>, which clears the scene color
/// and depth, and end it with <tt>\ref fcg::Frame::endRenderPass "endRenderPass"</tt>. Call
/// <tt>\ref fcg::Frame::present "present"</tt> with a recorder that writes the final window colors. After successful
/// presentation, draw the GUI, or anything else that should appear on top of the presented image, in the depth-free
/// overlay pass from <tt>\ref fcg::Frame::beginOverlayRenderPass "beginOverlayRenderPass"</tt>. Finish with
/// <tt>\ref fcg::Window::endFrame "endFrame"</tt> to submit the recorded commands. Presenting without a completed
/// scene pass leaves the presented contents undefined.
///
/// \subsection fcg_windows_workflows_formats Querying formats
/// <tt>\ref fcg::Window::renderTargetInfo "Window::renderTargetInfo"</tt> returns the color format, depth format and
/// sample count of the scene render pass — create pipelines with these, as also reported by
/// <tt>\ref fcg::Player::mainRenderTargetInfo "Player::mainRenderTargetInfo"</tt>.
/// <tt>\ref fcg::Window::swapchainFormat "swapchainFormat"</tt> returns the presentation format.
///
/// \subsection fcg_windows_workflows_resize Resizing
/// <tt>\ref fcg::Window::viewportSize "Window::viewportSize"</tt> returns the current viewport size in pixels. Query
/// it each frame, or react to the size change in <tt>\ref fcg::Applet::onViewportResize
/// "Applet::onViewportResize"</tt>; the depth buffer and scene texture are recreated automatically.
///
/// Initial client dimensions in <tt>\ref fcg::WindowSettings "WindowSettings"</tt> are measured at 100% display
/// scale. For example, a requested 800-by-600 window uses a 1200-by-900 client area at 150% scale on Windows.
/// Moving to a different display scale preserves the window's size relative to its GUI. User resizing establishes
/// a new size to preserve. Maximized, minimized and fullscreen windows are adjusted when restored to normal mode.
/// Set <tt>\ref fcg::WindowSettings::scaleToDisplay "WindowSettings::scaleToDisplay"</tt> to false to use native
/// screen-coordinate dimensions and disable automatic resizing:
/// \snippet window_examples.cpp native_size
/// In a custom event loop, call <tt>\ref fcg::Window::updateDisplayScale "Window::updateDisplayScale"</tt> after
/// processing SDL events and before starting the GUI frame. This call can block while a resize is synchronized.
/// Requested sizes are subject to window-manager restrictions and can exceed the usable desktop area.
///
/// \note
/// On X11, display scaling follows the desktop-wide scale reported by SDL; moving between monitors need not change
/// that scale. On macOS and Wayland, changing pixel density can increase the viewport resolution while keeping
/// the client dimensions in window coordinates unchanged.
///
/// \section fcg_windows_lifetime Ownership and lifetime
/// Windows are created with <tt>\ref fcg::Window::create "Window::create"</tt> and held in a
/// <tt>std::unique_ptr</tt>. A window must be claimed before frames can be acquired, and a claimed window must outlive
/// the device that claimed it, or be released with <tt>\ref fcg::Device::unclaimWindow "Device::unclaimWindow"</tt>
/// first; destroying a claimed window terminates the application. A <tt>\ref fcg::Frame "Frame"</tt> is never owned by
/// the application: it is obtained from <tt>\ref fcg::Window::beginFrame "beginFrame"</tt> and stays valid until
/// <tt>\ref fcg::Window::endFrame "endFrame"</tt>.
///
/// \section fcg_windows_errors Handling errors
/// <tt>\ref fcg::Window::create "create"</tt> returns \c nullptr on failure, logged to the SDL log.
/// <tt>\ref fcg::Window::beginFrame "beginFrame"</tt> returns \c nullptr when no frame could be acquired (e.g. while
/// the window is minimized); skip rendering in that case. Misuse such as overlapping passes or ending a frame with
/// an active pass throws \c std::logic_error, and GPU-level acquisition failures throw \c std::runtime_error.
/// <tt>\ref fcg::Frame::present "Frame::present"</tt> returns presentation errors and propagates recorder exceptions.
/// After a recorder fails, finish the frame with <tt>\ref fcg::Window::endFrame "endFrame"</tt>; its displayed contents
/// are unspecified. Ending a frame without presenting also leaves its displayed contents unspecified.
///
/// \see \ref fcg_devices, \ref fcg_render_state, \ref fcg_runtime



//////
//
// Includes
//

// C++ STL
#include <memory>
#include <string>
#include <optional>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/render_target.h"
#include "FCG/frame.h"

// SDL3 library (SDL_GPUTextureFormat in the public API)
#include <SDL3/SDL_gpu.h>



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_Window;
struct SDL_GPUTexture;

// Framework types
namespace fcg {
	class Device;
	class Frame;
	namespace detail {
		class WindowScaling;
	}
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_windows
/// @{



//////
//
// Structs and enums
//

/// Creation parameters for a <tt>\ref Window</tt>.
struct WindowSettings
{
	/// The text shown in the window title bar.
	std::string title = "FCG Window";

	/// The initial client width at 100% display scale, or in screen coordinates when \c scaleToDisplay is false.
	unsigned width = 1280;

	/// The initial client height at 100% display scale, or in screen coordinates when \c scaleToDisplay is false.
	unsigned height = 720;

	/// Whether the user should be able to resize the window.
	bool resizable = true;

	/// Scale the client dimensions with the display at creation and on display-scale changes, including for
	/// non-resizable windows. The window manager may constrain the requested dimensions.
	bool scaleToDisplay = true;
};



//////
//
// Classes
//

/// \brief
/// 	An operating-system window that can be rendered into with a <tt>\ref fcg::Device</tt>.
///
/// Before rendering, the window must be claimed by a device with \c Device::claimWindow. Each frame is then rendered
/// by calling <tt>\ref beginFrame</tt>, recording render passes on the returned <tt>\ref Frame</tt>, and calling
/// <tt>\ref endFrame</tt>.
///
/// A claimed window must outlive the device that claimed it, or be released with \c Device::unclaimWindow before it
/// is destroyed.
///
/// Applications using \c fcg::run don't need to create, claim or drive a window themselves: \c fcg::run does this for
/// the main window, which applets can access through \c Player::mainWindow.
class FCG_FRAMEWORK_EXPORT Window
{
	////
	// Friend declarations

	/// Needs access to claim and release the window in \c Device::claimWindow and \c Device::unclaimWindow.
	friend class Device;


public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Create a new window.
	///
	/// The window is created with support for high pixel densities, so its viewport size in pixels can exceed the
	/// size given in screen coordinates. With \c WindowSettings::scaleToDisplay enabled, initial client dimensions
	/// are scaled for the display before showing the window. Failures are logged to the SDL log.
	///
	/// \param settings
	/// 	The creation parameters.
	///
	/// \return The new window, or \c nullptr if window creation failed.
	[[nodiscard]] static auto create (const WindowSettings &settings = {}) -> std::unique_ptr<Window>;

	/// \brief
	/// 	Destroy the window.
	///
	/// \pre
	/// 	The window is not claimed by a device. Destroying a claimed window logs a critical error and terminates the
	/// 	program. Release the claim with \c Device::unclaimWindow first, or destroy the claiming device first.
	~Window();

	/// Not copyable.
	Window(const Window&) = delete;

	/// Not copy-assignable.
	auto operator= (const Window&) -> Window& = delete;


	////
	// Accessors

	/// The raw SDL window handle, owned by this window and valid for its lifetime.
	[[nodiscard]] auto handle () const -> SDL_Window*;

	/// The SDL window ID, e.g. for matching SDL window events to this window.
	[[nodiscard]] auto id () const -> unsigned;

	/// \brief
	/// 	The current size of the viewport of this window.
	///
	/// The size is updated by <tt>\ref beginFrame</tt> to the size of the acquired window image, and by
	/// <tt>\ref pollViewportSize</tt>.
	///
	/// \return The viewport size in physical pixels, or <tt>(0, 0)</tt> before the size was first determined.
	[[nodiscard]] auto viewportSize () const -> const glm::uvec2& {
		return m_viewportSize;
	}

	/// \brief
	/// 	The texture format of the images shown in this window.
	///
	/// Only pipelines that draw directly into the window image, such as GUI overlays in
	/// \c Frame::beginOverlayRenderPass, need this format. Pipelines rendering the scene use
	/// <tt>\ref renderTargetInfo</tt> instead.
	///
	/// \return
	/// 	The swapchain texture format, which is either \c SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM or
	/// 	\c SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, or \c SDL_GPU_TEXTUREFORMAT_INVALID if the window is not claimed
	/// 	by a device.
	[[nodiscard]] auto swapchainFormat () const -> SDL_GPUTextureFormat;

	/// \brief
	/// 	Describe the color and depth attachments of the scene render pass (see \c Frame::beginRenderPass).
	///
	/// Use this to create graphics pipelines for rendering the scene. The description is available as soon as the
	/// window is claimed, i.e. also before the first frame and while the window is minimized.
	///
	/// \return
	/// 	The attachment formats and sample count, or \c std::nullopt if the window is not claimed by a device.
	[[nodiscard]] auto renderTargetInfo () const -> std::optional<RenderTargetInfo>;


	////
	// Methods

	/// \brief Adjust the normal window's client dimensions to the current display scale.
	///
	/// Call after processing SDL events and before starting a GUI frame in a custom loop. The framework calls this
	/// automatically in \c fcg::run, \c beginFrame and \c pollViewportSize. User resizing establishes a new size to
	/// preserve across display-scale changes. Minimized, maximized and fullscreen windows are adjusted on restoration.
	/// The window manager may constrain the requested size. With \c WindowSettings::scaleToDisplay disabled, this
	/// call has no effect. Failed display queries are retried on the next call; resize failures are logged.
	/// Synchronizing a resize can block until the window manager responds or SDL times out.
	///
	/// \pre Call on the main thread.
	void updateDisplayScale ();

	/// \brief
	/// 	Update the viewport size from the current size of the window in pixels.
	///
	/// Called by \c fcg::run during startup and in iterations without an acquired frame; applications normally don't
	/// call this. While rendering, \c beginFrame supplies the acquired image's dimensions instead. Call this when
	/// \c viewportSize must be up to date while no frames are being rendered, e.g. after a resize event. If querying
	/// the window size fails, the error is logged and the viewport size is left unchanged.
	///
	/// \param oldSize
	/// 	Receives the previous viewport size if it changed, or is reset to \c std::nullopt otherwise.
	///
	/// \return \c true if the viewport size changed, \c false otherwise.
	auto pollViewportSize (std::optional<glm::uvec2> &oldSize) -> bool;

	/// \brief
	/// 	Begin rendering the next frame of this window.
	///
	/// Waits until the next window image is available, which can block. The returned <tt>\ref Frame</tt> provides a
	/// command buffer, the scene texture, the window image and a depth texture of matching size. It is owned by the
	/// window and stays valid until <tt>\ref endFrame</tt> is called. Also updates <tt>\ref viewportSize</tt>.
	///
	/// \pre
	/// 	No other frame of this window is in progress.
	///
	/// \param device
	/// 	The GPU device to render the frame with. It must currently claim this window.
	///
	/// \return
	/// 	The new frame, or \c nullptr if no frame can be rendered right now (e.g. while the window is minimized). In
	/// 	the latter case, skip rendering and do not call \c endFrame.
	///
	/// \throws std::logic_error
	/// 	If another frame of this window is still in progress, or if \c device does not claim this window.
	/// \throws std::runtime_error
	/// 	If acquiring the command buffer or the window image fails, or if the depth or scene texture cannot be
	/// 	created.
	auto beginFrame (Device &device) -> Frame*;

	/// \brief
	/// 	Finish the current frame and submit it to the GPU for display.
	///
	/// Submission displays the window image recorded by \c Frame::present. If presentation was omitted or failed,
	/// the displayed contents are unspecified. Afterwards, the <tt>\ref Frame</tt> returned by <tt>\ref beginFrame</tt>
	/// is invalid, including when submission fails. Submission failures are logged to the SDL log and throw
	/// \c std::runtime_error. Pre-submission logic errors leave an existing frame in progress.
	///
	/// \pre
	/// 	A frame of this window is in progress and has no active render pass.
	///
	/// \throws std::logic_error
	/// 	If no frame is in progress, a render pass is still active, or a presentation recorder is executing.
	/// \throws std::runtime_error
	/// 	If submitting the command buffer fails. The frame is finished and must not be used or submitted again.
	void endFrame ();

	/// \brief
	/// 	Set the text shown in the window title bar.
	///
	/// Failures are logged to the SDL log.
	///
	/// \note
	/// 	SDL requires window titles to be set on the main thread.
	///
	/// \param title
	/// 	The new window title.
	void setTitle (const std::string &title);

	/// \brief
	/// 	Request the window to close.
	///
	/// Afterwards, <tt>\ref shouldClose</tt> returns \c true. The window itself stays open until it is destroyed. For
	/// the main window, \c fcg::run leaves its main loop at the next opportunity and shuts the application down.
	void requestClose () {
		closeRequested = true;
	}

	/// \brief
	/// 	Check whether closing the window was requested.
	///
	/// \return \c true after a call to <tt>\ref requestClose</tt>, \c false otherwise.
	[[nodiscard]] auto shouldClose () const -> bool {
		return closeRequested;
	}


private:

	////
	// Object construction

	/// Private default constructor. Windows are created via <tt>\ref create</tt>.
	Window();


	////
	// Methods

	/// Claim the window for the given device and create the shared scene sampler. Returns \c false if SDL or
	/// resource creation fails, and throws \c std::logic_error if the window is already claimed.
	auto claim (Device &device) -> bool;

	/// Remove the claim of the given device to this window. Waits for the GPU to become idle, then releases the
	/// depth texture, scene texture and shared scene sampler. Throws \c std::logic_error if \c device does not
	/// claim this window.
	void unclaim (Device &device);


	////
	// Fields

	/// Depth format used by both target queries and depth-texture allocation.
	static constexpr SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

	/// Sample count used by both target queries and depth-texture allocation.
	static constexpr SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1;

	/// The SDL window handle.
	SDL_Window *m_handle = nullptr;

	/// Normal-window sizing policy, absent when automatic display scaling is disabled.
	std::unique_ptr<detail::WindowScaling> m_scaling;

	/// The device currently claiming this window, if any.
	Device *m_device = nullptr;

	/// The scene color texture in sRGB-encoded RGBA8 storage; rendering writes and sampling reads linear values.
	/// Recreated together with the depth texture whenever the swapchain size changes.
	std::optional<Texture> sceneTexture;

	/// Nearest-neighbor, clamp-to-edge sampler used for presentation.
	std::optional<Sampler> presentationSampler;

	/// A depth buffer suitable for rendering to the swapchain images of this \c Window.
	SDL_GPUTexture *depthTexture = nullptr;

	/// The size of the current <tt>\ref depthTexture</tt>, if any. Tracked separately from
	/// <tt>\ref m_viewportSize</tt> because the depth buffer only needs to be recreated when the actual swapchain
	/// texture size changes.
	glm::uvec2 m_depthTextureSize = { 0, 0 };

	/// The current frame in flight, if any.
	std::optional<Frame> m_frame;

	/// The current dimensions of the viewport of this \c Window.
	glm::uvec2 m_viewportSize = { 0, 0 };

	/// Whether closing the window was requested.
	bool closeRequested = false;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif  // ifndef __FCG_WINDOW_H__
