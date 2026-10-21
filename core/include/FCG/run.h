
#ifndef __FCG_RUN_H__
#define __FCG_RUN_H__


//////
//
// Module documentation
//

/// \defgroup fcg_runtime Runtime and player
/// \ingroup fcg_modules
/// \brief Starting the application with \c fcg::run, and the \c Player through which applets interact with it.
///
/// <tt>\ref fcg::run "run"</tt> is the entry point of every framework application: pass it your applets and it
/// initializes SDL, the GPU device, the main window and the GUI, runs the main loop until the application is closed,
/// and cleans everything up. While the loop runs, applets interact with the application through the
/// <tt>\ref fcg::Player "Player"</tt> that every callback receives: they change the window title, request continuous
/// redraws, request shutdown, look up cameras, query the viewport, and customize scene presentation.
///
/// \section fcg_runtime_usage Getting started
/// The simplest way to run applets is the template overload, which default-constructs one applet of each given type:
/// \snippet runtime_examples.cpp run_single
/// For applets that need constructor arguments or custom initial settings, construct them yourself:
/// \snippet runtime_examples.cpp run_settings
///
/// \section fcg_runtime_workflows Common tasks
/// \subsection fcg_runtime_workflows_settings Window settings
/// <tt>\ref fcg::PlayerSettings "PlayerSettings"</tt> holds the main window title and display-scaling preference.
/// The main window scales its client dimensions with the display at startup and when the display scale changes.
/// Set <tt>\ref fcg::PlayerSettings::mainWindowScaleToDisplay "PlayerSettings::mainWindowScaleToDisplay"</tt> to
/// false to keep its dimensions in native screen coordinates. See \ref fcg_windows for sizing and restoration.
/// Applets can change the title later with
/// <tt>\ref fcg::Player::setWindowTitle "Player::setWindowTitle"</tt>.
///
/// \subsection fcg_runtime_workflows_shutdown Shutting down
/// The application shuts down when the user closes the main window or presses Escape, when the OS requests it, or when
/// an applet calls <tt>\ref fcg::Player::requestClose "Player::requestClose"</tt>. During shutdown, the applets are
/// destroyed in reverse order while the GPU device and the player are still valid.
///
/// \subsection fcg_runtime_workflows_player Player services
/// Through the <tt>\ref fcg::Player "Player"</tt>, applets can:
/// - query and change the scene clear color (<tt>\ref fcg::Player::clearColor "clearColor"</tt>);
/// - adjust built-in scene gamma correction and RGB bounds
///   (<tt>\ref fcg::Player::gammaCorrection "gammaCorrection"</tt>);
/// - request continuous redraws while animating
///   (<tt>\ref fcg::Player::pushContinuousRedraw "pushContinuousRedraw"</tt>);
/// - look up a camera applet by name (<tt>\ref fcg::Player::findCamera "findCamera"</tt>);
/// - query the main viewport size and render-target formats
///   (<tt>\ref fcg::Player::viewportSize "viewportSize"</tt>,
///   <tt>\ref fcg::Player::mainRenderTargetInfo "mainRenderTargetInfo"</tt>);
/// - convert GUI sizes and positions authored at 100% display scale into ImGui coordinates
///   (<tt>\ref fcg::Player::toGuiUnits "toGuiUnits"</tt>; see \ref fcg_gui_workflows_scaling);
/// - read back the depth buffer (<tt>\ref fcg::Player::scheduleDepthReadback "scheduleDepthReadback"</tt>), e.g. for
///   focus picking as done by <tt>\ref fcg::CameraFocus "CameraFocus"</tt>;
/// - replace the main window's scene presentation
///   (<tt>\ref fcg::Player::setPresentationRecorder "setPresentationRecorder"</tt>).
///
/// \subsection fcg_runtime_workflows_gamma Adjusting built-in gamma correction
/// Built-in scene presentation converts linear colors to sRGB, applies gamma correction, then clamps RGB. Use
/// <tt>\ref fcg::Player::gammaCorrection "Player::gammaCorrection"</tt> to read or change the application-wide
/// <tt>\ref fcg::GammaUniforms "GammaUniforms"</tt> parameters. The player's gamma defaults to 1 in each of the red,
/// green, and blue channels, preserving standard sRGB conversion, and the final RGB bounds default to [0, 1].
/// Values above 1 brighten a channel's midtones; values below 1 darken them. Alpha is unchanged, and Dear ImGui is
/// drawn afterwards with its own internal color management.
///
/// In an applet's <tt>\ref fcg::Applet::gui "Applet::gui"</tt> callback, use one slider per channel to change gamma
/// and a button to reset all three gamma values to 1 and bounds to [0, 1]:
/// \snippet presentation_examples.cpp builtin_gamma
/// Changes before presentation, including in <tt>\ref fcg::Applet::init "Applet::init"</tt>,
/// <tt>\ref fcg::Applet::gui "Applet::gui"</tt>, <tt>\ref fcg::Applet::update "Applet::update"</tt> or
/// <tt>\ref fcg::Applet::render "Applet::render"</tt>, affect the next presentation. Supply finite positive gamma in
/// each channel and ordered clamp endpoints that are not NaN; infinite endpoints are allowed. See
/// \ref fcg_fullscreen_workflows_gamma for output bounds and sampled-color requirements.
///
/// \subsection fcg_runtime_workflows_presentation Custom scene presentation
/// To replace built-in presentation, install one application-wide
/// <tt>\ref fcg::PresentationRecorder "PresentationRecorder"</tt> through
/// <tt>\ref fcg::Player::setPresentationRecorder "Player::setPresentationRecorder"</tt> whose job it is to record the
/// rendering commands for presenting the scene image. Each rendered frame runs all
/// <tt>\ref fcg::Applet::render "Applet::render"</tt> scene contributions, then the registered presentation recorder,
/// then Dear ImGui rendering. Presentation is skipped when no window image has been acquired.
///
/// This example uses <tt>\ref fcg::FullscreenPass::gammaCorrection "FullscreenPass::gammaCorrection"</tt> to present
/// the scene with an adjustable gamma curve per channel. It samples linear RGB, applies <tt>pow(rgb, 1/gamma)</tt>
/// componentwise, clamps the result and preserves alpha. The default gamma of 2.2 in each channel gives a simple
/// approximation to sRGB encoding.
/// The custom recorder supplies the final display encoding in place of the runtime's standard sRGB conversion.
/// See \ref fcg_fullscreen_workflows_gamma for gamma, clamp-range and sampled-color requirements.
///
/// Include the public headers, including <tt>\#include &lt;FCG/frame.h&gt;</tt> for the recorder and the
/// <tt>\ref fcg::PresentationContext "PresentationContext"</tt> supplied on each invocation:
/// \snippet runtime_examples.cpp presentation_includes
/// Keep a <tt>\ref fcg::GammaUniforms "GammaUniforms"</tt> member in the applet. Default initialization selects gamma
/// 2.2 in each channel and final RGB bounds of [0, 1]:
/// \snippet runtime_examples.cpp presentation_parameters
/// Create the pass on the first <tt>\ref fcg::Applet::recreatePipelines "Applet::recreatePipelines"</tt> call using
/// the window format from
/// <tt>\ref fcg::Player::swapchainFormat "Player::swapchainFormat"</tt> (scene pipelines use
/// <tt>\ref fcg::Player::mainRenderTargetInfo "Player::mainRenderTargetInfo"</tt> for their attachments instead).
/// The recorder owns the pass and reads the applet's current parameters during each invocation, supplying one uniform
/// block with the scene texture and sampler. Retain this pass across scene-format changes, which preserve the window
/// format, and leave later recorder selections in place:
/// \snippet runtime_examples.cpp presentation_install
///
/// While the custom recorder is registered, sliders in <tt>\ref fcg::Applet::gui "Applet::gui"</tt> change each
/// channel's gamma for that frame's presentation. Increasing gamma brightens midtones; decreasing it darkens them.
/// The adjustment affects the scene, with Dear ImGui drawn afterwards. The button calls
/// <tt>\ref fcg::Player::clearPresentationRecorder "Player::clearPresentationRecorder"</tt> to remove the custom
/// recorder and resume built-in sRGB presentation using the player's current gamma correction settings:
/// \snippet runtime_examples.cpp presentation_reset
/// Registration and clearing are main-thread operations. Changes before presentation affect that frame; the latest
/// registration wins. An empty recorder clears the registration. Custom recorders use their own parameters; installing
/// or clearing one preserves <tt>\ref fcg::Player::gammaCorrection "Player::gammaCorrection"</tt> settings. Do not
/// replace or clear a recorder from inside its invocation; either operation throws \c std::logic_error.
///
/// The supplied <tt>\ref fcg::PresentationContext "PresentationContext"</tt> borrows the current scene, sampler,
/// command buffer and window target for this invocation only. Use these fresh resources each time, including after
/// resize; target extents are in pixels. Rebuild bindings locally as above rather than retaining scene or window-image
/// handles. <tt>\ref fcg::FullscreenPass::record "FullscreenPass::record"</tt> begins and ends its own pass;
/// recorder-local binding arrays remain valid for the recording call. End every GPU pass even on error or exception.
/// Leave command-buffer submission or cancellation, frame completion and frame acquisition to the runtime.
///
/// Captured references to applet parameters must remain valid while the recorder is registered. The runtime clears
/// recorders before destroying applets. Return recording errors through the recorder's result; exceptions propagate
/// out of <tt>\ref fcg::run "run"</tt>.
///
/// For other custom effects, create a pass with <tt>\ref fcg::FullscreenPass::create "FullscreenPass::create"</tt> and
/// a fragment shader that supplies the intended final display encoding. Intermediate passes may precede the final
/// window draw in the same recorder, allowing to inject a chain of complex post-processing effects (screen-space ambient
/// occlusion, depth-of-field, HDR tonemapping and bloom, temporal anti-aliasing, etc.) if desired.
///
/// \subsection fcg_runtime_workflows_scene Selecting scene storage
/// Set <tt>\ref fcg::PlayerSettings::sceneRenderTargetFormat "PlayerSettings::sceneRenderTargetFormat"</tt> before
/// running the application, or call <tt>\ref fcg::Player::setSceneFormat "Player::setSceneFormat"</tt> from an applet
/// callback. The default is \c SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB. Requests require single-sample 2D support
/// for combined color-target and sampler usage; integer and depth formats are unsupported. Success accepts the
/// request, and <tt>\ref fcg::Player::mainRenderTargetInfo "Player::mainRenderTargetInfo"</tt> continues to report
/// the effective format until it is applied. Requesting that effective format cancels a pending change.
/// All initialization callbacks finish before the latest request is applied and every applet creates pipelines in
/// <tt>\ref fcg::Applet::recreatePipelines "Applet::recreatePipelines"</tt>. Runtime requests apply before the next
/// iteration's frame acquisition, also while minimized; recreation precedes resize callbacks and input. Requests
/// made during recreation wait until the following iteration. Format changes preserve depth readbacks when the
/// viewport size stays the same. See \ref fcg_applets_workflows_pipelines for a compiled example.
///
/// \section fcg_runtime_lifetime Ownership and lifetime
/// <tt>\ref fcg::run "run"</tt> takes ownership of the applets it is given. Run one application at a time, from the
/// main thread; the call blocks until the application shuts down.
///
/// A previously current ImGui context remains caller-owned and is restored when the call returns or unwinds.
/// Keep that context alive and unused during the call. Leave the framework's context current until its GUI is
/// destroyed, including throughout applet destruction; see <tt>\ref fcg::Gui "Gui"</tt> for its lifetime contract.
///
/// Presentation recorders own their captures, including move-only passes. Captures are destroyed on replacement,
/// clearing or shutdown. On normal shutdown and exception unwinding, the runtime clears the recorder before destroying
/// applets, while the device is still alive. References captured from applet state must remain valid while registered.
///
/// \section fcg_runtime_errors Handling errors
/// <tt>\ref fcg::run "run"</tt> returns \c EXIT_FAILURE if initialization (e.g. of the SDL library, essential rendering
/// facilities like the GPU device, or ImGui) fails, or if the main loop cannot continue because an indefinite wait for
/// events reported a failure. The reason for such a critical exit is written to the SDL log. When initialization fails,
/// no applet callback is ever invoked. With an empty applet list, <tt>\ref fcg::run "run"</tt> returns \c EXIT_SUCCESS
/// after logging a warning.
/// GPU failures while acquiring or presenting a frame throw \c std::runtime_error, and exceptions from applet callbacks
/// or other custom injected code (like \ref fcg::PresentationRecorder "presentation recorders") propagate out of the
/// call. A failed frame's displayed contents are unspecified.
///
/// \see \ref fcg_applets, \ref fcg_events, \ref fcg_windows



