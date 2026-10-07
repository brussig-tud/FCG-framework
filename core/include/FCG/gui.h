/**
 * \defgroup fcg_gui GUI
 * \ingroup fcg_components
 *
 * <code>\ref fcg::Gui</code> integrates Dear ImGui with the framework runtime and SDL backends. Applets contribute GUI
 * content through <code>\ref fcg::Applet::gui</code>.
 *
 * \par Guide incomplete
 * This guide is a stub. Consult the API declarations below for currently documented behavior.
 *
 * \section fcg_gui_workflows Common workflows
 * Guide incomplete: workflow descriptions remain to be investigated and written.
 *
 * \section fcg_gui_lifetime Ownership and lifetime
 * Guide incomplete: consult individual type and member contracts.
 *
 * \section fcg_gui_errors Errors
 * Guide incomplete: error handling remains to be investigated and written.
 *
 * \section fcg_gui_examples Examples
 * Guide incomplete: worked examples remain to be added and compiled.
 *
 * \see \ref fcg_runtime, \ref fcg_applets, \ref fcg_windows
 */


#ifndef __FCG_GUI_H__
#define __FCG_GUI_H__


//////
//
// Includes
//

// C++ STL
#include <memory>

// Local includes
#include "FCG/export.h"


//////
//
// Forward declarations
//

// Opaque SDL3 types
union SDL_Event;
struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPURenderPass;

// Framework types
namespace fcg {
	class Device;
	class Window;
	class Frame;
}




//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/** \addtogroup fcg_gui
 * @{
 */



//////
//
// Classes
//

/// Encapsulates the framework-wide \em Dear \em ImGui context and its SDL3 backends.
///
/// An instance of this class is created by <code>\ref fcg::run</code> after the main window has been claimed for the
/// GPU device, and is destroyed before the window is unclaimed. While it is alive, applet <code>\ref Applet::gui</code>
/// callbacks are invoked with the framework *ImGui* context current, so applets can issue *ImGui* calls directly.
class FCG_FRAMEWORK_EXPORT Gui
{
public:

	////
	// Constants

	/// The interval, in milliseconds, after which <code>\ref fcg::run</code> redraws while
	/// <code>\ref needsPeriodicRedraw</code> is `true`. Chosen to match the shortest visible/hidden transition gap of the
	/// blinking *ImGui* text caret.
	static constexpr int periodicRedrawIntervalMs = 400;


	////
	// Object construction/destruction

	/// Create the framework GUI for the given main window. The window must already be claimed for the device.
	///
	/// \param device The shared GPU device.
	/// \param mainWindow The main window the GUI will be rendered into.
	///
	/// \returns The new GUI, or `nullptr` if initialization failed.
	[[nodiscard]] static auto create (Device &device, Window &mainWindow) -> std::unique_ptr<Gui>;

	/// The destructor. Waits for the GPU to go idle, then shuts down the *ImGui* backends and destroys the
	/// framework *ImGui* context.
	~Gui();

	/// \c Gui is not copyable.
	Gui(const Gui&) = delete;

	/// \c Gui is not copy-assignable.
	auto operator= (const Gui&) -> Gui& = delete;


	////
	// Methods

	/// Feed an SDL event to the *ImGui* SDL3 platform backend. Must be called for every event that the main loop
	/// dispatches, so the GUI can react to input.
	///
	/// \param event The SDL event to process.
	void processEvent (const SDL_Event &event);

	/// Apply the display content scale of the given window's monitor to the *ImGui* style, so widgets are sized
	/// according to the system DPI scaling. Queries the current scale and only touches the *ImGui* style when it
	/// changed since the last call, so it is cheap to call in reaction to every event that might indicate a
	/// change (e.g. \c SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED or the window being moved to another monitor).
	/// Called once during <code>\ref create</code> to establish the initial scale.
	///
	/// \param window The window whose current display content scale should be applied.
	void updateContentScale (const Window &window);

	/// Begin a new *ImGui* frame. Called by <code>\ref fcg::run</code> once per main loop iteration, before the applet
	/// <code>\ref Applet::gui</code> callbacks are invoked.
	void newFrame ();

	/// Finalize the *ImGui* frame and prepare the draw data for rendering on the given frame's command buffer. Called by
	/// <code>\ref fcg::run</code> after all applets had their <code>\ref Applet::gui</code> turn. Safe to call with a
	/// `nullptr` frame (e.g. when the window is minimized), in which case no GPU preparation is performed.
	///
	/// \param frame The current frame, or `nullptr` if no frame could be started.
	void prepareRender (Frame *frame);

	/// Record the *ImGui* draw data into the given render pass. Must be called after the applet
	/// <code>\ref Applet::render</code> callbacks so the GUI is drawn on top, and only if a frame is actually in flight.
	///
	/// \param commandBuffer The command buffer of the current frame.
	/// \param renderPass The render pass targeting the main window swapchain texture.
	void renderDrawData (SDL_GPUCommandBuffer *commandBuffer, SDL_GPURenderPass *renderPass);

	/// Whether the GUI currently wants text input. While this is `true`, key presses should be considered
	/// consumed by the GUI (e.g. *Escape* should not close the window while a text field is being edited).
	[[nodiscard]] auto wantsTextInput () const -> bool;
	[[nodiscard]] auto wantsKeyboard () const -> bool;
	[[nodiscard]] auto wantsMouse () const -> bool;

	/// Whether the GUI currently needs periodic redraws even without any input events. This is the case while a
	/// text field with a blinking caret is active. A blocking main loop should wake up after
	/// <code>\ref periodicRedrawIntervalMs</code> at the latest in this situation.
	[[nodiscard]] auto needsPeriodicRedraw () const -> bool;


private:

	////
	// Object construction

	/// Private default constructor. GUIs are created via <code>\ref create</code>.
	Gui() = default;


	////
	// Fields

	/// The GPU device the renderer backend was initialized with. Referenced, not owned – the device is owned by
	/// <code>\ref fcg::run</code> and outlives the GUI.
	SDL_GPUDevice *m_device = nullptr;

	/// The display content scale most recently applied to the *ImGui* style. Used by <code>\ref updateContentScale</code>
	/// to detect changes and to rescale style sizes by the correct ratio.
	float m_contentScale = 1.0f;
};



/** @} */

//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_GUI_H__
