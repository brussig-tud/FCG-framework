
#ifndef __FCG_GUI_H__
#define __FCG_GUI_H__


//////
//
// Module documentation
//

/// \defgroup fcg_gui GUI
/// \ingroup fcg_modules
/// \brief Dear ImGui widgets drawn on top of the scene, defined in the applets' \c gui callbacks.
///
/// The framework integrates <em>Dear ImGui</em>: every main-loop iteration, each applet defines its widgets in
/// <tt>\ref fcg::Applet::gui "Applet::gui"</tt>, and the framework renders the combined widgets on top of the presented
/// scene. The <tt>\ref fcg::Gui "Gui"</tt> class drives this integration; it is created and owned by
/// <tt>\ref fcg::run "run"</tt>, and applications normally do not interact with it directly.
///
/// \section fcg_gui_usage Getting started
/// Issue \c ImGui:: calls in <tt>\ref fcg::Applet::gui "Applet::gui"</tt>; the ImGui context is current during that
/// callback:
/// \snippet gui_examples.cpp gui_widgets
/// Do not call <tt>ImGui::Render()</tt> or <tt>ImGui::NewFrame()</tt> yourself; the framework does.
///
/// \section fcg_gui_workflows Common tasks
/// \subsection fcg_gui_workflows_input Input capture
/// While the GUI uses the keyboard, keyboard events are withheld from the applets; the same applies to mouse events
/// while the GUI uses the mouse. In particular, pressing Escape does not close the application while a text field is
/// being edited.
///
/// \subsection fcg_gui_workflows_scaling Display scaling
/// On platforms with logical window coordinates, widgets keep their size relative to the window and rendering uses the
/// additional framebuffer pixels automatically. On platforms whose window coordinates are physical pixels, the GUI
/// follows the operating system's content scale. Existing GUI windows scale with their contents when moving between
/// displays. The bitmap default font is used at its native size on a 1x framebuffer; other sizes use a scalable font.
///
/// Customize the ImGui style in <tt>\ref fcg::Applet::init "Applet::init"</tt> using unscaled dimensions. Later changes
/// to dimensions use the current window-coordinate scale and are preserved when moving between displays. Colors and
/// other style settings can be edited directly at any scale. Use <tt>ImGui::GetStyle().FontScaleMain</tt> for user font
/// enlargement; <tt>ImGui::GetStyle().FontScaleDpi</tt> is managed by the framework.
///
/// Express widget sizes and positions relative to font or style dimensions, as in this example:
/// \snippet gui_examples.cpp gui_relative_size
/// Sizes and positions explicitly supplied by an applet take precedence over automatic window scaling. Temporary
/// style overrides must be balanced before the end of the frame.
///
/// \section fcg_gui_lifetime Ownership and lifetime
/// The <tt>\ref fcg::Gui "Gui"</tt> instance is created by <tt>\ref fcg::run "run"</tt> after the main window has been
/// claimed by the GPU device, and is destroyed after the applets, which therefore can use ImGui until their
/// destructors run. Applications never create a GUI themselves.
///
/// \see \ref fcg_applets, \ref fcg_events, \ref fcg_runtime



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
struct SDL_Window;
struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPURenderPass;

// Framework types
namespace fcg {
	class Device;
	class Window;
	class Frame;
	namespace detail { // implementation details
		class GuiScaling;
	}
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_gui
/// @{



//////
//
// Classes
//

/// \brief
/// 	The framework's <em>Dear ImGui</em> integration: owns the ImGui context and connects it to the main window's
/// 	input events and to the GPU device for rendering.
///
/// <tt>\ref fcg::run "run"</tt> creates one instance after setting up the main window and drives it every
/// main-loop iteration: it feeds all SDL events to <tt>\ref processEvent "processEvent"</tt>, starts a new ImGui frame
/// with <tt>\ref newFrame "newFrame"</tt> before the applets' <tt>\ref fcg::Applet::gui "Applet::gui"</tt> callbacks,
/// and renders the result on top of the presented scene. While an instance exists, its ImGui context is the current
/// one, so applets can issue ImGui calls directly. Applications normally don't use this class themselves.
///
/// Because ImGui has a single current context, at most one instance may exist at a time. In particular, applications
/// run by \c fcg::run must not create another one.
class FCG_FRAMEWORK_EXPORT Gui
{
public:

	////
	// Constants

	/// The interval, in milliseconds, at which \c fcg::run redraws while
	/// <tt>\ref needsPeriodicRedraw "needsPeriodicRedraw"</tt> returns \c true, even if no input arrives.
	static constexpr int periodicRedrawIntervalMs = 400;


	////
	// Object construction/destruction

	/// \brief
	/// 	Creates the GUI for the given main window. Called by \c fcg::run; applications normally don't call this.
	///
	/// The GUI is set up for the dark ImGui style with keyboard navigation enabled. Its style scale and default font
	/// follow the window's display scale and are updated automatically when it changes.
	///
	/// \pre
	/// 	\p mainWindow is claimed by \p device (see <tt>\ref fcg::Device::claimWindow "Device::claimWindow"</tt>). No
	/// 	other \c Gui instance exists.
	///
	/// \param device The GPU device used for rendering the GUI. It must outlive the returned object.
	/// \param mainWindow The window whose input the GUI processes and into which it is rendered.
	///
	/// \return The new GUI, or \c nullptr if initialization failed. The reason is logged to the SDL log.
	[[nodiscard]] static auto create (Device &device, Window &mainWindow) -> std::unique_ptr<Gui>;