//////
//
// Includes
//

// C++ STL
#include <memory>
#include <optional>
#include <string>
#include <vector>

// SDL3 library
#include <SDL3/SDL_gpu.h>

// Local includes
#include "FCG/export.h"
#include "FCG/applet.h"



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
// Structs and enums
//

/// Initial settings of the applet player, passed to <tt>\ref fcg::run "run"</tt>.
struct PlayerSettings
{
	/// The initial title of the main window. If not set, the title is <tt>"FCG Player"</tt>.
	std::optional<std::string> mainWindowTitle = std::nullopt;

	/// Scale the main window's initial and normal client dimensions with the display, as described by
	/// \c WindowSettings::scaleToDisplay.
	bool mainWindowScaleToDisplay = true;

	/// Scene color storage. Requires single-sample 2D color-target and sampler support; integer and depth formats
	/// are unsupported. Invalid configuration is logged and makes \c run return \c EXIT_FAILURE before callbacks.
	SDL_GPUTextureFormat sceneRenderTargetFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
};



//////
//
// Functions
//

/// \brief
/// 	Runs the given applets in the framework's applet player and returns when the player has shut down.
///
/// The function initializes SDL, creates the GPU device, the main window and the GUI, and calls
/// <tt>\ref fcg::Applet::init "Applet::init"</tt> on each applet in order. After applying scene-format requests, it
/// calls <tt>\ref fcg::Applet::recreatePipelines "Applet::recreatePipelines"</tt> on each applet in order, then runs
/// the main loop, invoking the applet callbacks as described for <tt>\ref fcg::Applet "Applet"</tt>, and blocks until the application is closed
/// (by closing the main window, pressing Escape, an OS quit request or
/// <tt>\ref fcg::Player::requestClose "Player::requestClose"</tt>). Finally, it destroys the applets in reverse order
/// while the GPU device and the player are still alive, releases all framework resources and shuts down SDL.
///
/// Exceptions thrown by applet callbacks propagate out of this function. The player and the GPU device remain valid
/// while the applets are destroyed during the unwinding. If finishing a frame also fails during this unwinding,
/// the submission failure is logged and the original exception propagates unchanged.
///
/// A previously current ImGui context is restored on return or exception unwinding.
///
/// \pre
/// 	Call from the main thread. Only one call may be active at a time. No element of \p applets is \c nullptr.
/// 	Any previously current ImGui context must remain alive and unused during the call. While the framework GUI
/// 	exists, keep its context current, including during applet destruction, and leave context destruction and
/// 	backend initialization and shutdown to the framework.
///
/// \note
/// 	When the GUI requires periodic redraws, the runtime uses timed event waits. A wait that produces no event is
/// 	treated as a timeout and the loop continues. SDL does not provide a reliable distinction between timeout and
/// 	failure for these waits, so their failures are not guaranteed to be logged or reflected in the return value.
///
/// \param applets The applets to run, in callback order. The function takes ownership of them.
/// \param settings The initial player settings.
///
/// \return
/// 	\c EXIT_SUCCESS after a normal shutdown, and also when \p applets is empty (in which case a warning is logged and
/// 	nothing else happens). \c EXIT_FAILURE if SDL, the GPU device, the main window, the default presentation pass
/// 	or the GUI could not be initialized, or if an indefinite event wait reports failure. The reason is logged to the
/// 	SDL log. Initialization failures invoke no applet callbacks; event-wait failures can occur after callbacks have run.
///
/// \throws std::runtime_error
/// 	If acquiring a frame from the main window or presenting it fails at the GPU level.
FCG_FRAMEWORK_EXPORT int run (
	std::vector<std::unique_ptr<Applet>> applets, PlayerSettings &&settings = PlayerSettings()
);

