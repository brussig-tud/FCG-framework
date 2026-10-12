
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
/// redraws, request shutdown, look up cameras, and query the viewport.
///
/// \section fcg_runtime_usage Getting started
/// The simplest way to run applets is the template overload, which default-constructs one applet of each given type:
/// \snippet runtime_examples.cpp run_single
/// For applets that need constructor arguments or custom initial settings, construct them yourself:
/// \snippet runtime_examples.cpp run_settings
///
/// \section fcg_runtime_workflows Common tasks
/// \subsection fcg_runtime_workflows_settings Window settings
/// <tt>\ref fcg::PlayerSettings "PlayerSettings"</tt> holds the initial settings, the main window title for now.
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
/// - request continuous redraws while animating
///   (<tt>\ref fcg::Player::pushContinuousRedraw "pushContinuousRedraw"</tt>);
/// - look up a camera applet by name (<tt>\ref fcg::Player::findCamera "findCamera"</tt>);
/// - query the main viewport size and render-target formats
///   (<tt>\ref fcg::Player::viewportSize "viewportSize"</tt>,
///   <tt>\ref fcg::Player::mainRenderTargetInfo "mainRenderTargetInfo"</tt>);
/// - read back the depth buffer (<tt>\ref fcg::Player::scheduleDepthReadback "scheduleDepthReadback"</tt>), e.g. for
///   focus picking as done by <tt>\ref fcg::CameraFocus "CameraFocus"</tt>.
///
/// \section fcg_runtime_lifetime Ownership and lifetime
/// <tt>\ref fcg::run "run"</tt> takes ownership of the applets it is given. Run one application at a time, from the
/// main thread; the call blocks until the application shuts down.
///
/// \section fcg_runtime_errors Handling errors
/// <tt>\ref fcg::run "run"</tt> returns \c EXIT_FAILURE if SDL, the GPU device, the window or the GUI could not be
/// initialized, or if waiting for events failed; the reason is written to the SDL log and no applet callback is
/// invoked. With an empty applet list it returns \c EXIT_SUCCESS after logging a warning. GPU failures while acquiring
/// or presenting a frame throw \c std::runtime_error, and exceptions from applet callbacks propagate out of the call.
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

/// Initial settings of the applet player, passed to <tt>\ref fcg::run "run"</tt>. Applets can change the
/// corresponding properties at runtime through the <tt>\ref fcg::Player "Player"</tt>.
struct PlayerSettings
{
	/// The initial title of the main window. If not set, the title is <tt>"FCG Player"</tt>.
	std::optional<std::string> mainWindowTitle = std::nullopt;
};



//////
//
// Functions
//

/// \brief
/// 	Runs the given applets in the framework's applet player and returns when the player has shut down.
///
/// The function initializes SDL, creates the GPU device, the main window and the GUI, and calls
/// <tt>\ref fcg::Applet::init "Applet::init"</tt> on each applet in order. It then runs the main loop, invoking the
/// applet callbacks as described for <tt>\ref fcg::Applet "Applet"</tt>, and blocks until the application is closed
/// (by closing the main window, pressing Escape, an OS quit request or
/// <tt>\ref fcg::Player::requestClose "Player::requestClose"</tt>). Finally, it destroys the applets in reverse order
/// while the GPU device and the player are still alive, releases all framework resources and shuts down SDL.
///
/// Exceptions thrown by applet callbacks propagate out of this function. The player and the GPU device remain valid
/// while the applets are destroyed during the unwinding.
///
/// \pre
/// 	Call from the main thread. Only one call may be active at a time. No element of \p applets is \c nullptr.
///
/// \param applets The applets to run, in callback order. The function takes ownership of them.
/// \param settings The initial player settings.
///
/// \return
/// 	\c EXIT_SUCCESS after a normal shutdown, and also when \p applets is empty (in which case a warning is logged and
/// 	nothing else happens). \c EXIT_FAILURE if SDL, the GPU device, the main window or the GUI could not be
/// 	initialized, or if waiting for events failed. The reason is logged to the SDL log. If initialization fails, no
/// 	applet callbacks are invoked.
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



/// @}



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_RUN_H__
