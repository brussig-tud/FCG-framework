

#ifndef __FCG_VIEWING_H__
#define __FCG_VIEWING_H__


//////
//
// Module documentation
//

/// \defgroup fcg_viewing Viewing and cameras
/// \ingroup fcg_modules
/// \brief Shared camera optics and poses, and the interface implemented by the camera applets.
///
/// <tt>\ref fcg::CameraParameters "CameraParameters"</tt> bundles the optics (projection) and the pose (position and
/// orientation) of a camera and computes projection and modelview matrices from them. <tt>\ref fcg::Camera
/// "Camera"</tt> is the abstract interface that the ready-made camera applets <tt>\ref fcg::applet::OrbitCamera
/// "OrbitCamera"</tt> and <tt>\ref fcg::applet::Camera2D "Camera2D"</tt> implement: parameters are queried through
/// <tt>\ref fcg::Camera::params "params"</tt> and changed through validated setters. Write a custom camera by deriving
/// from <tt>\ref fcg::Camera "Camera"</tt> and mirroring every setter in <tt>\ref fcg::Camera::params "params"</tt>.
///
/// Matrices use right-handed coordinates and SDL's zero-to-one depth range. Perspective angles are degrees; the
/// orthographic extent is the full vertical world height. The horizontal extent follows the viewport aspect.
///
/// \section fcg_viewing_usage Getting started
/// The camera interface is shared by all camera applets, so scene code can configure any of them uniformly:
/// \snippet camera_examples.cpp shared camera
///
/// \section fcg_viewing_workflows Common tasks
/// \subsection fcg_viewing_workflows_projection Choosing a projection
/// The projection is either a perspective field of view, in degrees
/// (<tt>\ref fcg::CameraParameters::Intrinsics::PerspectiveFov "PerspectiveFov"</tt>),
/// or an orthographic extent in world units
/// (<tt>\ref fcg::CameraParameters::Intrinsics::OrthoExtend "OrthoExtend"</tt>).
/// <tt>\ref fcg::Camera::setFrustumDiameterAtFocus "setFrustumDiameterAtFocus"</tt> sets the visible world height at
/// the focal point for either projection.
///
/// \subsection fcg_viewing_workflows_mutating Moving the camera
/// The focal point is always eye plus focus distance times view direction, even before the applet has been
/// initialized. Use <tt>\ref fcg::Camera::setFocalPoint "setFocalPoint"</tt> or
/// <tt>\ref fcg::Camera::translateToFocalPoint "translateToFocalPoint"</tt> to aim at a point, and
/// <tt>\ref fcg::Camera::setFovY "setFovY"</tt> or <tt>\ref fcg::Camera::setFocalLength "setFocalLength"</tt> to
/// zoom. How a requested focus point is reached depends on the camera; see the two applet guides.
///
/// \subsection fcg_viewing_workflows_implementing Implementing a custom camera
/// Derive from <tt>\ref fcg::Camera "Camera"</tt>, keep a <tt>\ref fcg::CameraParameters "CameraParameters"</tt>
/// member, and make every setter validate its input and update it so that <tt>\ref fcg::Camera::params "params"</tt>
/// always reflects the current state. If your camera can be focused with <tt>\ref fcg::CameraFocus "CameraFocus"</tt>,
/// its setters should call <tt>\ref fcg::CameraFocus::cancel "CameraFocus::cancel"</tt> to stop a running focus
/// animation before applying a manual change.
///
/// \section fcg_viewing_lifetime Ownership and lifetime
/// Parameters are ordinary values. References returned by <tt>\ref fcg::Camera::params "params"</tt> are borrowed
/// from the camera and expire with it. The interface owns no applet or GPU resources.
///
/// \section fcg_viewing_errors Handling errors
/// Setters reject nonfinite inputs, unsupported projection alternatives, and unsupported poses with
/// \c std::invalid_argument, leaving the state unchanged. Accepted distances and extents may be clamped. Setting the
/// focus distance from a frustum diameter throws \c std::logic_error for orthographic cameras, where the two are
/// independent. Matrix calculations require the documented valid inputs.
///
/// \see \ref fcg_camera_focus, \ref fcg_orbit_camera, \ref fcg_camera_2d, \ref fcg_render_state



