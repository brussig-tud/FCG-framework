
#ifndef __FCG_APPLET_H__
#define __FCG_APPLET_H__


//////
//
// Library documentation
//

/// \page fcg_core_overview Core library overview
///
/// Core is the foundation of the FCG Framework. It provides the application lifecycle (applets that are driven by
/// <tt>\ref fcg::run "run"</tt>), the main window and the GPU device, input events, GPU resources such as buffers,
/// textures and full-screen passes, the render state that is shared between applets, ready-made camera controls, and
/// an integrated Dear ImGui GUI.
///
/// Link your application against \verbatim FCG-Framework::Core \endverbatim and include the headers of the modules you
/// use. All public headers live directly under \c FCG/, e.g. <tt>\#include &lt;FCG/applet.h&gt;</tt>.
///
/// \section fcg_core_start Where to start
/// Most applications implement one or more applets and pass them to <tt>\ref fcg::run "run"</tt>. Where to go from
/// there depends on what you want to build:
/// - Write an applet and run it: start with \ref fcg_applets and \ref fcg_runtime.
/// - React to keyboard, mouse or file drops: \ref fcg_events.
/// - Upload geometry and other bulk data to the GPU: \ref fcg_buffers.
/// - Display image files: load them with the Image library (\ref fcg_images) and hand them to the GPU with
///   <tt>\ref fcg::Image::upload "Image::upload"</tt>; for textures created on the GPU, see \ref fcg_textures.
/// - Render a full-screen effect such as an sRGB conversion: \ref fcg_fullscreen.
/// - Add a camera with orbit, pan and zoom controls: \ref fcg_viewing, \ref fcg_orbit_camera and
///   \ref fcg_camera_2d.
/// - Show Dear ImGui widgets: \ref fcg_gui.
/// - Compile shaders and create pipelines: \ref fcg_devices and \ref fcg_resources.
/// - Transform geometry between model, view and clip space: \ref fcg_render_state.
/// - Structure applet behavior as states and transitions: \ref fcg_utilities.



//////
//
// Module documentation
//

/// \defgroup fcg_applets Applets
/// \ingroup fcg_modules
/// \brief The units of application logic: implement the \c Applet callbacks and run them with \c fcg::run.
///
/// An <tt>\ref fcg::Applet "Applet"</tt> is a C++ class that implements the framework's per-frame callbacks. The
/// framework drives one or more applets in a single application window: it initializes SDL and the GPU, creates the
/// main window and the GUI, and then repeatedly delivers input events, collects GUI and update work, and asks every
/// applet to record its draw calls.
///
/// All applets share the main window, the GPU device and the <tt>\ref fcg::Player "Player"</tt>, which gives access to
/// shared application state. When several applets are run together, each callback is invoked on every applet in the
/// order in which the applets were passed to <tt>\ref fcg::run "run"</tt>.
///
/// \section fcg_applets_usage Getting started
/// Derive from <tt>\ref fcg::Applet "Applet"</tt>, implement the callbacks, and pass the applet to
/// <tt>\ref fcg::run "run"</tt>:
/// \snippet applet_examples.cpp first_applet
/// \snippet applet_examples.cpp run_single
///
/// \section fcg_applets_workflows Common tasks
/// \subsection fcg_applets_workflows_running Running applets together
/// Combine applets by passing several instances to <tt>\ref fcg::run "run"</tt>, e.g.
/// <tt>fcg::run&lt;MyCamera, MyScene&gt;()</tt>. The order matters in two ways:
/// - Events are delivered in order, and every applet sees every event. Check
///   <tt>\ref fcg::EventContext::wasHandled "EventContext::wasHandled"</tt> to skip events that a preceding applet
///   already consumed.
/// - The <tt>\ref fcg::Applet::render "render"</tt> callbacks share one
///   <tt>\ref fcg::RenderState "RenderState"</tt> in the same order. A camera applet placed before a scene applet
///   installs its viewing matrices for the scene to draw with.
///
/// \subsection fcg_applets_workflows_redraw Continuous redraw
/// By default, the player waits for input before it starts the next main-loop iteration. Applets that animate
/// something request continuous iterations with
/// <tt>\ref fcg::Player::pushContinuousRedraw "Player::pushContinuousRedraw"</tt> for as long as they animate, and
/// balance it with <tt>\ref fcg::Player::popContinuousRedraw "Player::popContinuousRedraw"</tt> afterwards.
///
/// \subsection fcg_applets_workflows_input Input and GUI
/// Handle input in <tt>\ref fcg::Applet::onEvent "onEvent"</tt>; see \ref fcg_events. Define Dear ImGui widgets in
/// <tt>\ref fcg::Applet::gui "gui"</tt>; see \ref fcg_gui.
///
/// \subsection fcg_applets_workflows_render Rendering
/// <tt>\ref fcg::Applet::render "render"</tt> receives an active render pass targeting the frame's scene texture.
/// Record the draw calls that should appear in the scene; do not end or re-begin the pass. Create pipelines compatible
/// with <tt>\ref fcg::Player::mainRenderTargetInfo "Player::mainRenderTargetInfo"</tt>, and push uniform data through
/// the command buffer that is passed alongside the pass.
///
/// \section fcg_applets_lifetime Ownership and lifetime
/// <tt>\ref fcg::run "run"</tt> takes ownership of the applets and destroys them in reverse order after the main loop
/// has ended, while the GPU device and the player are still valid. All callbacks run on the thread that called
/// <tt>\ref fcg::run "run"</tt>.
///
/// \section fcg_applets_errors Handling errors
/// Exceptions thrown by applet callbacks propagate out of <tt>\ref fcg::run "run"</tt>. If initializing SDL, the GPU
/// device, the window or the GUI fails, <tt>\ref fcg::run "run"</tt> returns \c EXIT_FAILURE and no applet callback is
/// invoked.
///
/// \see \ref fcg_runtime, \ref fcg_events, \ref fcg_gui, \ref fcg_render_state



