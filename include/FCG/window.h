
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
#include "FCG/frame.h"



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



//////
//
// Structs
//

/// Creation parameters for a \ref Window.
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

/// A window that can be rendered into using a \link GPU device fcg::Device \endlink.
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


	////
	// Methods

	/// Begin the next \link frame fcg::Frame \endlink, which will contain th acquires swapchain texture of this window
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

	/// Request the window to be closed. In case of the main window, \ref fcg::run will destroy it at the next
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

	/// Private default constructor. Windows are created via \ref create.
	Window() = default;


	////
	// Methods

	/// Claim the window for the given \ref fcg::Device.
	auto claim (Device &device) -> bool;

	/// Remove the claim of the given device to this window.
	void unclaim (Device &device);


	////
	// Fields

	/// The SDL window handle.
	SDL_Window *m_handle = nullptr;

	/// The device currently claiming this window, if any.
	Device *m_device = nullptr;

	/// A depth buffer suitable for rendering to the swapchain images of this \c Window.
	SDL_GPUTexture *depthTexture = nullptr;

	/// The current frame in flight, if any
	std::optional<Frame> m_frame;

	/// The current dimensions of the viewport of this \c Window.
	glm::uvec2 m_viewportSize;

	/// Whether closing the window was requested.
	bool closeRequested = false;
};



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_WINDOW_H__
