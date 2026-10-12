

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
/// <tt>\ref fcg::run "run"</tt> before your scene applets and navigate with the mouse while it installs its viewing
/// matrices for the applets that follow. It accepts only perspective fields of view. All parameters can also be set
/// programmatically through the <tt>\ref fcg::Camera "Camera"</tt> interface, already before the applet is
/// initialized.
///
/// \section fcg_orbit_camera_usage Getting started
/// Create the applet, optionally configure it, and put it in front of the scene applets:
/// \snippet camera_examples.cpp orbit camera
///
/// \section fcg_orbit_camera_workflows Common tasks
/// \subsection fcg_orbit_camera_workflows_controls Mouse controls
/// - Left-drag orbits around the focal point.
/// - Shift+left-drag rolls the camera.
/// - Right-drag pans.
/// - The mouse wheel dollies: it changes the field of view while keeping the focal point fixed.
/// - Middle-drag moves the eye along the view direction while keeping the focus distance, so the focal point moves
///   with the camera.
///
/// Double-click a rendered object to smoothly translate the focus there over half a second; see \ref
/// fcg_camera_focus. The camera consumes these mouse events, so scene applets placed after it do not receive them.
///
/// \subsection fcg_orbit_camera_workflows_programmatic Programmatic navigation
/// <tt>\ref fcg::applet::OrbitCamera::setFocalPoint "setFocalPoint"</tt> retargets the focus from the existing eye,
/// <tt>\ref fcg::applet::OrbitCamera::resetRotation "resetRotation"</tt> restores direction -Z and up +Y around the
/// current focus, and <tt>\ref fcg::applet::OrbitCamera::resetFocus "resetFocus"</tt> retargets the world origin from
/// the existing eye. Manual mutations cancel a running picked-focus animation.
///
/// \section fcg_orbit_camera_lifetime Ownership and lifetime
/// Run the applet before the scene applets it should serve. The <tt>\ref fcg::Player "Player"</tt> must outlive the
/// applet; the normal run loop guarantees this.
///
/// \section fcg_orbit_camera_errors Handling errors
/// Nonfinite inputs, zero or parallel axes, orthographic alternatives, and a focus closer than 0.001 world units to
/// the eye throw \c std::invalid_argument without changing the camera. Focus and clipping distances clamp to 0.001,
/// and perspective angles clamp to [1, 179] degrees.
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
/// Add it to \c fcg::run before the applets that draw the scene, so that the matrices it loads into the
/// \c RenderState apply to them. The camera can be controlled with the mouse (see \c onEvent), through its GUI
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

	/// \brief Does nothing; the camera is fully usable from construction on.
	///
	/// \param device
	/// 	Unused.
	/// \param player
	/// 	Unused.
	void init (Device &device, Player &player) override;

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
	/// \param event
	/// 	The event to process.
	/// \param context
	/// 	The dispatch context used to mark the event as handled.
	/// \param player
	/// 	The player used for depth picking. Must be the same in every call.
	///
	/// \throws std::invalid_argument
	/// 	If a navigation step would lead to an invalid pose, e.g. one that cannot be represented in single
	/// 	precision. The camera remains in a valid state.
	/// \throws std::runtime_error
	/// 	If a double-click cannot request the scene depth; see
	/// 	<tt>\ref fcg::CameraFocus::pick "CameraFocus::pick"</tt>.
	void onEvent (const Event &event, EventContext &context, Player &player) override;

	/// \brief Show the camera's GUI window.
	///
	/// The window, initially collapsed, contains widgets for all camera parameters, the navigation speed factor,
	/// buttons for \c resetRotation and \c resetFocus, and a summary of the mouse controls.
	///
	/// \param device
	/// 	Unused.
	/// \param player
	/// 	Unused.
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
	/// 	If \c zNear is not finite.
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
	/// \param eye
	/// 	The new eye position in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c eye is not finite or the resulting pose or focal point cannot be represented.
	void setEye (const glm::vec3 &eye) override;

	/// \brief Set the viewing direction, keeping the eye, the up vector and the focus distance.
	///
	/// \param dir
	/// 	The new viewing direction in world space; it is normalized.
	///
	/// \throws std::invalid_argument
	/// 	If \c dir is zero or not finite, (nearly) parallel to the current up vector, or the resulting pose cannot
	/// 	be represented.
	void setDir (const glm::vec3 &dir) override;

	/// \brief Set the up vector, keeping the eye, the viewing direction and the focus distance.
	///
	/// \param up
	/// 	The new up vector in world space; it is normalized.
	///
	/// \throws std::invalid_argument
	/// 	If \c up is zero or not finite, (nearly) parallel to the current viewing direction, or the resulting pose
	/// 	cannot be represented.
	void setUp (const glm::vec3 &up) override;

	/// \brief Turn the camera from its current eye position toward the given point, which becomes the focal point.
	///
	/// The viewing direction and focus distance change to point at \c focalPoint. The up vector is always made
	/// perpendicular to the new viewing direction, staying as close as possible to the current up vector. If the
	/// current up vector is (nearly) parallel to the new direction, +Y is used instead, or +Z when looking (nearly)
	/// along the Y axis.
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
	// Fields

	/// The current camera parameters, as returned by \c params.
	CameraParameters m_params;

	/// The cached world-to-camera transform, absent when invalidated by a pose change.
	std::optional<glm::mat4> viewMatrix;

	/// The cached projection, absent when invalidated by an optical or viewport change.
	std::optional<glm::mat4> projMatrix;

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



/// @}



//////
//
// Namespaces close
//

} // namespace applet

} // namespace fcg


#endif // ifndef __FCG_ORBIT_CAMERA_H__