/// \brief
/// 	Runs the given applets with default player settings.
///
/// \overload
///
/// \tparam A The applet types.
///
/// \param applets The applets to run, in callback order. The function takes ownership of them.
///
/// \return The exit code of the player, as described for the main overload.
///
/// \throws std::runtime_error
/// 	In the same situations as the main overload.
template<AppletConcept... A>
inline int run (std::unique_ptr<A>&&... applets) {
	// Move all applet pointers into a vector and forward to the runtime-polymorphic fcg::run.
	std::vector<std::unique_ptr<Applet>> appletList;
	appletList.reserve(sizeof...(applets));
	(appletList.emplace_back(std::move(applets)), ...);
	return run(std::move(appletList));
}

/// \brief
/// 	Runs the given applets with the given initial player settings.
///
/// \overload
///
/// \tparam A The applet types.
///
/// \param settings The initial player settings.
/// \param applets The applets to run, in callback order. The function takes ownership of them.
///
/// \return The exit code of the player, as described for the main overload.
///
/// \throws std::runtime_error
/// 	In the same situations as the main overload.
template<AppletConcept... A>
inline int run (PlayerSettings &&settings, std::unique_ptr<A>&&... applets) {
	// Move all applet pointers into a vector and forward to the runtime-polymorphic fcg::run.
	std::vector<std::unique_ptr<Applet>> appletList;
	appletList.reserve(sizeof...(applets));
	(appletList.emplace_back(std::move(applets)), ...);
	return run(std::move(appletList), std::move(settings));
}

/// \brief
/// 	Default-constructs one applet of each of the given types and runs them, e.g.
/// 	<tt>fcg::run<MyCamera, MyScene>()</tt>.
///
/// \overload
///
/// \tparam A The applet types, in callback order. Each must be default-constructible.
///
/// \param settings The initial player settings.
///
/// \return The exit code of the player, as described for the main overload.
///
/// \throws std::runtime_error
/// 	In the same situations as the main overload.
template<AppletConcept... A>
inline int run (PlayerSettings &&settings = PlayerSettings()) {
	// Forward to run with pre-constructed applets.
	return run(std::move(settings), std::make_unique<A>()...);
}



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif  // ifndef __FCG_RUN_H__
