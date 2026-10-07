
#ifndef __FCG_VIEWING_H__
#define __FCG_VIEWING_H__


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

namespace fcg {

/// \defgroup fcg_viewing Viewing and cameras
/// \ingroup fcg_components
///
/// \brief Shared optics, poses, and camera mutations independent of applet lifecycle.
///
/// Use <tt>\ref fcg::CameraParameters "CameraParameters"</tt> for pure matrix calculations, or
/// <tt>\ref fcg::Camera "Camera"</tt> to manipulate a concrete camera through validated setters.
/// Matrices use right-handed coordinates and SDL's zero-to-one depth range. Perspective angles are degrees;
/// orthographic extent is the full vertical world height. The horizontal extent follows the viewport aspect.
///
/// \section fcg_viewing_workflows Common workflows
/// Query parameters through <tt>\ref fcg::Camera::params "Camera::params"</tt> and apply changes through setters.
/// The focal point is always eye plus focus distance times direction, even before applet initialization.
/// <tt>\ref fcg::Camera::setFrustumDiameterAtFocus "Camera::setFrustumDiameterAtFocus"</tt> changes the field of
/// view or orthographic height. Orthographic extent is independent of focus distance.
///
/// \section fcg_viewing_lifetime Ownership and lifetime
/// Parameters are ordinary values. References returned by <tt>\ref fcg::Camera::params "Camera::params"</tt>
/// are borrowed from the camera and expire with it. The interface owns no applet or GPU resources.
///
/// \section fcg_viewing_errors Errors
/// Concrete setters reject nonfinite inputs, unsupported alternatives, and unsupported poses with
/// \c std::invalid_argument, leaving state unchanged. Accepted distances and extents may be clamped.
/// Successful manual mutations cancel focus animations and pending focus application.
/// Parameter calculations require the documented valid inputs; they do not validate or mutate unrelated fields.
/// Adjusting orthographic focus distance from a frustum diameter throws \c std::logic_error.
///
/// \section fcg_viewing_compatibility Source compatibility
/// The former <tt>fcg::applet::OrbitCameraParams</tt> is now
/// <tt>\ref fcg::CameraParameters "fcg::CameraParameters"</tt> with no legacy alias.
/// Its <tt>\ref fcg::CameraParameters::Intrinsics::fovY "fovY"</tt> field now holds a variant.
/// Focal-point access returns a value, and camera applets additionally inherit
/// <tt>\ref fcg::Camera "fcg::Camera"</tt>; update consumers that depend on the previous type or class layout.
///
/// \section fcg_viewing_examples Examples
/// \snippet camera_examples.cpp shared camera
///
/// \see \ref fcg_camera_focus, \ref fcg_orbit_camera, \ref fcg_camera_2d, \ref fcg_render_state
/// \addtogroup fcg_viewing
/// @{



//////
//
// Structs and enums
//

/// Camera optics and world-space pose, suitable for calculations without a player.
struct CameraParameters
{
	////
	// Types

	/// Optical parameters; all scalar values must be finite.
	struct Intrinsics
	{
		////
		// Types

		/// Perspective vertical field of view in degrees.
		struct PerspectiveFov
		{
			/// The vertical angle in degrees, strictly between zero and 180.
			float angle;
		};

		/// Orthographic full vertical extent in world units.
		struct OrthoExtend
		{
			/// The full vertical world height, strictly positive.
			float size;
		};

		/// Supported vertical field-of-view descriptions.
		using FovY = std::variant<PerspectiveFov, OrthoExtend>;


		////
		// Fields

		/// The projection alternative and its vertical field of view.
		FovY fovY;

		/// The positive focus distance in world units.
		float f;

		/// The positive near clipping distance.
		float zNear;

		/// The far clipping distance, greater than <tt>zNear</tt>.
		float zFar;


		////
		// Accessors

		/// \brief Build a right-handed projection with depth in [0, 1].
		///
		/// \pre
		/// 	Viewport dimensions are positive; clipping distances are finite with <tt>0 < zNear < zFar</tt>.
		/// 	The perspective angle is finite and in (0, 180), or orthographic size is finite and positive.
		///
		/// \param viewportSize The drawable viewport dimensions in pixels.
		///
		/// \return The projection matrix for the current optics and viewport aspect ratio.
		[[nodiscard]] inline auto projectionMatrix (const glm::uvec2 &viewportSize) const -> glm::mat4
		{
			const float aspect = (float)viewportSize.x / (float)viewportSize.y;
			if (const auto *perspective = std::get_if<PerspectiveFov>(&fovY))
				return glm::perspectiveRH_ZO(glm::radians(perspective->angle), aspect, zNear, zFar);
			const float halfHeight = .5f * std::get<OrthoExtend>(fovY).size;
			const float halfWidth = halfHeight * aspect;
			return glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, zNear, zFar);
		}

		/// \brief Return full vertical frustum height at the focus distance.
		///
		/// \pre
		/// 	Perspective focus distance is finite and positive and angle is finite and in (0, 180),
		/// 	or orthographic size is finite and positive.
		///
		/// \return The full vertical world height; orthographic height is independent of <tt>f</tt>.
		[[nodiscard]] inline auto frustumDiameterAtFocus () const -> float {
			if (const auto *perspective = std::get_if<PerspectiveFov>(&fovY))
				return 2.f * f * glm::tan(.5f * glm::radians(perspective->angle));
			return std::get<OrthoExtend>(fovY).size;
		}


		////
		// Methods

