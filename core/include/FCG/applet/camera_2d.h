
#ifndef __FCG_CAMERA_2D_H__
#define __FCG_CAMERA_2D_H__


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

/// \defgroup fcg_camera_2d Planar camera
/// \ingroup fcg_components
///
/// \brief Orthographic camera applet for XY scenes with pan, zoom, and rotation around Z.
///
/// <tt>\ref fcg::applet::Camera2D "Camera2D"</tt> independently implements <tt>\ref fcg::Applet "Applet"</tt> and
/// <tt>\ref fcg::Camera "Camera"</tt>. Its <tt>\ref fcg::CameraParameters "CameraParameters"</tt> are authoritative:
/// direction stays exactly -Z and up stays normalized in XY. Zoom and focus movement preserve configured depth.
///
/// \section fcg_camera_2d_workflows Common workflows
/// RMB drag pans in the rotated camera plane; Shift+LMB drag rotates around Z, latching Shift at drag start.
/// Ordinary LMB dragging is ignored. Wheel or MMB drag scales the vertical world height around the current focus.
/// Height stays constant across resizes, while horizontal extent follows the aspect ratio.
/// Unmodified LMB double-click smoothly centers picked XY over half a second. Depth is unprojected using the last
/// rendered matrices and drawable viewport; background depth one is ignored.
/// <tt>\ref fcg::applet::Camera2D::resetRotation "Camera2D::resetRotation"</tt> restores up +Y;
/// <tt>\ref fcg::applet::Camera2D::resetFocus "Camera2D::resetFocus"</tt> centers XY at the origin.
/// Both reset immediately and preserve zoom and depth.
///
/// \section fcg_camera_2d_lifetime Ownership and lifetime
/// Queries and setters work before initialization. Run this applet before scene applets to install its matrices.
/// The player must outlive pending readbacks and active focus animations; the normal run loop destroys applets first.
/// Cancelled readbacks are collected without applying their results; resize-invalidated tokens are discarded.
///
/// \section fcg_camera_2d_errors Errors
/// Unsupported projection alternatives, tilted or zero axes, changed focus depth, and nonfinite inputs throw
/// \c std::invalid_argument without changing state. Direction and plane checks use a normalized tolerance of
/// <tt>1e-5</tt>; focus Z uses a world-unit tolerance of <tt>1e-5</tt>, preserving current Z exactly.
/// Height clamps to [0.001, 100000], focus and near distances to at least 0.001, and far distance to at least
/// near plus 0.001. Rotation uses degrees in [-180, 180). Successful manual changes cancel picked focus transitions.
/// Explicit eye and focus-distance setters can configure depth; focus-movement methods preserve it.
///
/// \section fcg_camera_2d_examples Examples
/// \snippet camera_examples.cpp planar camera
///
/// \see \ref fcg_viewing, \ref fcg_camera_focus, \ref fcg_orbit_camera, \ref fcg_events
/// \addtogroup fcg_camera_2d
/// @{



//////
//
// Classes
//

/// Orthographic camera constrained to direction -Z and a normalized XY up vector.
class FCG_FRAMEWORK_EXPORT Camera2D : public Applet, public Camera
{
public:

	////
	// Object construction/destruction

	/// Initialize height 4, focus distance 3, clipping distances 0.125/100, eye (0, 0, 3), and up +Y.
	Camera2D();

	/// Release the focus controller, collect any pending readback, and balance active redraw requests.
	~Camera2D() override;


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

	/// Set an orthographic vertical extent; perspective alternatives are rejected.
	///
	/// Accepted heights clamp to [0.001, 100000] world units.
	void setFovY (const CameraParameters::Intrinsics::FovY &fovY) override;

	/// Set the focus distance, clamped to at least 0.001 world units, without moving the eye.
	///
	/// This setter can change the configured focus depth.
	void setFocalLength (float f) override;

	/// Set the near clipping distance, clamping to 0.001 and extending the far plane if necessary.
	void setNearPlane (float zNear) override;

	/// Set the far clipping distance, clamping to at least the near distance plus 0.001.
	void setFarPlane (float zFar) override;

	/// Set the world-space eye position, preserving orientation and focus distance.
	///
	/// This setter can change the configured focus depth.
	void setEye (const glm::vec3 &eye) override;

	/// Accept a nonzero direction whose normalized value matches -Z within <tt>1e-5</tt>.
	///
	/// Accepted directions are snapped to exactly -Z; all other directions are rejected.
	void setDir (const glm::vec3 &dir) override;

	/// Set a nonzero planar up vector, normalizing it and snapping its Z component to zero.
	///
	/// The normalized Z component must be within <tt>1e-5</tt> of zero; tilted vectors are rejected.
	void setUp (const glm::vec3 &up) override;

	/// Pan to the requested XY focus, preserving configured Z exactly.
	///
	/// The requested Z must match the current focus depth within <tt>1e-5</tt> world units.
	void setFocalPoint (const glm::vec3 &focalPoint) override;

	/// Pan to the requested XY focus, preserving orientation, focus distance, and configured Z exactly.
	///
	/// The requested Z must match the current focus depth within <tt>1e-5</tt> world units.
	void translateToFocalPoint (const glm::vec3 &focalPoint) override;

	/// Restore up +Y immediately, preserving focus, zoom, and depth.
	void resetRotation () override;

	/// Center focus XY at the origin immediately, preserving rotation, zoom, and depth.
	void resetFocus () override;


	////
	// Accessors

	/// Derive rotation from the up vector, in degrees in [-180, 180).
	[[nodiscard]] auto rotation () const -> float;


	////
	// Methods

	/// Set the full vertical world height, clamped to [0.001, 100000].
	void setViewHeight (float height);

	/// Set eye Z without changing eye XY or focus distance.
	void setEyeZ (float z);

	/// Set planar rotation in degrees, normalized to [-180, 180); zero means up +Y.
	void setRotation (float angle);

	/// Pan to an XY focus, preserving configured Z exactly.
	void setFocalPoint (const glm::vec2 &focalPoint);


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

	/// The last unmodified left-click time, if any, for double-click detection.
	std::optional<std::chrono::steady_clock::time_point> lastLeftClickTime;
};



/// @}



//////
//
// Namespaces close
//

} // namespace fcg::applet


#endif // ifndef __FCG_CAMERA_2D_H__
