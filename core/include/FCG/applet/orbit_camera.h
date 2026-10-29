
#ifndef __FCG_ORBIT_CAMERA_H__
#define __FCG_ORBIT_CAMERA_H__


//////
//
// Module documentation
//

/// \defgroup fcg_orbit_camera Orbit camera
/// \ingroup fcg_modules
/// \brief A perspective camera applet with orbit, pan, roll, dolly, and picked focus.
///
/// <tt>\ref fcg::applet::OrbitCamera "OrbitCamera"</tt> is a ready-to-use camera: add it to
/// <tt>\ref fcg::run "run"</tt> alongside your scene applets and select it to navigate with the mouse.
/// While active, it supplies the scene's viewing matrices. It accepts only perspective fields of view. All parameters can also be set
/// programmatically through the <tt>\ref fcg::Camera "Camera"</tt> interface, already before the applet is
/// initialized.
///
/// \section fcg_orbit_camera_usage Getting started
/// Create the applet, optionally configure it, and combine it with scene applets:
/// \snippet camera_examples.cpp orbit camera
///
/// \section fcg_orbit_camera_workflows Common tasks
/// \subsection fcg_orbit_camera_workflows_controls Mouse controls
/// - Left-drag orbits around the focal point.
/// - Shift+left-drag rolls the camera.
/// - Right-drag pans.
/// - The mouse wheel moves the eye toward or away from the focal point, changing the focus distance while keeping
///   the focal point and field of view fixed.
/// - Middle-drag moves the eye along the view direction while keeping the focus distance, so the focal point moves
///   with the camera.
///
/// Double-click a rendered object to smoothly translate the focus there over half a second; see \ref
/// fcg_camera_focus.
///
/// \subsection fcg_orbit_camera_workflows_programmatic Programmatic navigation
/// <tt>\ref fcg::applet::OrbitCamera::setFocalPoint "setFocalPoint"</tt> retargets the focus from the existing eye,
/// <tt>\ref fcg::applet::OrbitCamera::resetRotation "resetRotation"</tt> restores direction -Z and up +Y around the
/// current focus, and <tt>\ref fcg::applet::OrbitCamera::resetFocus "resetFocus"</tt> retargets the world origin from
/// the existing eye. Manual mutations cancel a running picked-focus animation.
/// Use <tt>\ref fcg::Camera::setOrientation "Camera::setOrientation"</tt> to replace direction and up together,
/// or <tt>\ref fcg::Camera::setDir "Camera::setDir"</tt> and <tt>\ref fcg::Camera::setUp "Camera::setUp"</tt>
/// to change one axis. These setters keep the axes normalized and perpendicular.
///
/// \section fcg_orbit_camera_lifetime Ownership and lifetime
/// Select the camera to supply viewing matrices for scene applets. The <tt>\ref fcg::Player "Player"</tt> must outlive the
/// applet; the normal run loop guarantees this.
///
/// \section fcg_orbit_camera_errors Handling errors
/// Nonfinite inputs, zero or parallel axes, orthographic alternatives, and a focus closer than 0.001 world units to
/// the eye throw \c std::invalid_argument without changing the camera. Focus and clipping distances clamp to 0.001,
/// and perspective angles clamp to [1, 179] degrees.
/// Accepted extreme focus distances or world coordinates can still destabilize mouse navigation through loss of
/// single-precision accuracy. Keep scene coordinates and camera distances on a comparable, moderate scale.
///
/// \see \ref fcg_viewing, \ref fcg_camera_focus, \ref fcg_camera_2d, \ref fcg_applets



//////
//
// Includes
//

// C++ STL
#include <chrono>
#include <memory>
#include <optional>

// Local includes
#include "FCG/applet.h"
#include "FCG/viewing.h"



