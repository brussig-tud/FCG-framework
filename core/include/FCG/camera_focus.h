
#ifndef __FCG_CAMERA_FOCUS_H__
#define __FCG_CAMERA_FOCUS_H__


//////
//
// Module documentation
//

/// \defgroup fcg_camera_focus Camera focus picking
/// \ingroup fcg_modules
/// \brief Focusing the camera on the object under the mouse cursor.
///
/// <tt>\ref fcg::CameraFocus "CameraFocus"</tt> adds depth picking to any <tt>\ref fcg::Camera "Camera"</tt>: it
/// finds the world-space point under a mouse click by reading back the rendered depth buffer, then smoothly moves the
/// camera's focal point there over half a second. Both ready-made camera applets use it for their double-click focus
/// navigation; you can also drive it yourself for custom triggers. In planar mode, only the XY coordinates change and
/// the focus depth is preserved, for cameras that cannot move along their view direction, such as
/// <tt>\ref fcg::applet::Camera2D "Camera2D"</tt>.
///
/// \section fcg_camera_focus_usage Getting started
/// The controller is not an applet; it borrows the camera and is wired into the owning applet's callbacks:
/// \snippet camera_focus_examples.cpp focus navigation
///
/// \section fcg_camera_focus_workflows Common tasks
/// \subsection fcg_camera_focus_workflows_wiring Wiring the controller into an applet
/// Call the four methods from the applet callbacks, every time they run:
/// - <tt>\ref fcg::CameraFocus::rendered "rendered"</tt> in <tt>\ref fcg::Applet::render "render"</tt>, with the view
///   and projection matrices and the viewport used for the current frame;
/// - <tt>\ref fcg::CameraFocus::pick "pick"</tt> in <tt>\ref fcg::Applet::onEvent "onEvent"</tt>, with the
///   mouse-button event that should trigger focusing;
/// - <tt>\ref fcg::CameraFocus::update "update"</tt> in <tt>\ref fcg::Applet::update "update"</tt>, which applies
///   finished picks and advances the animation;
/// - <tt>\ref fcg::CameraFocus::resized "resized"</tt> in <tt>\ref fcg::Applet::onViewportResize
///   "onViewportResize"</tt>.
///
/// While an animation runs, the controller requests continuous redraws and balances them itself.
///
/// \subsection fcg_camera_focus_workflows_triggers Custom triggers
/// <tt>\ref fcg::CameraFocus::pick "pick"</tt> accepts any mouse-button event: the triggering button, modifier keys
/// and click count are the caller's choice. Mouse positions are given in window coordinates and are scaled to drawable
/// pixels automatically. A depth of 1 (the background) is ignored.
///
/// \section fcg_camera_focus_lifetime Ownership and lifetime
/// The controller borrows its camera, which must outlive the controller. The <tt>\ref fcg::Player "Player"</tt>
/// passed to <tt>\ref fcg::CameraFocus::pick "pick"</tt> and <tt>\ref fcg::CameraFocus::update "update"</tt> must
/// stay alive while a pick or animation is in progress; use the same player for both.
///
/// \section fcg_camera_focus_errors Handling errors
/// <tt>\ref fcg::CameraFocus::pick "pick"</tt> returns \c false when nothing has been rendered yet, the viewport has
/// changed since the last frame, the event is not a mouse-button event, or the click position is outside the window.
/// Readback failures and exceptions from the camera setters propagate from <tt>\ref fcg::CameraFocus::update
/// "update"</tt>; an animation whose setter fails is cancelled. Supply finite, invertible matrices, and use planar
/// mode for cameras that cannot move along their view direction.
///
/// \see \ref fcg_viewing, \ref fcg_events, \ref fcg_orbit_camera, \ref fcg_camera_2d



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

// Framework types
namespace fcg {
	class Camera;
	class Player;
	class Event;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_camera_focus
/// @{



//////
//
// Classes
//

/// \brief Moves the focal point of a <tt>\ref fcg::Camera "Camera"</tt> smoothly to a scene point picked by mouse.
///
/// A camera applet owns one controller and drives it from its applet callbacks:
/// - in \c Applet::render, call \c rendered every frame with the matrices used for that frame;
/// - in \c Applet::onEvent, call \c pick for the mouse-button event that should pick a new focus;
/// - in \c Applet::update, call \c update once per frame, always with the same \c Player;
/// - in \c Applet::onViewportResize, call \c resized;
/// - before any manual change to the camera, call \c cancel.
///
/// A successful pick reads the depth under the cursor and moves the focal point there over half a second, keeping
/// the camera's orientation and focus distance. In planar mode, only the X and Y coordinates of the focal point
/// change.
///
/// \pre
/// 	The camera outlives the controller. The \c Player passed to \c pick and \c update outlives the controller.
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