//////
//
// Includes
//

// C++ STL
#include <string>
#include <memory>
#include <concepts>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/event.h"
#include "FCG/render_state.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPURenderPass;
struct SDL_GPUCommandBuffer;

// Framework types
namespace fcg {
	class Device;
	class RenderState;
	class Player;
	class Applet;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_applets
/// @{



//////
//
// Classes
//

/// \brief
/// 	Base class for the units of application logic that \c fcg::run drives: derive from it, implement the callbacks
/// 	and pass instances to <tt>\ref fcg::run "run"</tt>.
///
/// All applets of an application share the main window, the GPU device and the <tt>\ref fcg::Player "Player"</tt>.
/// All callbacks run on the thread that called \c fcg::run. When several applets are run together, each callback is
/// invoked on every applet in the order in which the applets were passed to \c fcg::run.
///
/// The player invokes the callbacks as follows:
/// 1. <tt>\ref init "init"</tt> once per applet, after the main window, the GPU device and the GUI have been set up
/// 	and before the first frame.
/// 2. Once per main-loop iteration:
/// 	- <tt>\ref onViewportResize "onViewportResize"</tt> if the size of the main viewport has changed;
/// 	- <tt>\ref onEvent "onEvent"</tt> for each pending input event of the main window;
/// 	- <tt>\ref gui "gui"</tt> followed by <tt>\ref update "update"</tt>, applet by applet;
/// 	- <tt>\ref render "render"</tt>, but only if a frame could be started (for example, not while the window is
/// 	  minimized).
/// 3. After the main loop has ended, the applets are destroyed in reverse order. The device and the player are still
/// 	valid in the destructors.
///
/// By default, the player waits for input before it starts the next main-loop iteration. Applets that animate
/// something can request continuous iterations with
/// <tt>\ref fcg::Player::pushContinuousRedraw "Player::pushContinuousRedraw"</tt>.
class FCG_FRAMEWORK_EXPORT Applet
{
public:

	////
	// Object construction/destruction

	/// Virtual destructor. When run by \c fcg::run, the applet is destroyed after the main loop has ended, while the
	/// device and the player are still alive.
	virtual ~Applet () = default;


	////
	// Methods

	/// \brief
	/// 	The human-readable name of the applet. Used, for example, in log messages and by
	/// 	<tt>\ref fcg::Player::findCamera "Player::findCamera"</tt>.
	///
	/// \return A reference to the name, which must remain valid for the lifetime of the applet.
	[[nodiscard]] virtual auto name () const -> const std::string& = 0;

	/// \brief
	/// 	Called once after the main window, the GPU device and the GUI have been set up, before the first frame. Use
	/// 	it to create pipelines, buffers, textures and other resources.
	///
	/// The main viewport size is already available via <tt>\ref fcg::Player::viewportSize "Player::viewportSize"</tt>.
	///
	/// \param device The GPU device that the applet will use for rendering.
	/// \param player The player running the applet.
	virtual void init (Device &device, Player &player) = 0;

	/// \brief
	/// 	Called at the beginning of a main-loop iteration whenever the size of the main viewport has changed. The main
	/// 	viewport is the scene texture targeted by the render pass passed to <tt>\ref render "render"</tt>.
	///
	/// The call happens before any input events of that iteration are delivered. Query the new dimensions with
	/// <tt>\ref fcg::Player::viewportSize "Player::viewportSize"</tt>.
	///
	/// \param device The GPU device used for rendering.
	/// \param oldViewportSize The dimensions of the main viewport before the change, in pixels.
	/// \param player The player running the applet.
	virtual void onViewportResize (Device &device, const glm::uvec2 &oldViewportSize, Player &player) = 0;