//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class CameraFocus;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// The library's namespace for ready-to-use applets.
namespace applet {

/// \addtogroup fcg_orbit_camera
/// @{



//////
//
// Classes
//

/// \brief Applet providing an interactive perspective camera that orbits around its focal point.
///
/// Add it to \c fcg::run and select it through \c Player::setActiveCamera when needed. While active, its matrices
/// apply to all ordinary scene applets. The camera can be controlled with the mouse (see \c onEvent), through its GUI
/// window, and programmatically through the <tt>\ref fcg::Camera "Camera"</tt> interface at any time, including
/// before initialization.
///
/// The projection is always perspective. The viewing direction and up vector are always normalized and never
/// parallel. Every successful setter call and every mouse navigation step stops a running focus animation; a setter
/// that throws leaves the camera unchanged.
class FCG_FRAMEWORK_EXPORT OrbitCamera : public Applet, public Camera
{
public:

	////
	// Object construction/destruction

	/// \brief Create a camera at (0, 0, 3) looking along -Z toward the origin, with up +Y.
	///
	/// The vertical field of view is 60 degrees, the focus distance 3, and the near and far clipping distances 0.125
	/// and 100.
	OrbitCamera();

	/// \brief Destroy the camera.
	///
	/// \pre The \c Player last passed to \c onEvent or \c update is still alive.
	~OrbitCamera() override;


	////
	// Interface: fcg::Applet

	/// \brief Return the applet name.
	///
	/// \return <tt>"Orbit Camera"</tt>.
	[[nodiscard]] auto name () const -> const std::string& override;

	/// \brief Connect camera changes to scene refreshes in the player.
	///
	/// Camera setters are usable before initialization. After initialization, changes request a scene refresh
	/// from \c player, including changes made through a camera pointer from another applet.
	///
	/// \pre The player outlives this camera.
	///
	/// \param device Unused.
	/// \param player The player running this camera, borrowed until camera destruction.
	void init (Device &device, Player &player) override;

	/// \brief No pipelines are needed by this camera.
	void recreatePipelines (Device&, Player&) override {};

	/// \brief Adapt the projection to the new viewport aspect ratio.
	///
	/// Also stops a running focus animation and discards a double-click pick in progress.
	///
	/// \param device
	/// 	Unused.
	/// \param oldViewportSize
	/// 	Unused.
	/// \param player
	/// 	Unused.
	void onViewportResize (Device &device, const glm::uvec2 &oldViewportSize, Player &player) override;

	/// \brief Navigate the camera with the mouse.
	///
	/// - Left drag orbits the eye around the focal point.
	/// - Left drag with Shift held at the press rolls the camera around its viewing direction.
	/// - Right drag pans the camera parallel to the image plane.
	/// - Middle drag moves the eye and the focal point together along the viewing direction.
	/// - The wheel moves the eye toward or away from the focal point, which stays fixed.
	/// - A double-click with the left button and without Shift moves the focal point smoothly to the clicked scene
	///   point (see <tt>\ref fcg::CameraFocus "CameraFocus"</tt>). A press counts as a double-click if the system
	///   reports it as one, or if it follows the previous left press without Shift within 250 ms.
	///
	/// Pan and dolly speeds scale with the focus distance and the speed factor set in the GUI window.
	///
	/// The following events are marked as handled with \c EventContext::markHandled: every press of the left,
	/// middle or right mouse button, the release of the button that started the current drag, mouse motion while
	/// a drag is active, and wheel events with nonzero vertical scroll.
	///
	/// \note
	/// 	Extreme focus distances or world coordinates can lose significant digits during navigation. An accepted
	/// 	configuration can then produce an unusable pose or viewing matrix without an exception; numerical failure
	/// 	is not always detected.
	///
	/// \param event
	/// 	The event to process.
	/// \param context
	/// 	The dispatch context used to mark the event as handled.
	/// \param player
	/// 	The player used for depth picking. Must be the same in every call.
	///
	/// \throws std::invalid_argument
	/// 	If a navigation operation requests a pose or focus distance rejected by the corresponding camera setter.
	/// \throws std::runtime_error
	/// 	If a double-click cannot request the scene depth; see
	/// 	<tt>\ref fcg::CameraFocus::pick "CameraFocus::pick"</tt>.
	void onEvent (const Event &event, EventContext &context, Player &player) override;

	/// \brief Show the camera's GUI window.
	///
	/// The window, initially collapsed, contains widgets for all camera parameters, the navigation speed factor,
	/// buttons for \c resetRotation and \c resetFocus, and a summary of the mouse controls.
	///
	/// \param device Unused.
	/// \param player The player driving this camera.
	///
	/// \throws std::invalid_argument
	/// 	If an eye position or focus distance entered in the GUI is rejected by the corresponding setter.
	void gui (Device &device, Player &player) override;