//////
//
// Includes
//

// C++ STL
#include <stdexcept>
#include <variant>

// GLM library
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

// Local includes
#include "FCG/export.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_viewing
/// @{



//////
//
// Structs and enums
//

/// \brief Camera optics and world-space pose as plain values.
///
/// Use this struct to compute view and projection matrices without a running player, or read the current state of a
/// camera through <tt>\ref fcg::Camera::params "Camera::params"</tt>. All members are public and unvalidated; the
/// documented valid ranges are preconditions of the matrix calculations.
struct CameraParameters
{
	////
	// Types

	/// Optical parameters of a camera. All scalar values must be finite.
	struct Intrinsics
	{
		////
		// Types

		/// Selects a perspective projection, described by its vertical field-of-view angle.
		struct PerspectiveFov
		{
			/// The full vertical field-of-view angle in degrees, strictly between 0 and 180.
			float angle;
		};

		/// Selects an orthographic projection, described by the full vertical extent of the view volume.
		struct OrthoExtend
		{
			/// The full vertical height of the view volume in world units, strictly positive.
			float size;
		};

		/// \brief
		/// 	The projection type together with its vertical extent: either a perspective field-of-view angle or an
		/// 	orthographic view height.
		///
		/// In both cases, the horizontal extent follows from the viewport aspect ratio.
		using FovY = std::variant<PerspectiveFov, OrthoExtend>;


		////
		// Fields

		/// The projection type and its vertical field of view.
		FovY fovY;

		/// \brief
		/// 	The focus distance in world units, strictly positive.
		///
		/// This is the distance from the eye along the viewing direction to the focal point, i.e. the focal point is
		/// <tt>eye + f * dir</tt>. Cameras orbit around, zoom toward and pan with this point. It is unrelated to the
		/// optical focal length of a lens, even though its setter is named
		/// <tt>\ref fcg::Camera::setFocalLength "Camera::setFocalLength"</tt>.
		float f;

		/// The distance of the near clipping plane from the eye along the viewing direction, in world units.
		/// Must be strictly positive.
		float zNear;

		/// The distance of the far clipping plane from the eye along the viewing direction, in world units.
		/// Must be greater than \c zNear.
		float zFar;


		////
		// Accessors

		/// \brief Compute the projection matrix for these optics and the given viewport.
		///
		/// The matrix maps right-handed eye space (camera looking along -Z) to clip space with depth in [0, 1], as
		/// expected by SDL GPU. The aspect ratio is <tt>viewportSize.x / viewportSize.y</tt>.
		///
		/// \pre
		/// 	Both viewport dimensions are positive, <tt>0 < zNear < zFar</tt>, and the perspective angle is in
		/// 	(0, 180) or the orthographic size is positive.
		///
		/// \param viewportSize
		/// 	The drawable viewport dimensions in pixels, e.g. \c Player::viewportSize.
		///
		/// \return The projection matrix.
		[[nodiscard]] inline auto projectionMatrix (const glm::uvec2 &viewportSize) const -> glm::mat4
		{
			const float aspect = (float)viewportSize.x / (float)viewportSize.y;
			if (const auto *perspective = std::get_if<PerspectiveFov>(&fovY))
				return glm::perspectiveRH_ZO(glm::radians(perspective->angle), aspect, zNear, zFar);
			const float halfHeight = .5f * std::get<OrthoExtend>(fovY).size;
			const float halfWidth = halfHeight * aspect;
			return glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, zNear, zFar);
		}