	/// \brief
	/// 	Waits until the GPU is idle, then shuts down the GUI and destroys its ImGui context.
	///
	/// \pre
	/// 	The GPU device passed to <tt>\ref create "create"</tt> is still alive.
	~Gui();

	/// Not copyable.
	Gui(const Gui&) = delete;

	/// Not copy-assignable.
	auto operator= (const Gui&) -> Gui& = delete;


	////
	// Accessors

	/// \brief
	/// 	Whether a GUI text field is currently being edited. While this is \c true, key presses belong to the text
	/// 	field; in particular, Escape does not close the application.
	///
	/// \return \c true while the GUI wants text input.
	[[nodiscard]] auto wantsTextInput () const -> bool;

	/// \brief
	/// 	Whether the GUI currently captures keyboard input, for example while a widget has keyboard focus.
	///
	/// While this is \c true, \c fcg::run does not deliver keyboard events to
	/// <tt>\ref fcg::Applet::onEvent "Applet::onEvent"</tt>.
	///
	/// \return \c true while the GUI captures the keyboard.
	[[nodiscard]] auto wantsKeyboard () const -> bool;

	/// \brief
	/// 	Whether the GUI currently captures mouse input, for example while the cursor is over a GUI window or a
	/// 	widget is being dragged.
	///
	/// While this is \c true, \c fcg::run does not deliver mouse motion, button and wheel events to
	/// <tt>\ref fcg::Applet::onEvent "Applet::onEvent"</tt>.
	///
	/// \return \c true while the GUI captures the mouse.
	[[nodiscard]] auto wantsMouse () const -> bool;

	/// \brief
	/// 	Whether the GUI needs periodic redraws for a blinking text caret or a pending display-scale query retry.
	///
	/// While this is \c true, \c fcg::run redraws at least every
	/// <tt>\ref periodicRedrawIntervalMs "periodicRedrawIntervalMs"</tt> milliseconds.
	///
	/// \return \c true while periodic redraws are needed.
	[[nodiscard]] auto needsPeriodicRedraw () const -> bool;


	////
	// Methods

	/// Passes an SDL event to the GUI so it can react to input. Called by \c fcg::run for every SDL event;
	/// applications normally don't call this.
	///
	/// \param event The SDL event to process.
	void processEvent (const SDL_Event &event);

	/// \brief
	/// 	Begins a new ImGui frame. Called by \c fcg::run once per main-loop iteration, before the applets'
	/// 	<tt>\ref fcg::Applet::gui "Applet::gui"</tt> callbacks; applications normally don't call this.
	///
	/// \param frame
	/// 	The frame into which this GUI will be rendered, or \c nullptr when none is available. Its framebuffer extent
	/// 	is used for text rasterization. The frame must remain alive until <tt>\ref prepareRender "prepareRender"</tt>.
	void newFrame (Frame *frame = nullptr);

	/// \brief
	/// 	Ends the current ImGui frame and prepares its draw data for rendering. Called by \c fcg::run after all
	/// 	applets had their <tt>\ref fcg::Applet::gui "Applet::gui"</tt> turn; applications normally don't call this.
	///
	/// Must be called once for every <tt>\ref newFrame "newFrame"</tt>, also when no frame could be started. If
	/// \p frame is not \c nullptr, the draw data is uploaded using the frame's command buffer, so the call must be made
	/// while no render pass of that frame is active and before <tt>\ref renderDrawData "renderDrawData"</tt>.
	///
	/// \param frame The current frame, or \c nullptr if no frame could be started (e.g. while the window is minimized).
	void prepareRender (Frame *frame);

	/// \brief
	/// 	Records the GUI draw calls into the given render pass. Called by \c fcg::run after the scene has been
	/// 	presented; applications normally don't call this.
	///
	/// \pre
	/// 	<tt>\ref prepareRender "prepareRender"</tt> was called with the same frame, and \p renderPass is that frame's
	/// 	overlay pass obtained from <tt>\ref fcg::Frame::beginOverlayRenderPass "Frame::beginOverlayRenderPass"</tt>.
	///
	/// \param commandBuffer The command buffer of the current frame.
	/// \param renderPass The overlay render pass of the current frame.
	void renderDrawData (SDL_GPUCommandBuffer *commandBuffer, SDL_GPURenderPass *renderPass);


private:

	////
	// Object construction

	/// Private default constructor. GUIs are created via <tt>\ref create "create"</tt>.
	Gui();


	////
	// Fields

	/// The GPU device the renderer backend was initialized with. Referenced, not owned; the device must outlive the
	/// GUI.
	SDL_GPUDevice *m_device = nullptr;

	/// The main window. Referenced, not owned; the window must outlive the GUI.
	SDL_Window *m_window = nullptr;

	/// Display scaling, borrowed default fonts and normalized GUI geometry, owned by this GUI.
	std::unique_ptr<detail::GuiScaling> m_scaling;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif  // ifndef __FCG_GUI_H__