	/// \brief Advance a running focus animation, or start one after a successful double-click pick.
	///
	/// \param device
	/// 	Unused.
	/// \param player
	/// 	The player used for depth picking. Must be the same in every call.
	/// \param dt
	/// 	The time since the last frame in seconds.
	///
	/// \throws std::runtime_error
	/// 	If the depth for a double-click pick cannot be read; see
	/// 	<tt>\ref fcg::CameraFocus::update "CameraFocus::update"</tt>.
	/// \throws std::logic_error
	/// 	Under the conditions listed for <tt>\ref fcg::CameraFocus::update "CameraFocus::update"</tt>.
	/// \throws std::invalid_argument
	/// 	If an animation step leads to an invalid pose. The animation stops.
	void update (Device &device, Player &player, float dt) override;

	/// \brief Load the camera's modelview and projection matrices into \c renderState.
	///
	/// Replaces the top of both matrix stacks. Does nothing if the viewport is empty.
	///
	/// \param device
	/// 	Unused.
	/// \param renderState
	/// 	The render state shared by the applets drawing the scene.
	/// \param renderPass
	/// 	Unused.
	/// \param commandBuffer
	/// 	Unused.
	/// \param player
	/// 	The player providing the viewport size.
	void render (
		Device &device, RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, Player &player
	) override;


	////
	// Interface: fcg::Camera

	/// \brief Stop navigation and focus activity before dormancy, preserving camera parameters.
	///
	/// \param player The player running this camera.
	void onDeactivate (Player &player) noexcept override;

	/// \brief Access the current camera parameters. The projection is always perspective.
	///
	/// \return A read-only reference that stays valid for the lifetime of the camera.
	[[nodiscard]] inline auto params () const -> const CameraParameters& override {
		return m_params;
	}

	/// \brief Set the perspective vertical field of view.
	///
	/// The angle is clamped to [1, 179] degrees.
	///
	/// \param fovY
	/// 	A <tt>\ref fcg::CameraParameters::Intrinsics::PerspectiveFov "PerspectiveFov"</tt> angle in degrees.
	///
	/// \throws std::invalid_argument
	/// 	If \c fovY is orthographic or the angle is not finite.
	void setFovY (const CameraParameters::Intrinsics::FovY &fovY) override;

	/// \brief Set the focus distance, keeping the eye in place so that the focal point moves along the view direction.
	///
	/// The distance is clamped to at least 0.001.
	///
	/// \note
	/// 	An extreme focus distance can destabilize orbit controls even when accepted. Single-precision arithmetic
	/// 	can lose the small positional offsets needed to calculate a valid viewing pose after moving the eye.
	///
	/// \param f
	/// 	The new focus distance in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c f is not finite or the resulting focal point cannot be represented.
	void setFocalLength (float f) override;

	/// \brief Set the near clipping distance.
	///
	/// The distance is clamped to at least 0.001. If the far clipping distance is less than 0.001 beyond the new
	/// near distance, it is moved out to that minimum.
	///
	/// \param zNear
	/// 	The new near clipping distance in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c zNear is not finite or no greater finite far clipping distance is representable as a
	/// 	\c float value. This includes a request for the largest finite \c float value.
	void setNearPlane (float zNear) override;

	/// \brief Set the far clipping distance.
	///
	/// The distance is clamped to at least the near clipping distance plus 0.001.
	///
	/// \param zFar
	/// 	The new far clipping distance in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c zFar is not finite.
	void setFarPlane (float zFar) override;

	/// \brief Move the eye, keeping orientation and focus distance, so that the focal point moves along.
	///
	/// \note
	/// 	Extreme world coordinates can destabilize orbit controls even when accepted. Single-precision arithmetic
	/// 	can lose small positional or directional offsets as the eye moves, producing an unusable viewing matrix.
	///
	/// \param eye
	/// 	The new eye position in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c eye is not finite or the resulting pose or focal point cannot be represented.
	void setEye (const glm::vec3 &eye) override;