		/// \brief Compute the full vertical height of the view volume at the focus distance.
		///
		/// For a perspective projection, this is <tt>2 * f * tan(angle / 2)</tt>. For an orthographic projection, it
		/// is the orthographic size, independent of \c f.
		///
		/// \pre
		/// 	For a perspective projection, \c f is positive and the angle is in (0, 180). For an orthographic
		/// 	projection, the size is positive.
		///
		/// \return The vertical height in world units.
		[[nodiscard]] inline auto frustumDiameterAtFocus () const -> float {
			if (const auto *perspective = std::get_if<PerspectiveFov>(&fovY))
				return 2 * f * glm::tan(.5f * glm::radians(perspective->angle));
			return std::get<OrthoExtend>(fovY).size;
		}


		////
		// Methods

		/// \brief
		/// 	Change the vertical field of view so that the view height at the focus distance becomes \c diameter.
		///
		/// Keeps the projection type. For a perspective projection, the angle changes; for an orthographic
		/// projection, the size is set to \c diameter. No clamping is applied.
		///
		/// \pre \c diameter is finite and positive. For a perspective projection, \c f is finite and positive.
		///
		/// \param diameter
		/// 	The requested full vertical height at the focus distance, in world units.
		inline void setFovYForFrustumDiameterAtFocus (float diameter) {
			if (auto *perspective = std::get_if<PerspectiveFov>(&fovY))
				perspective->angle = glm::degrees(2.f * glm::atan(.5f * diameter / f));
			else
				std::get<OrthoExtend>(fovY).size = diameter;
		}

		/// \brief Change the focus distance so that the view height at the focus distance becomes \c diameter.
		///
		/// Only meaningful for a perspective projection; the angle stays unchanged. No clamping is applied.
		///
		/// \pre \c diameter is finite and positive, and the perspective angle is in (0, 180).
		///
		/// \param diameter
		/// 	The requested full vertical height at the focus distance, in world units.
		///
		/// \throws std::logic_error
		/// 	If the projection is orthographic, whose view height does not depend on the focus distance. The
		/// 	parameters stay unchanged.
		inline void setFocusDistForFrustumDiameter (float diameter) {
			const auto *perspective = std::get_if<PerspectiveFov>(&fovY);
			if (!perspective)
				throw std::logic_error("Orthographic frustum height is independent of focus distance");
			f = .5f * diameter / glm::tan(.5f * glm::radians(perspective->angle));
		}
	};

	/// World-space position and orientation of a camera.
	struct Extrinsics
	{
		////
		// Fields

		/// The eye position in world space.
		glm::vec3 eye;

		/// The viewing direction in world space. The framework's cameras keep it normalized.
		glm::vec3 dir;

		/// \brief
		/// 	The up vector in world space. The framework's cameras keep it normalized.
		///
		/// It only needs to be non-parallel to \c dir; the effective up direction of the view is the component of
		/// \c up perpendicular to \c dir.
		glm::vec3 up;


		////
		// Accessors

		/// \brief Compute the right-handed world-to-eye transform, with the camera looking along -Z in eye space.
		///
		/// \pre
		/// 	All vectors are finite, \c dir and \c up are nonzero and not parallel, and \c dir is not negligibly
		/// 	small compared to \c eye (<tt>eye + dir</tt> differs from \c eye).
		///
		/// \return The modelview matrix for this pose.
		[[nodiscard]] inline auto modelviewMatrix () const -> glm::mat4 {
			return glm::lookAtRH(eye, eye + dir, up);
		}
	};


	////
	// Fields

	/// The intrinsic camera parameters, describing its optics.
	Intrinsics intrinsics;

	/// The extrinsic camera parameters, describing its world-space position and orientation.
	Extrinsics extrinsics;
};



//////
//
// Classes
//

