
#ifndef __FCG_CAMERA_2D_H__
#define __FCG_CAMERA_2D_H__


//////
//
// Module documentation
//

/// \defgroup fcg_camera_2d Planar camera
/// \ingroup fcg_modules
/// \brief An orthographic camera applet for XY scenes with pan, zoom, and rotation around Z.
///
/// <tt>\ref fcg::applet::Camera2D "Camera2D"</tt> is a ready-to-use camera for 2D scenes: add it to
/// <tt>\ref fcg::run "run"</tt> before your scene applets and it installs its viewing matrices for the applets that
/// follow. It looks along -Z and only zooms, pans and rotates in the XY plane; the focus depth stays fixed, so zoom
/// and focus movement never change what is sharp. All parameters can also be set programmatically through the
/// <tt>\ref fcg::Camera "Camera"</tt> interface, already before the applet is initialized.
///
/// \section fcg_camera_2d_usage Getting started
/// Create the applet, optionally configure it, and put it in front of the scene applets:
/// \snippet camera_examples.cpp planar camera
///
/// \section fcg_camera_2d_workflows Common tasks
/// \subsection fcg_camera_2d_workflows_controls Mouse controls
/// - Right-drag pans in the rotated camera plane.
/// - Shift+left-drag rotates around Z (Shift is latched at drag start).
/// - The mouse wheel or middle-drag zooms by scaling the vertical world height around the current focus.
///
/// A plain left-button double click smoothly centers the picked XY position over half a second; see
/// \ref fcg_camera_focus. Ordinary left-dragging is left for scene applets. The height stays constant across
/// resizes, while the horizontal extent follows the aspect ratio.
///
/// \subsection fcg_camera_2d_workflows_programmatic Programmatic navigation
/// <tt>\ref fcg::applet::Camera2D::resetRotation "resetRotation"</tt> restores up +Y, and
/// <tt>\ref fcg::applet::Camera2D::resetFocus "resetFocus"</tt> centers the XY origin; both take effect immediately
/// and preserve zoom and depth. Rotation is measured in degrees in [-180, 180), with 0 meaning up +Y.
/// <tt>\ref fcg::Camera::setUp "Camera::setUp"</tt> projects the supplied up vector onto XY and normalizes it;
/// tilted inputs with a usable XY component are accepted. <tt>\ref fcg::Camera::setDir "Camera::setDir"</tt>
/// accepts normalized directions within <tt>1e-5</tt> of -Z and stores exactly -Z.
/// <tt>\ref fcg::Camera::setOrientation "Camera::setOrientation"</tt> changes both axes together under the same
/// direction restriction, keeping the up vector exactly in XY.
///
/// \section fcg_camera_2d_lifetime Ownership and lifetime
/// Run the applet before the scene applets it should serve. The <tt>\ref fcg::Player "Player"</tt> must outlive the
/// applet; the normal run loop guarantees this.
///
/// \section fcg_camera_2d_errors Handling errors
/// Unsupported projection alternatives or viewing directions, zero or nearly parallel axes, changed focus depth,
/// and nonfinite inputs throw \c std::invalid_argument without changing the camera. Direction and parallel checks
/// use a normalized tolerance of \c 1e-5; focus depth uses a world-unit tolerance of \c 1e-5. The height clamps to
/// [0.001, 100000] world units, focus and near distances to at least 0.001, and the far distance to at least near
/// plus 0.001.
///
/// \see \ref fcg_viewing, \ref fcg_camera_focus, \ref fcg_orbit_camera, \ref fcg_applets



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

/// \addtogroup fcg_camera_2d
/// @{



//////
//
// Classes
//

/// \brief Applet providing an interactive orthographic camera for XY scenes, with pan, zoom and rotation around Z.
///
/// Add it to \c fcg::run before the applets that draw the scene, so that the matrices it loads into the
/// \c RenderState apply to them. The camera can be controlled with the mouse (see \c onEvent), through its GUI
/// window, and programmatically through the <tt>\ref fcg::Camera "Camera"</tt> interface at any time, including
/// before initialization.
///
/// The camera always looks along -Z, and its up vector always lies in the XY plane and is normalized; its angle is
/// available as \c rotation. The projection is always orthographic: the view height is the full vertical extent of
/// the visible area in world units, stays constant when the window is resized, and the horizontal extent follows the
/// viewport aspect ratio.
///
/// The <em>focus Z</em> is the Z coordinate of the focal point, <tt>eye.z - f</tt>. Only \c setEye, \c setEyeZ and
/// \c setFocalLength change it; all methods that move the focal point, as well as double-click picking, preserve it
/// exactly.
///
/// Every successful setter call and every mouse navigation step stops a running focus animation; a setter that
/// throws leaves the camera unchanged.
class FCG_FRAMEWORK_EXPORT Camera2D : public Applet, public Camera
{
public:

	////
	// Object construction/destruction

	/// \brief Create a camera at (0, 0, 3) looking along -Z with up +Y.
	///
	/// The view height is 4, the focus distance 3 (so the focal point is the origin and the focus Z is 0), and the
	/// near and far clipping distances 0.125 and 100.
	Camera2D();

	/// \brief Destroy the camera.
	///
	/// \pre The \c Player last passed to \c onEvent or \c update is still alive.
	~Camera2D() override;


	////
	// Interface: fcg::Applet

	/// \brief Return the applet name.
	///
	/// \return <tt>"2D Camera"</tt>.
	[[nodiscard]] auto name () const -> const std::string& override;

	/// \brief Does nothing; the camera is fully usable from construction on.
	///
	/// \param device
	/// 	Unused.
	/// \param player
	/// 	Unused.
	void init (Device &device, Player &player) override;

	/// \brief No pipelines are needed by this camera.
	void recreatePipelines (Device&, Player&) override {};

	/// \brief Adapt the projection to the new viewport aspect ratio, keeping the view height.
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
	/// - Right drag pans the camera in its rotated image plane; at speed factor 1, the scene follows the cursor.
	/// - Left drag with Shift held at the press rotates the camera around the Z axis.
	/// - The wheel and middle drag zoom by changing the view height, clamped to [0.001, 100000]. The zoom is
	///   centered at the focal point.
	/// - A double-click with the left button and no modifier keys (Shift, Ctrl, Alt or GUI) pans smoothly so that
	///   the clicked scene point is centered (see <tt>\ref fcg::CameraFocus "CameraFocus"</tt>); the focus Z is
	///   preserved. A press counts as a double-click if the system reports it as one, or if it follows the previous
	///   unmodified left press within 250 ms.
	/// - A left drag without Shift does nothing.
	///
	/// Pan and zoom speeds scale with the speed factor set in the GUI window.
	///
	/// The following events are marked as handled with \c EventContext::markHandled: every press of the middle or
	/// right mouse button, every left press with Shift, an unmodified left press only if it completes a double-click
	/// and picking starts, the release of the button that started the current drag, mouse motion while a drag is
	/// active, and wheel events with nonzero vertical scroll.
	///
	/// \param event
	/// 	The event to process.
	/// \param context
	/// 	The dispatch context used to mark the event as handled.
	/// \param player
	/// 	The player used for depth picking and for the viewport size. Must be the same in every call.
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
	/// The window, initially collapsed, contains widgets for the view height, focus distance, clipping distances,
	/// eye position, focal point XY, rotation and navigation speed factor, buttons for \c resetRotation and
	/// \c resetFocus, and a summary of the mouse controls.
	///
	/// \param device Unused.
	/// \param player The player driving this camera.
	///
	/// \throws std::invalid_argument
	/// 	If a value entered in the GUI is rejected by the corresponding setter.
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

	/// \brief Access the current camera parameters.
	///
	/// The projection is always orthographic and the viewing direction is exactly -Z.
	///
	/// \return A read-only reference that stays valid for the lifetime of the camera.
	[[nodiscard]] inline auto params () const -> const CameraParameters& override {
		return m_params;
	}

	/// \brief Set the orthographic view height.
	///
	/// The height is clamped to [0.001, 100000] world units.
	///
	/// \param fovY
	/// 	An <tt>\ref fcg::CameraParameters::Intrinsics::OrthoExtend "OrthoExtend"</tt> height in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c fovY is perspective or the height is not finite.
	void setFovY (const CameraParameters::Intrinsics::FovY &fovY) override;

	/// \brief Set the focus distance, keeping the eye in place.
	///
	/// The distance is clamped to at least 0.001. Since the eye stays in place, the focus Z changes to
	/// <tt>eye.z - f</tt>.
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

	/// \brief Move the eye, keeping rotation and focus distance, so that the focal point moves along.
	///
	/// Changing the eye's Z coordinate changes the focus Z by the same amount.
	///
	/// \param eye
	/// 	The new eye position in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c eye is not finite or the resulting focal point cannot be represented.
	void setEye (const glm::vec3 &eye) override;

	/// \brief Set direction and up together, snapping a supported normalized direction to exactly -Z.
	///
	/// Directions within <tt>1e-5</tt> of -Z are accepted. The prepared up vector is projected onto XY and
	/// normalized again. The eye and focus distance stay unchanged.
	///
	/// \pre The preconditions of \c Camera::setOrientationUnchecked hold.
	///
	/// \param dir The normalized viewing direction in world space.
	/// \param up The normalized up vector perpendicular to \p dir, projected onto XY before storage.
	///
	/// \throws std::invalid_argument
	/// 	If \c dir differs from -Z by more than <tt>1e-5</tt> or the resulting pose cannot be represented. The camera
	/// 	remains unchanged, including any running focus animation.
	void setOrientationUnchecked (const glm::vec3 &dir, const glm::vec3 &up) override;