	/// \brief Create a focus controller for the given camera.
	///
	/// \param camera
	/// 	The camera whose focal point is moved. It is borrowed and must outlive the controller.
	/// \param planar
	/// 	If \c true, only the X and Y coordinates of the focal point change and its Z coordinate is preserved
	/// 	exactly, as needed by cameras restricted to movement in the XY plane. If \c false (the default), the
	/// 	focal point moves freely in 3D.
	explicit CameraFocus(Camera &camera, bool planar = false);

	/// Not copyable.
	CameraFocus(const CameraFocus&) = delete;

	/// Not movable.
	CameraFocus(CameraFocus&&) = delete;

	/// \brief Destroy the controller, stopping any focus animation.
	///
	/// \pre The \c Player last passed to \c pick or \c update is still alive.
	~CameraFocus();

	/// Not copy-assignable.
	auto operator= (const CameraFocus&) -> CameraFocus& = delete;

	/// Not move-assignable.
	auto operator= (CameraFocus&&) -> CameraFocus& = delete;


	////
	// Methods

	/// \brief Record the matrices of the frame being rendered, which \c pick uses to locate the clicked point.
	///
	/// Call every frame from \c Applet::render with the same matrices that the camera installs in the
	/// \c RenderState. Calls with an empty viewport are ignored.
	///
	/// \pre The matrices are finite and <tt>projection * view</tt> is invertible.
	///
	/// \param view
	/// 	The modelview matrix used for rendering.
	/// \param projection
	/// 	The projection matrix used for rendering.
	/// \param viewport
	/// 	The drawable viewport size in pixels. Must equal \c Player::viewportSize for \c pick to succeed.
	void rendered (const glm::mat4 &view, const glm::mat4 &projection, const glm::uvec2 &viewport);

	/// \brief Start picking a new focal point at the cursor position of a mouse-button event.
	///
	/// Call from \c Applet::onEvent. The caller decides which button, click count and modifiers trigger a pick. On
	/// success, any running focus animation stops, and the depth of the scene under the cursor is requested from the
	/// player. Call \c update later in the same frame to start the animation promptly, as happens naturally when
	/// \c pick is called from \c Applet::onEvent and \c update from \c Applet::update. A delayed \c update can still
	/// read the requested depth until a newer readback replaces it or the viewport changes.
	///
	/// \param event
	/// 	The mouse-button event whose position, in window coordinates, should be picked.
	/// \param player
	/// 	The player providing the depth of the rendered scene.
	///
	/// \return
	/// 	\c true if picking started. \c false, with no change, if \c event is not a mouse-button event, if
	/// 	\c rendered has not yet been called with a non-empty viewport (since construction or the last \c resized),
	/// 	if that viewport differs from the current \c Player::viewportSize, or if the event position is not finite
	/// 	or lies outside the window.
	///
	/// \throws std::runtime_error
	/// 	If the depth of the scene cannot be requested, e.g. when no frame is currently available; see
	/// 	\c Player::scheduleDepthReadback.
	[[nodiscard]] auto pick (const Event &event, Player &player) -> bool;

	/// \brief Advance the focus animation and start it after a successful \c pick.
	///
	/// Call exactly once per frame from \c Applet::update. After a pick, this waits if necessary until the GPU has
	/// provided the depth under the cursor. If that pixel shows background (depth 1), nothing happens. Otherwise,
	/// the focal point moves from its current position to the picked point over 0.5 seconds with smoothstep easing,
	/// by calling \c Camera::translateToFocalPoint each frame. While the animation runs, the controller requests
	/// continuous redraw from the player (see \c Player::pushContinuousRedraw).
	///
	/// \pre Always the same \c Player is passed, also to \c pick.
	///
	/// \param player
	/// 	The player that renders the camera's view.
	/// \param dt
	/// 	The time since the last frame in seconds. Negative or nonfinite values count as zero.
	///
	/// \throws std::runtime_error
	/// 	If the depth requested by \c pick cannot be read, including when a newer readback has replaced it;
	/// 	see \c Player::getDepthReadbackResult.
	/// \throws std::logic_error
	/// 	If the player has discarded the requested depth, e.g. after a viewport change without a call to
	/// 	\c resized.
	/// \throws std::invalid_argument
	/// 	If the camera rejects a focus position. The animation is stopped before the exception propagates.
	void update (Player &player, float dt);

	/// \brief Stop the focus animation and discard a pick whose result has not been applied yet.
	///
	/// Call before changing the camera manually. Keep calling \c update every frame afterwards.
	void cancel ();

	/// \brief Adapt to a change of the viewport size.
	///
	/// Call from \c Applet::onViewportResize. Stops the focus animation like \c cancel, discards any pick in
	/// progress, and makes \c pick return \c false until \c rendered has been called again.
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



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_CAMERA_FOCUS_H__