/// \brief Common interface of the framework's cameras for reading and changing camera parameters.
///
/// Implemented by <tt>\ref fcg::applet::OrbitCamera "OrbitCamera"</tt> and
/// <tt>\ref fcg::applet::Camera2D "Camera2D"</tt>; use it to control a camera without knowing its concrete type, e.g.
/// one obtained from <tt>\ref fcg::Player::findCamera "Player::findCamera"</tt>. Read the current state through
/// \c params and the derived queries, and change it only through the setters. Queries and setters can be used at any
/// time, including before the camera applet is initialized.
///
/// Each camera supports only some projections and poses and may clamp values to its own ranges; the documentation of
/// the implementing class lists the exact rules. A setter that throws leaves the camera unchanged.
///
/// \note
/// 	For implementers: \c params must reflect the effect of every setter immediately. A camera that uses a
/// 	<tt>\ref fcg::CameraFocus "CameraFocus"</tt> should call
/// 	<tt>\ref fcg::CameraFocus::cancel "CameraFocus::cancel"</tt> in each setter that changes the camera, so that
/// 	manual changes stop a running focus animation. This is also safe in \c translateToFocalPoint, which the
/// 	controller itself calls while animating.
class FCG_FRAMEWORK_EXPORT Camera
{
public:

	////
	// Object construction/destruction

	/// Virtual destructor, allowing cameras to be destroyed through this interface.
	virtual ~Camera();


	////
	// Accessors

	/// \brief Access the current camera parameters.
	///
	/// \return
	/// 	A read-only reference that stays valid for the lifetime of the camera and always reflects its current
	/// 	state.
	[[nodiscard]] virtual auto params () const -> const CameraParameters& = 0;

	/// \brief Compute the world-space focal point.
	///
	/// \return <tt>eye + f * dir</tt> from the current parameters.
	[[nodiscard]] inline auto focalPoint () const -> glm::vec3 {
		return params().extrinsics.eye + params().intrinsics.f * params().extrinsics.dir;
	}

	/// \brief Compute the world-to-eye transform of the current pose.
	///
	/// \return
	/// 	The modelview matrix as computed by
	/// 	<tt>\ref fcg::CameraParameters::Extrinsics::modelviewMatrix "Extrinsics::modelviewMatrix"</tt>.
	[[nodiscard]] inline auto modelviewMatrix () const -> glm::mat4 {
		return params().extrinsics.modelviewMatrix();
	}

	/// \brief Compute the projection matrix of the current optics for the given viewport.
	///
	/// \param viewportSize
	/// 	The drawable viewport dimensions in pixels. Both must be positive.
	///
	/// \return
	/// 	The projection matrix as computed by
	/// 	<tt>\ref fcg::CameraParameters::Intrinsics::projectionMatrix "Intrinsics::projectionMatrix"</tt>.
	[[nodiscard]] inline auto projectionMatrix (const glm::uvec2 &viewportSize) const -> glm::mat4 {
		return params().intrinsics.projectionMatrix(viewportSize);
	}

	/// \brief Compute the full vertical height of the view volume at the focus distance.
	///
	/// \return
	/// 	The height in world units as computed by
	/// 	<tt>\ref fcg::CameraParameters::Intrinsics::frustumDiameterAtFocus "frustumDiameterAtFocus"</tt>.
	[[nodiscard]] inline auto frustumDiameterAtFocus () const -> float {
		return params().intrinsics.frustumDiameterAtFocus();
	}


	////
	// Methods

	/// \brief Set the projection type and its vertical field of view.
	///
	/// The camera may clamp the angle or size to its supported range.
	///
	/// \param fovY
	/// 	The perspective angle in degrees or the orthographic view height in world units.
	///
	/// \throws std::invalid_argument
	/// 	If the value is not finite or the camera does not support the requested projection type.
	virtual void setFovY (const CameraParameters::Intrinsics::FovY &fovY) = 0;

	/// \brief Set the focus distance \c f, the distance from the eye to the focal point along the viewing direction.
	///
	/// Despite its name, this does not set an optical focal length; see
	/// <tt>\ref CameraParameters::Intrinsics::f "CameraParameters::Intrinsics::f"</tt>. The camera may clamp the
	/// value to a minimum.
	///
	/// \param f
	/// 	The new focus distance in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c f is not finite or the resulting focal point cannot be represented.
	virtual void setFocalLength (float f) = 0;