	/// \brief Pan so that the given point becomes the focal point; same as \c translateToFocalPoint.
	///
	/// \param focalPoint
	/// 	The new focal point in world space. Its Z coordinate must match the current focus Z within <tt>1e-5</tt>
	/// 	world units; the focus Z itself stays exactly unchanged.
	///
	/// \throws std::invalid_argument
	/// 	If \c focalPoint is not finite, its Z coordinate differs from the focus Z, or the resulting pose cannot be
	/// 	represented.
	void setFocalPoint (const glm::vec3 &focalPoint) override;

	/// \brief Pan so that the given point becomes the focal point, keeping rotation, zoom and focus distance.
	///
	/// \param focalPoint
	/// 	The new focal point in world space. Its Z coordinate must match the current focus Z within <tt>1e-5</tt>
	/// 	world units; the focus Z itself stays exactly unchanged.
	///
	/// \throws std::invalid_argument
	/// 	If \c focalPoint is not finite, its Z coordinate differs from the focus Z, or the resulting pose cannot be
	/// 	represented.
	void translateToFocalPoint (const glm::vec3 &focalPoint) override;

	/// Restore up +Y (rotation 0), keeping the focal point, zoom and focus Z.
	void resetRotation () override;

	/// \brief Pan so that the focal point is at X = Y = 0, keeping rotation, zoom and focus Z.
	///
	/// \throws std::invalid_argument
	/// 	If the resulting pose cannot be represented.
	void resetFocus () override;


	////
	// Accessors

	/// \brief Return the rotation of the camera around the Z axis.
	///
	/// Zero means up +Y. Positive angles rotate the camera counterclockwise as seen from +Z (the up vector turns
	/// from +Y toward -X), so the scene appears rotated clockwise on screen. The up vector is
	/// <tt>(-sin(angle), cos(angle), 0)</tt>.
	///
	/// \return The angle in degrees, in [-180, 180).
	[[nodiscard]] auto rotation () const -> float;


	////
	// Methods

	/// \brief Set the orthographic view height.
	///
	/// \param height
	/// 	The full vertical extent of the visible area in world units, clamped to [0.001, 100000].
	///
	/// \throws std::invalid_argument
	/// 	If \c height is not finite.
	void setViewHeight (float height);

	/// \brief Set the eye's Z coordinate, keeping eye X and Y and the focus distance.
	///
	/// The focus Z moves by the same amount as the eye.
	///
	/// \param z
	/// 	The new Z coordinate of the eye.
	///
	/// \throws std::invalid_argument
	/// 	If \c z is not finite or the resulting focal point cannot be represented.
	void setEyeZ (float z);

	/// \brief Set the rotation of the camera around the Z axis, with the sign convention described at \c rotation.
	///
	/// \param angle
	/// 	The angle in degrees. Any finite value is accepted and wrapped to [-180, 180).
	///
	/// \throws std::invalid_argument
	/// 	If \c angle is not finite.
	void setRotation (float angle);

	/// \brief Pan so that the focal point has the given X and Y coordinates, keeping the focus Z.
	///
	/// \param focalPoint
	/// 	The new X and Y coordinates of the focal point in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c focalPoint is not finite or the resulting pose cannot be represented.
	void setFocalPoint (const glm::vec2 &focalPoint);


private:

	////
	// Fields

	/// The current camera parameters, as returned by \c params.
	CameraParameters m_params;

	/// The cached world-to-camera transform, absent when invalidated by a pose change.
	std::optional<glm::mat4> viewMatrix;

	/// The cached projection, absent when invalidated by an optical or viewport change.
	std::optional<glm::mat4> projMatrix;

	/// The controller for double-click depth picking and the resulting planar focus transitions.
	std::unique_ptr<CameraFocus> focusChange;

	/// Multiplier applied to pan and zoom speeds, adjustable in the GUI.
	float speedFactor = 1.f;

	/// The mouse button currently dragging the camera, or \c MouseButton::Unknown when no drag is active.
	MouseButton activeDragButton = MouseButton::Unknown;

	/// The time of the last unmodified left press, if any, for double-click detection.
	std::optional<std::chrono::steady_clock::time_point> lastLeftClickTime;
};



//////
//
// Namespaces close
//

/// @}

} // namespace applet

} // namespace fcg


#endif // ifndef __FCG_CAMERA_2D_H__