	/// \brief
	/// 	Called once for each input event of the main window, before <tt>\ref gui "gui"</tt> and
	/// 	<tt>\ref update "update"</tt> of the same main-loop iteration. The default implementation does nothing.
	///
	/// Events that the GUI captures are not delivered: keyboard events are withheld while
	/// <tt>\ref fcg::Gui::wantsKeyboard "Gui::wantsKeyboard"</tt> is \c true, and mouse events while
	/// <tt>\ref fcg::Gui::wantsMouse "Gui::wantsMouse"</tt> is \c true.
	///
	/// Every applet receives every delivered event, even if a preceding applet has marked it as handled. Check
	/// <tt>\ref fcg::EventContext::wasHandled "EventContext::wasHandled"</tt> to skip events already consumed, and call
	/// <tt>\ref fcg::EventContext::markHandled "EventContext::markHandled"</tt> after consuming one yourself.
	///
	/// Pressing Escape closes the application, unless an applet marks the \c EventType::KeyDown event as handled. If
	/// the GUI captures the keyboard, the event is not delivered and Escape still closes the application, except while
	/// a GUI text field is being edited.
	///
	/// \param event
	/// 	The event. It is only valid during this call; do not store it or pointers obtained from it, but copy the
	/// 	data you need.
	/// \param context The context shared by all applets for this event.
	/// \param player The player running the applet.
	virtual void onEvent (const Event &event, EventContext &context, Player &player) {}

	/// \brief
	/// 	Called once per main-loop iteration to define the applet's <em>Dear ImGui</em> widgets.
	///
	/// The framework's ImGui context is current during this call, so applets can issue \c ImGui:: calls directly. The
	/// framework renders the resulting draw data on top of the scene; applets must not call \c ImGui::Render
	/// themselves. For each applet, this is called right before its <tt>\ref update "update"</tt>.
	///
	/// \param device The GPU device used for rendering.
	/// \param player The player running the applet.
	virtual void gui (Device &device, Player &player) = 0;

	/// \brief
	/// 	Called once per main-loop iteration, right after this applet's <tt>\ref gui "gui"</tt>, to update the applet
	/// 	state for the next frame.
	///
	/// \param device The GPU device used for rendering.
	/// \param player The player running the applet.
	/// \param dt
	/// 	The time elapsed since the previous main-loop iteration, in seconds. When an input event wakes up the player
	/// 	after it was idle waiting for input, the waiting time is excluded, so \p dt is typically very small (a few
	/// 	microseconds or less).
	virtual void update (Device &device, Player &player, float dt) = 0;

	/// \brief
	/// 	Called once per frame, after all applets had their <tt>\ref gui "gui"</tt> and
	/// 	<tt>\ref update "update"</tt> turn, to record the applet's draw calls.
	///
	/// The render pass targets the frame's scene texture, not the window's swapchain image. Before the first applet
	/// renders, the scene color is cleared to <tt>\ref fcg::Player::clearColor "Player::clearColor"</tt> and the depth
	/// buffer to 1. Shaders write linear colors. Create pipelines compatible with
	/// <tt>\ref fcg::Player::mainRenderTargetInfo "Player::mainRenderTargetInfo"</tt>. After all applets have
	/// rendered, the framework presents the scene to the window and draws the GUI on top.
	///
	/// Not called for main-loop iterations in which no frame could be started, for example while the window is
	/// minimized.
	///
	/// \param device The GPU device used for rendering.
	/// \param renderState
	/// 	The render state of this frame. The same object is passed to every applet in order, so changes made by an
	/// 	applet (for example, a camera setting the view matrices) are visible to the applets after it.
	/// \param renderPass
	/// 	The scene render pass. Record all draw calls that should appear in the scene into this pass. The framework
	/// 	begins the pass before the first applet and ends it after the last one; do not end or re-begin it yourself.
	/// \param commandBuffer
	/// 	The command buffer that the render pass belongs to. Use it for operations that cannot be recorded through the
	/// 	render pass handle, such as pushing uniform data.
	/// \param player The player running the applet.
	virtual void render (
		Device &device, RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, Player &player
	) = 0;
};

/// \brief
/// 	The concept of a type that can be run as an applet, i.e. one that derives from
/// 	<tt>\ref fcg::Applet "Applet"</tt>. Used by the template overloads of <tt>\ref fcg::run "run"</tt>.
///
/// \tparam A The type to check.
template <class A>
concept AppletConcept = std::derived_from<A, Applet>;



/// @}



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_APPLET_H__