	/// \brief Store normalized perpendicular direction and up vectors together, keeping the eye and focus distance.
	///
	/// \pre The preconditions of \c Camera::setOrientationUnchecked hold.
	///
	/// \param dir The normalized viewing direction in world space.
	/// \param up The normalized up vector in world space.
	void setOrientationUnchecked (const glm::vec3 &dir, const glm::vec3 &up) override;

	/// \brief Turn the camera from its current eye position toward the given point, which becomes the focal point.
	///
	/// The viewing direction and focus distance change to point at \c focalPoint. The up vector is always made
	/// perpendicular to the new viewing direction, staying as close as possible to the current up vector. If the
	/// current up vector is (nearly) parallel to the new direction, +Y is used instead, or +Z when looking (nearly)
	/// along the Y axis.
	///
	/// \note
	/// 	Extreme coordinates or a point far from the eye can destabilize orbit controls even when accepted.
	/// 	Single-precision arithmetic can lose small offsets when moving the eye around a distant focal point.
	///
	/// \param focalPoint
	/// 	The new focal point in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c focalPoint is not finite, closer than 0.001 world units to the eye, or the resulting pose cannot be
	/// 	represented.
	void setFocalPoint (const glm::vec3 &focalPoint) override;

	/// \brief Move the eye so that the given point becomes the focal point, keeping orientation and focus distance.
	///
	/// \note
	/// 	Extreme world coordinates can destabilize orbit controls even when accepted. Single-precision arithmetic
	/// 	can lose small positional or directional offsets during navigation, producing an unusable viewing matrix.
	///
	/// \param focalPoint
	/// 	The new focal point in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c focalPoint is not finite or the resulting pose cannot be represented.
	void translateToFocalPoint (const glm::vec3 &focalPoint) override;

	/// \brief Look along -Z with up +Y, keeping the focal point and the focus distance.
	///
	/// The eye moves to the focal point plus <tt>(0, 0, f)</tt>.
	///
	/// \throws std::invalid_argument
	/// 	If the resulting pose cannot be represented.
	void resetRotation () override;

	/// \brief Turn the camera from its current eye position toward the world origin, as in \c setFocalPoint.
	///
	/// \throws std::invalid_argument
	/// 	If the eye is closer than 0.001 world units to the origin.
	void resetFocus () override;


	////
	// Methods

	/// \brief Set the perspective vertical field of view.
	///
	/// \param fovY
	/// 	The angle in degrees, clamped to [1, 179].
	///
	/// \throws std::invalid_argument
	/// 	If \c fovY is not finite.
	void setFovY (float fovY);


private:

	////
	// Methods

	/// Request a scene refresh from the player bound during initialization, if any.
	void requireSceneRedraw () const noexcept;


	////
	// Fields

	/// The player running this camera, borrowed from initialization until applet destruction.
	Player *m_player = nullptr;

	/// The current camera parameters, as returned by \c params.
	CameraParameters m_params;

	/// The cached world-to-camera transform, absent when invalidated by a pose change.
	std::optional<glm::mat4> viewMatrix;

	/// The cached projection, absent when invalidated by an optical or viewport change.
	std::optional<glm::mat4> projMatrix;

	/// Viewport dimensions used for the cached projection.
	glm::uvec2 projectionViewport{0};

	/// The controller for double-click depth picking and the resulting smooth focus transitions.
	std::unique_ptr<CameraFocus> focusChange;

	/// Multiplier applied to pan and dolly speeds, adjustable in the GUI.
	float speedFactor = 1.f;

	/// The mouse button currently dragging the camera, or \c MouseButton::Unknown when no drag is active.
	MouseButton activeDragButton = MouseButton::Unknown;

	/// The time of the last left press without Shift, if any, for double-click detection.
	std::optional<std::chrono::steady_clock::time_point> lastLeftClickTime;

	/// Whether Shift was held when the active drag started, selecting roll instead of orbit for left drags.
	bool dragStartedWithShift = false;
};



//////
//
// Namespaces close
//

/// @}

} // namespace applet

} // namespace fcg


#endif // ifndef __FCG_ORBIT_CAMERA_H__
