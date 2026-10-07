
#ifndef __FCG_CAMERA_FOCUS_H__
#define __FCG_CAMERA_FOCUS_H__


//////
//
// Includes
//

// C++ STL
#include <cstdint>
#include <optional>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"



//////
//
// Forward declarations
//

namespace fcg {
	class Camera;
	class Player;
	class Event;
}



//////
//
// Namespaces open
//

namespace fcg {

/// \defgroup fcg_camera_focus Camera focus picking
/// \ingroup fcg_components
///
/// \brief Reusable depth picking and smooth focal-point transitions for cameras.
///
/// <tt>\ref fcg::CameraFocus "CameraFocus"</tt> operates on any <tt>\ref fcg::Camera "Camera"</tt> without
/// inheriting from <tt>\ref fcg::Applet "Applet"</tt>. It unprojects picked depth using the last rendered
/// matrices and drawable viewport, then translates focus over half a second using smoothstep interpolation.
/// Optional planar mode interpolates only XY and preserves focus Z exactly.
/// Both <tt>\ref fcg::applet::OrbitCamera "OrbitCamera"</tt> and
/// <tt>\ref fcg::applet::Camera2D "Camera2D"</tt> use this controller for double-click focus navigation.
///
/// \section fcg_camera_focus_workflows Common workflows
/// After rendering, call <tt>\ref fcg::CameraFocus::rendered "CameraFocus::rendered"</tt> with the matrices
/// and drawable viewport used for that frame. Pass a mouse-button event to
/// <tt>\ref fcg::CameraFocus::pick "CameraFocus::pick"</tt> to schedule a depth readback; callers choose
/// the triggering button, modifiers, and click count. Logical window coordinates are scaled to drawable pixels.
/// Call <tt>\ref fcg::CameraFocus::update "CameraFocus::update"</tt> each frame to collect readbacks and
/// advance the animation. Background depth one is ignored. Call
/// <tt>\ref fcg::CameraFocus::cancel "CameraFocus::cancel"</tt> before manually changing the camera.
///
/// \section fcg_camera_focus_lifetime Ownership and lifetime
/// The controller borrows its camera and players. The camera must outlive the controller; a player must outlive
/// its pending readback and active animation, including their cleanup in the controller's destructor.
/// Use the same player for picking and updating while either is outstanding. Controllers cannot be copied or moved.
/// Cancelled readbacks are still collected: continue updating before the next frame overwrites the result.
/// After the player replaces its readback storage on resize, call
/// <tt>\ref fcg::CameraFocus::resized "CameraFocus::resized"</tt> to discard invalidated tokens and snapshots.
/// The normal <tt>\ref fcg::Applet::onViewportResize "Applet::onViewportResize"</tt> callback runs at this point.
/// Continuous-redraw requests are balanced on completion, cancellation, resize, and destruction.
///
/// \section fcg_camera_focus_errors Errors
/// Picking returns false for missing snapshots, empty or mismatched viewports, non-button events, and invalid
/// click coordinates. Depth readback failures and camera setter exceptions propagate; an animation whose setter
/// fails is cancelled. The destructor logs readback failures instead of throwing. Supplied matrices must be
/// finite and invertible, and the camera must support the requested focus translations. Use planar mode for
/// cameras constrained to fixed-depth XY movement.
///
/// \section fcg_camera_focus_examples Examples
/// \snippet camera_focus_examples.cpp focus navigation
///
/// \see \ref fcg_viewing, \ref fcg_runtime, \ref fcg_events, \ref fcg_orbit_camera, \ref fcg_camera_2d
/// \addtogroup fcg_camera_focus
/// @{



//////
//
// Classes
//

/// \brief Depth picking and smooth focus translation through a borrowed \c Camera.
///
/// \pre The camera and players used for pending readbacks or active animations outlive their use by this controller.
class FCG_FRAMEWORK_EXPORT CameraFocus
{
	////
	// Types

	/// Matrices and viewport geometry from the last rendered frame.
	struct Snapshot
	{
		/// The inverse combined projection and modelview matrix used for unprojection.
		glm::mat4 inverse;

		/// The drawable viewport dimensions in pixels.
		glm::uvec2 viewport;
	};