	/// \brief Set the near clipping distance.
	///
	/// The camera may clamp the value to a minimum, and moves the far plane outward if necessary to keep it beyond
	/// the near plane.
	///
	/// \param zNear
	/// 	The new near clipping distance from the eye, in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c zNear is not finite.
	virtual void setNearPlane (float zNear) = 0;

	/// \brief Set the far clipping distance.
	///
	/// The value is clamped to stay greater than the near clipping distance.
	///
	/// \param zFar
	/// 	The new far clipping distance from the eye, in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c zFar is not finite.
	virtual void setFarPlane (float zFar) = 0;

	/// \brief Set the world-space eye position, keeping orientation and focus distance.
	///
	/// \param eye
	/// 	The new eye position.
	///
	/// \throws std::invalid_argument
	/// 	If \c eye is not finite or the resulting pose or focal point cannot be represented.
	virtual void setEye (const glm::vec3 &eye) = 0;

	/// \brief Set the viewing direction, which is normalized on acceptance.
	///
	/// \param dir
	/// 	The new viewing direction in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c dir is zero or not finite, parallel to the up vector, or not supported by the camera.
	virtual void setDir (const glm::vec3 &dir) = 0;

	/// \brief Set the up vector, which is normalized on acceptance.
	///
	/// \param up
	/// 	The new up vector in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c up is zero or not finite, parallel to the viewing direction, or not supported by the camera.
	virtual void setUp (const glm::vec3 &up) = 0;

	/// \brief Make the given world-space point the focal point.
	///
	/// How this is achieved depends on the camera, e.g. turning toward the point or panning to it; see the
	/// implementing class.
	///
	/// \param focalPoint
	/// 	The new focal point in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c focalPoint is not finite or cannot be reached under the camera's constraints.
	virtual void setFocalPoint (const glm::vec3 &focalPoint) = 0;

	/// \brief Move the eye so that the given point becomes the focal point, keeping orientation and focus distance.
	///
	/// \param focalPoint
	/// 	The new focal point in world space.
	///
	/// \throws std::invalid_argument
	/// 	If \c focalPoint is not finite or cannot be reached under the camera's constraints.
	virtual void translateToFocalPoint (const glm::vec3 &focalPoint) = 0;

	/// \brief
	/// 	Restore the default orientation (looking along -Z with up +Y), keeping the focal point and focus distance.
	///
	/// \throws std::invalid_argument
	/// 	If the resulting pose cannot be represented.
	virtual void resetRotation () = 0;

	/// \brief Move the focal point to the world origin in the camera's own way; see the implementing class.
	///
	/// \throws std::invalid_argument
	/// 	If the camera cannot focus the origin from its current state.
	virtual void resetFocus () = 0;

	/// \brief Change the vertical field of view so that the view height at the focus distance becomes \c diameter.
	///
	/// For a perspective camera, the angle changes; for an orthographic camera, the view height is set directly. The
	/// diameter is first clamped to at least 0.001, and the result is then subject to the clamping of \c setFovY.
	///
	/// \param diameter
	/// 	The requested full vertical height at the focus distance, in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c diameter is not finite, or if \c setFovY rejects the result.
	void setFrustumDiameterAtFocus (float diameter);

	/// \brief Change the focus distance so that the view height at the focus distance becomes \c diameter.
	///
	/// Only supported for perspective cameras; the angle and the eye stay unchanged. The diameter is first clamped to
	/// at least 0.001, and the result is then subject to the clamping of \c setFocalLength.
	///
	/// \param diameter
	/// 	The requested full vertical height at the focus distance, in world units.
	///
	/// \throws std::invalid_argument
	/// 	If \c diameter is not finite, or if \c setFocalLength rejects the result.
	/// \throws std::logic_error
	/// 	If the camera uses an orthographic projection, whose view height does not depend on the focus distance.
	/// 	The camera stays unchanged.
	void setFocusDistForFrustumDiameter (float diameter);
};



/// @}



//////
//
// Namespaces close
//

} // namespace fcg


#endif // ifndef __FCG_VIEWING_H__