		/// \brief Adjust vertical field of view while retaining the projection alternative.
		///
		/// \pre The diameter is finite and positive; perspective focus distance is finite and positive.
		///
		/// \param diameter The requested full vertical world height at the focus distance.
		inline void setFovYForFrustumDiameterAtFocus (float diameter) {
			if (auto *perspective = std::get_if<PerspectiveFov>(&fovY))
				perspective->angle = glm::degrees(2.f * glm::atan(.5f * diameter / f));
			else
				std::get<OrthoExtend>(fovY).size = diameter;
		}

		/// \brief Adjust focus distance to obtain the requested perspective frustum height.
		///
		/// \pre The diameter is finite and positive; perspective angle is finite and in (0, 180).
		///
		/// \param diameter The requested full vertical world height at the focus distance.
		///
		/// \throws std::logic_error Orthographic extent does not depend on distance; parameters stay unchanged.
		inline void setFocusDistForFrustumDiameter (float diameter) {
			const auto *perspective = std::get_if<PerspectiveFov>(&fovY);
			if (!perspective)
				throw std::logic_error("Orthographic frustum height is independent of focus distance");
			f = .5f * diameter / glm::tan(.5f * glm::radians(perspective->angle));
		}
	};

	/// World-space position and orientation.
	struct Extrinsics
	{
		////
		// Fields

		/// The eye position in world space.
		glm::vec3 eye;

		/// The viewing direction in world space; concrete cameras keep it normalized.
		glm::vec3 dir;

		/// The up vector in world space; concrete cameras keep it normalized.
		glm::vec3 up;


		////
		// Accessors

		/// \brief Build a right-handed world-to-camera transform.
		///
		/// \pre
		/// 	The pose is finite and nondegenerate: direction and up are nonzero and nonparallel,
		/// 	and <tt>eye + dir</tt> is representable and differs from <tt>eye</tt>.
		///
		/// \return The modelview matrix for the current world-space pose.
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

/// Abstract camera mutations and pure queries, independent of applet lifecycle.
///
/// Concrete cameras expose read-only parameters and validate all mutations through the virtual setters.
/// Queries and setters work before applet initialization. Successful manual mutations cancel active focus
/// animations and pending focus application; rejected mutations leave camera state unchanged.
class FCG_FRAMEWORK_EXPORT Camera
{
public:

	////
	// Object construction/destruction

	/// Destroy a concrete camera through the interface.
	virtual ~Camera();


	////
	// Accessors

	/// Borrow the authoritative parameters; all mutations must pass through setters.
	///
	/// \return A read-only reference, valid until the concrete camera is destroyed.
	[[nodiscard]] virtual auto params () const -> const CameraParameters& = 0;

	/// Compute the world-space focal point as <tt>eye + f * dir</tt>, without initialization or a cache.
	[[nodiscard]] inline auto focalPoint () const -> glm::vec3 {
		return params().extrinsics.eye + params().intrinsics.f * params().extrinsics.dir;
	}

	/// Compute the view transform using the parameter-level preconditions.
	[[nodiscard]] inline auto modelviewMatrix () const -> glm::mat4 {
		return params().extrinsics.modelviewMatrix();
	}

	/// Compute the projection using the parameter-level preconditions.
	///
	/// \param viewportSize The positive drawable viewport dimensions in pixels.
	[[nodiscard]] inline auto projectionMatrix (const glm::uvec2 &viewportSize) const -> glm::mat4 {
		return params().intrinsics.projectionMatrix(viewportSize);
	}

	/// Return full vertical frustum height at focus, in world units.
	[[nodiscard]] inline auto frustumDiameterAtFocus () const -> float {
		return params().intrinsics.frustumDiameterAtFocus();
	}


	////
	// Methods

	/// Select a supported projection description, clamping its scalar value to the concrete camera's range.
	virtual void setFovY (const CameraParameters::Intrinsics::FovY &fovY) = 0;

	/// Set positive focus distance, which may configure focus depth.
	virtual void setFocalLength (float f) = 0;

	/// Set near clipping distance, adjusting the far plane if necessary.
	virtual void setNearPlane (float zNear) = 0;

	/// Set far clipping distance greater than the near distance.
	virtual void setFarPlane (float zFar) = 0;

	/// Set eye position, which may configure focus depth.
	virtual void setEye (const glm::vec3 &eye) = 0;

	/// Set a supported nonzero direction; accepted input is normalized.
	virtual void setDir (const glm::vec3 &dir) = 0;

	/// Set a supported nonzero up vector; accepted input is normalized.
	virtual void setUp (const glm::vec3 &up) = 0;

	/// Establish the requested focus within the concrete camera's constraints.
	virtual void setFocalPoint (const glm::vec3 &focalPoint) = 0;

	/// Translate to a focus while preserving orientation and focus distance.
	virtual void translateToFocalPoint (const glm::vec3 &focalPoint) = 0;

	/// Restore the default orientation while preserving focus and focus distance.
	virtual void resetRotation () = 0;

	/// Reset focus according to the concrete camera's constraints.
	virtual void resetFocus () = 0;

	/// Adjust field of view through the validated virtual setter.
	///
	/// \param diameter The requested full vertical world height at the focus distance.
	void setFrustumDiameterAtFocus (float diameter);

	/// Adjust focus distance through the validated virtual setter.
	///
	/// \param diameter The requested full vertical world height at the focus distance.
	///
	/// \throws std::logic_error Orthographic height is independent of focus distance.
	void setFocusDistForFrustumDiameter (float diameter);
};


/// @}



//////
//
// Namespaces close
//

} // namespace fcg


#endif // ifndef __FCG_VIEWING_H__
