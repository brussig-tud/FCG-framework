
#ifndef __FCG_RANDOM_H__
#define __FCG_RANDOM_H__


//////
//
// Includes
//

// C++ STL
#include <memory>
#include <initializer_list>

// Local includes
#include "FCG/export.h"
#include "FCG/applet.h"



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

/// Configuration options for the applet player, passed to \ref fcg::run to set initial properties. They can be
/// subsequently changed by applets via the \ref fcg::Player interface at runtime.
struct PlayerSettings {
	/// The initial main window title.
	std::optional<std::string> mainWindowTitle = std::nullopt;
};



//////
//
// Functions
//

/// Runs the provided applet instances within the framework's runtime environment.
///
/// This function initializes the framework with the specified \ref fcg::Applet instances
/// and configures the player settings. The applets will be managed and executed according
/// to the lifecycle defined by the framework, enabling their rendering and updating logic
/// to be performed each frame.
///
/// \param applets A list of unique pointers to instances of \ref fcg::Applet. These applets
///                define the application-specific behavior and will be executed by the framework.
/// \param settings Optional settings for the applet player, defining initial properties such as the main window title.
///
/// \return An integer status code returned by the framework runtime. The exact code indicating normal shutdown after
/// "successful" execution is platform-dependent, but will always be equal to \c EXIT_SUCCESS as defined in the platform
/// C standard library.
FCG_FRAMEWORK_EXPORT int run (
	std::initializer_list<std::unique_ptr<Applet>> applets,
	PlayerSettings &&settings = PlayerSettings()
);

/// Run with the given \ref fcg::Applet instances, using default settings for the applet player. This is a convenience
/// wrapper around \ref fcg::run(std::initializer_list<std::unique_ptr<fcg::Applet>>, fcg::PlayerSettings&&).
///
/// \return See \ref fcg::run(std::initializer_list<std::unique_ptr<fcg::Applet>>, fcg::PlayerSettings&&).
template<AppletConcept... A>
inline int run (std::unique_ptr<A>&&... applets) {
	// Forward to runtime-polymorphic fcg::run.
	return run({std::move(applets)...});
}

/// Run with the given \ref fcg::Applet instances and custom initial settings for the applet player. This is a
/// convenience wrapper around
/// \ref fcg::run(std::initializer_list<std::unique_ptr<fcg::Applet>>, fcg::PlayerSettings&&).
///
/// \return See \ref fcg::run(std::initializer_list<std::unique_ptr<fcg::Applet>>, fcg::PlayerSettings&&).
template<AppletConcept... A>
inline int run (PlayerSettings &&settings, std::unique_ptr<A>&&... applets) {
	// Forward to runtime-polymorphic fcg::run.
	return run({std::move(applets)...}, std::move(settings));
}

/// Run with default-constructed applets and optional initial settings for the applet player. This is a convenience
/// wrapper around \ref fcg::run(std::initializer_list<std::unique_ptr<fcg::Applet>>, fcg::PlayerSettings&&).
///
/// \param settings See \ref fcg::run(std::initializer_list<std::unique_ptr<fcg::Applet>>, fcg::PlayerSettings&&).
///
/// \return See \ref fcg::run(std::initializer_list<std::unique_ptr<fcg::Applet>>, fcg::PlayerSettings&&).
template<AppletConcept... A>
inline int run (PlayerSettings &&settings = PlayerSettings()) {
	// Forward to run with pre-constructed applets.
	return run(std::move(settings), A::create()...);
}



//////
//
// Namespaces close
//

// namespace FCG
}


#endif  // ifndef __FCG_RANDOM_H__