	/// One scheduled depth readback and the focus-picking data associated with it.
	struct Pending
	{
		/// The borrowed player that owns the readback; it must outlive this request.
		Player *player;

		/// The token returned by <tt>Player::scheduleDepthReadback</tt>.
		uint64_t token;

		/// The matrix and viewport snapshot associated with the downloaded depth.
		Snapshot snapshot;

		/// The drawable pixel whose depth is to be sampled.
		glm::uvec2 pixel;

		/// Whether to apply the pick; cancelled requests must still be collected.
		bool apply;
	};

	/// A smooth transition between two world-space focal points.
	struct Animation
	{
		/// The focal point when the animation started.
		glm::vec3 start;

		/// The target focal point resolved from the depth readback.
		glm::vec3 target;

		/// Accumulated animation time in seconds.
		float elapsed;
	};


public:

	////
	// Object construction/destruction

	/// \brief Construct a focus controller, optionally constraining animation to XY.
	///
	/// \param camera The borrowed camera whose focus will be translated.
	/// \param planar Whether to preserve focus Z exactly; defaults to unrestricted 3D translation.
	explicit CameraFocus(Camera &camera, bool planar = false);

	/// Controllers cannot be copied because they own readback collection and redraw requests.
	CameraFocus(const CameraFocus&) = delete;

	/// Controllers cannot be moved because readback and redraw ownership stays with this instance.
	CameraFocus(CameraFocus&&) = delete;

	/// The destructor. Collects any pending readbacks and releases the redraw request for an active animation.
	~CameraFocus();

	/// Controllers cannot be copy-assigned because they own readback collection and redraw requests.
	auto operator= (const CameraFocus&) -> CameraFocus& = delete;

	/// Controllers cannot be move-assigned because readback and redraw ownership stays with this instance.
	auto operator= (CameraFocus&&) -> CameraFocus& = delete;


	////
	// Methods

	/// \brief Retain the matrix and drawable viewport snapshot from a rendered frame.
	///
	/// Empty viewports are ignored. Later camera changes do not alter this snapshot.
	///
	/// \pre The matrices are finite and their product is invertible.
	void rendered (const glm::mat4 &view, const glm::mat4 &projection, const glm::uvec2 &viewport);

	/// \brief Convert a window-space mouse-button event to drawable pixels and schedule a depth readback.
	///
	/// A successful pick cancels the previous transition and collects any previous readback before scheduling
	/// another. Button, click count, and modifier filtering are the caller's responsibility.
	///
	/// \pre Use the same player while a readback or animation is outstanding.
	///
	/// \return Whether a readback was scheduled for the click.
	[[nodiscard]] auto pick (const Event &event, Player &player) -> bool;

	/// \brief Collect pending readbacks and advance active focus animations by the given time in seconds.
	///
	/// Nonfinite or negative time steps contribute zero animation time. Cancelled picks are collected without
	/// applying their results. Setter failures cancel the active animation and propagate to the caller.
	///
	/// \pre Use the player that owns the outstanding readback or animation; collect before its result is overwritten.
	void update (Player &player, float dt);

	/// Cancel animation and pending focus application, preserving readbacks that still need collection.
	void cancel ();

	/// \brief Discard rendered snapshots and tokens invalidated by the player's viewport resize.
	///
	/// \pre The player has already replaced its readback storage and invalidated the old tokens.
	void resized ();


private:

	////
	// Methods

	/// Release the continuous-redraw request, if one is currently held.
	void releaseRedraw ();


	////
	// Fields

	/// The borrowed camera whose focus is being controlled.
	Camera &camera;

	/// Whether focus animation must preserve Z exactly and interpolate only XY.
	bool planar;

	/// Whether the controller is currently applying an animation step through the camera's setters.
	bool applying = false;

	/// The player owning the current animation's redraw request, or \c nullptr when none is held.
	Player *redrawPlayer = nullptr;

	/// The last rendered matrix and viewport snapshot, absent before rendering or after a resize.
	std::optional<Snapshot> lastRendered;

	/// The readback awaiting collection, whether or not its result should still be applied.
	std::optional<Pending> pending;

	/// The focus animation currently in progress, if any.
	std::optional<Animation> animation;
};



/// @}



//////
//
// Namespaces close
//

} // namespace fcg


#endif // ifndef __FCG_CAMERA_FOCUS_H__
