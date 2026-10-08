
/**
 * \defgroup fcg_windows Windows and frames
 * \ingroup fcg_components
 *
 * <code>\ref fcg::Window</code> and <code>\ref fcg::WindowSettings</code> describe windows and their creation settings.
 * <code>\ref fcg::Frame</code> exposes frame recording and render passes.
 *
 * \par Guide incomplete
 * This guide is a stub. Consult the API declarations below for currently documented behavior.
 *
 * \section fcg_windows_workflows Common workflows
 * Guide incomplete: workflow descriptions remain to be investigated and written.
 *
 * \section fcg_windows_lifetime Ownership and lifetime
 * Guide incomplete: consult individual type and member contracts.
 *
 * \section fcg_windows_errors Errors
 * Guide incomplete: error handling remains to be investigated and written.
 *
 * \section fcg_windows_examples Examples
 * Guide incomplete: worked examples remain to be added and compiled.
 *
 * \see \ref fcg_devices, \ref fcg_render_state, \ref fcg_runtime
 */


#ifndef __FCG_WINDOW_H__
#define __FCG_WINDOW_H__


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
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/** \addtogroup fcg_windows
 * @{
 */



//////
//
// Structs
//

/// Creation parameters for a <code>\ref Window</code>.
struct WindowSettings
{
	/// The text shown in the window title bar.
	std::string title = "FCG Window";

	/// The initial width of the window, in screen coordinates.
	unsigned width = 1280;

	/// The initial height of the window, in screen coordinates.
	unsigned height = 720;

	/// Whether the user should be able to resize the window.
	bool resizable = true;
};



//////
//
// Classes
//

/// A window that can be rendered into using a \link fcg::Device GPU device \endlink.
class FCG_FRAMEWORK_EXPORT Window
{
	////
	// Friend declarations

	// For claiming and unclaiming the window
	friend class Device;


public:

	////
	// Object construction/destruction

	/// Create a new window.
	/// \returns The window, or `nullptr` if window creation failed.
	[[nodiscard]] static auto create (const WindowSettings &settings = {}) -> std::unique_ptr<Window>;

	/// The destructor. Releases the window from the GPU device it was last rendered with, if any, and destroys
	/// the window. The GPU device itself is not touched beyond that – it is owned elsewhere and must outlive
	/// the window.
	~Window();

	/// Windows are not copyable.
	Window(const Window&) = delete;

	/// Windows are not copy-assignable.
	auto operator= (const Window&) -> Window& = delete;


	////
	// Accessors

	/// The raw SDL window handle.
	[[nodiscard]] auto handle () const -> SDL_Window*;

	/// The SDL window ID, e.g. for matching window events to this window.
	[[nodiscard]] auto id () const -> unsigned;

	/// The current dimensions of the viewport of this window.
	[[nodiscard]] auto viewportSize () const -> const glm::uvec2& {
		return m_viewportSize;
	}

	/// The texture format of this window's swapchain images, for presentation; scene pipelines use the main render-target metadata. The window must be claimed by a device.
	[[nodiscard]] auto swapchainFormat () const -> SDL_GPUTextureFormat;

	/// Attachment description available before rendering and while minimized; absent without a valid claim.
	[[nodiscard]] auto renderTargetInfo () const -> std::optional<RenderTargetInfo>;

	/// Update the stored viewport dimensions from the current window drawable size. In a blocking main loop,
	/// rendering does not necessarily happen right after a resize event, so this should be polled once per
	/// iteration to keep the viewport dimensions fresh for users that query them outside of rendering (e.g.
	/// for displaying them in the GUI).
	///
	/// \param oldSize Receives the dimensions before the update when they changed, or is reset to `std::nullopt`
	///                when they didn't.
	///
	/// \returns `true` if the viewport size changed, `false` otherwise.
	auto pollViewportSize (std::optional<glm::uvec2> &oldSize) -> bool;


	////
	// Methods

	/// Begin the next \link fcg::Frame frame \endlink, which will contain th acquires swapchain texture of this window
	/// as target, a depth buffer of matching size, and a command buffer to begin render passes on.
	///
	/// \param device The GPU device to render the frame with. Must have a current claim to this window.
	///
	/// \returns The new frame ready for rendering, or `nullptr` when no frame can be rendered right now (for
	/// example when the window is minimized). In the latter case, rendering must be skipped.
	auto beginFrame (Device &device) -> Frame*;

	/// End the current frame, releasing its resources after submitting the associated command buffer. The main reason
	/// for requiring a call to this function is to establish an explicit workflow, violations of which can be easily
	/// detected and thus avoid spurious or hard to track down bugs/crashes.
	void endFrame ();

	/// Set the text shown in the window title bar.
	/// Note that SDL requires window titles to be set on the main thread.
	///
	/// \param title The new window title.
	void setTitle (const std::string &title);

	/// Request the window to be closed. In case of the main window, <code>\ref fcg::run</code> will destroy it at the next
	/// opportunity.
	void requestClose () {
		closeRequested = true;
	}

	/// Check whether closing the window was requested, e.g. by the user clicking its close button.
	[[nodiscard]] auto shouldClose () const -> bool {
		return closeRequested;
	}


private:

	////
	// Object construction

	/// Private default constructor. Windows are created via <code>\ref create</code>.
	Window() = default;


	////
	// Methods

	/// Claim the window for the given <code>\ref fcg::Device</code>.
	auto claim (Device &device) -> bool;

	/// Remove the claim of the given device to this window.
	void unclaim (Device &device);


	////
	// Fields

	/// Depth format used by both target queries and depth-texture allocation.
	static constexpr SDL_GPUTextureFormat depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

	/// Sample count used by both target queries and depth-texture allocation.
	static constexpr SDL_GPUSampleCount samples = SDL_GPU_SAMPLECOUNT_1;

	/// The SDL window handle.
	SDL_Window *m_handle = nullptr;

	/// The device currently claiming this window, if any.
	Device *m_device = nullptr;

	/// Canonical sRGB8 scene storage; sampling decodes to linear space.
	std::optional<Texture> sceneTexture;

	/// Reusable encoder targeting the UNORM swapchain format.
	std::optional<FullscreenPass> presentation;

	/// Reusable filtered, clamped presentation sampler.
	std::optional<Sampler> presentationSampler;

	/// A depth buffer suitable for rendering to the swapchain images of this \c Window.
	SDL_GPUTexture *depthTexture = nullptr;

	/// The size of the current <code>\ref depthTexture</code>, if any. Tracked separately from
	/// <code>\ref m_viewportSize</code> because the depth buffer only needs to be recreated when the actual swapchain
	/// texture size changes.
	glm::uvec2 m_depthTextureSize = { 0, 0 };

	/// The current frame in flight, if any
	std::optional<Frame> m_frame;

	/// The current dimensions of the viewport of this \c Window.
	glm::uvec2 m_viewportSize = { 0, 0 };

	/// Whether closing the window was requested.
	bool closeRequested = false;
};



/** @} */

//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_WINDOW_H__
