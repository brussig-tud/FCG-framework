
#ifndef __FCG_ORBIT_CAMERA_H__
#define __FCG_ORBIT_CAMERA_H__


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

namespace fcg {
	class CameraFocus;
}



//////
//
// Namespaces open
//

namespace fcg::applet {

/// \defgroup fcg_orbit_camera Orbit camera
/// \ingroup fcg_components
///
/// \brief Perspective camera applet with orbit, pan, roll, dolly, and picked focus.
///
/// <tt>\ref fcg::applet::OrbitCamera "OrbitCamera"</tt> independently implements
/// <tt>\ref fcg::Applet "Applet"</tt> and <tt>\ref fcg::Camera "Camera"</tt>.
/// It stores <tt>\ref fcg::CameraParameters "CameraParameters"</tt> and accepts only perspective fields of view.
///
/// \section fcg_orbit_camera_workflows Common workflows
/// LMB drag orbits, Shift+LMB drag rolls, RMB drag pans, and the wheel or MMB drag dollies.
/// Double-click a rendered object to smoothly translate the focus over half a second.
/// <tt>\ref fcg::applet::OrbitCamera::setFocalPoint "OrbitCamera::setFocalPoint"</tt> retargets from the existing eye.
/// <tt>\ref fcg::applet::OrbitCamera::resetRotation "OrbitCamera::resetRotation"</tt> restores direction -Z and up +Y
/// around the current focus. <tt>\ref fcg::applet::OrbitCamera::resetFocus "OrbitCamera::resetFocus"</tt> retargets
/// the origin from the existing eye.
///
/// \section fcg_orbit_camera_lifetime Ownership and lifetime
/// Queries and setters work before initialization. Run the applet before scene applets so its matrices apply to them.
/// The player must outlive pending readbacks and active focus animations; the normal run loop destroys applets first.
///
/// \section fcg_orbit_camera_errors Errors
/// Nonfinite inputs, zero or parallel axes, orthographic alternatives, and a focus closer than 0.001 world units
/// to the eye throw \c std::invalid_argument without mutation. Focus and clipping distances clamp to 0.001;
/// perspective angles clamp to [1, 179] degrees. Successful manual mutations cancel picked focus transitions.
///
/// \section fcg_orbit_camera_examples Examples
/// \snippet camera_examples.cpp orbit camera
///
/// \see \ref fcg_viewing, \ref fcg_camera_focus, \ref fcg_camera_2d, \ref fcg_events
/// \addtogroup fcg_orbit_camera
/// @{



//////
//
// Classes
//

/// Applet implementing a perspective orbit camera with normalized, nonparallel axes.
class FCG_FRAMEWORK_EXPORT OrbitCamera : public Applet, public Camera
{
public:

	////
	// Object construction/destruction

	/// Initialize a 60-degree perspective camera looking from (0, 0, 3) toward the origin.
	OrbitCamera();

	/// Release the focus controller, collect any pending readback, and balance active redraw requests.
	~OrbitCamera() override;


	////
	// Interface: fcg::Applet

	/// The human-readable name of the camera applet.
	[[nodiscard]] auto name () const -> const std::string& override;

	/// Initialize the camera applet; parameter queries and setters also work before this callback.
	void init (Device &device, Player &player) override;

	/// Invalidate the projection cache and discard focus readbacks invalidated by the viewport resize.
	void onViewportResize (Device &device, const glm::uvec2 &oldViewportSize, Player &player) override;

	/// Handle mouse navigation and double-click focus picking.
	void onEvent (const Event &event, EventContext &context, Player &player) override;

	/// Define the camera parameter, navigation speed, and control-help widgets.
	void gui (Device &device, Player &player) override;

	/// Collect pending focus readbacks, advance focus animations, and update cached matrices.
	void update (Device &device, Player &player, float dt) override;

	/// Install the camera matrices in the render state and retain a snapshot for depth-based picking.
	void render (
		Device &device, RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, Player &player
	) override;


	////
	// Interface: fcg::Camera

	/// Read-only reference the authoritative camera parameters.
	[[nodiscard]] inline auto params () const -> const CameraParameters& override {
		return m_params;
	}

	/// Set a perspective vertical field of view; orthographic alternatives are rejected.
	///
	/// Accepted angles clamp to [1, 179] degrees.
	void setFovY (const CameraParameters::Intrinsics::FovY &fovY) override;

	/// Set the focus distance, clamped to at least 0.001 world units, without moving the eye.
	void setFocalLength (float f) override;

	/// Set the near clipping distance, clamping to 0.001 and extending the far plane if necessary.
	void setNearPlane (float zNear) override;

	/// Set the far clipping distance, clamping to at least the near distance plus 0.001.
	void setFarPlane (float zFar) override;

	/// Set the world-space eye position, preserving orientation and focus distance.
	void setEye (const glm::vec3 &eye) override;

	/// Set and normalize a nonzero viewing direction that is not parallel to the current up vector.
	void setDir (const glm::vec3 &dir) override;

	/// Set and normalize a nonzero up vector that is not parallel to the current viewing direction.
	void setUp (const glm::vec3 &up) override;

	/// Retarget the requested world-space focus from the existing eye, updating direction and focus distance.
	///
	/// The up vector is adjusted to keep the pose nondegenerate. Targets closer than 0.001 world units to the
	/// eye are rejected.
	void setFocalPoint (const glm::vec3 &focalPoint) override;

	/// Translate the eye to establish the requested focus, preserving orientation and focus distance.
	void translateToFocalPoint (const glm::vec3 &focalPoint) override;

	/// Restore direction -Z and up +Y around the current focus, preserving focus distance.
	void resetRotation () override;

	/// Retarget the world origin from the existing eye.
	void resetFocus () override;


	////
	// Methods

	/// Set a perspective vertical field of view in degrees, clamped to [1, 179].
	void setFovY (float fovY);


private:

	////
	// Fields

	/// The authoritative intrinsic and extrinsic camera parameters.
	CameraParameters m_params;

	/// The cached world-to-camera transform, absent when invalidated by a pose change.
	std::optional<glm::mat4> viewMatrix;

	/// The cached projection, absent when invalidated by an optical or viewport change.
	std::optional<glm::mat4> projMatrix;

	/// Owns depth readbacks and smooth focus transitions for this camera.
	std::unique_ptr<CameraFocus> focusChange;

	/// Multiplier applied to translation and zoom speeds.
	float speedFactor = 1.f;

	/// The mouse button currently dragging the camera, or \c MouseButton::Unknown when no drag is active.
	MouseButton activeDragButton = MouseButton::Unknown;

	/// The last left-click time without Shift, if any, for double-click detection.
	std::optional<std::chrono::steady_clock::time_point> lastLeftClickTime;

	/// Whether Shift was held when the active drag started.
	bool dragStartedWithShift = false;
};



/// @}



//////
//
// Namespaces close
//

} // namespace fcg::applet


#endif // ifndef __FCG_ORBIT_CAMERA_H__
