
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <stdexcept>

// Local includes
#include "FCG/viewing.h"
#include "camera_utils.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// \brief Project an up vector onto the plane perpendicular to a finite unit viewing direction.
///
/// \param dir The viewing direction.
/// \param up The up vector; its magnitude is removed before testing the angle between the axes.
///
/// \return The normalized projection, calculated in double precision to limit cancellation near parallel axes.
///
/// \throws std::invalid_argument If \c up is zero, nonfinite, or within the normalized parallel tolerance.
[[nodiscard]] auto projectedUp (const glm::vec3 &dir, const glm::vec3 &up) -> glm::vec3
{
	const auto direction = glm::normalize(glm::dvec3(dir));
	const auto upwards = glm::dvec3(fcg::detail::normalized(up));
	const auto right = glm::cross(direction, upwards);
	if (glm::length(right) < 1e-5)
		throw std::invalid_argument("Camera axes must be nonparallel");
	return glm::vec3(glm::normalize(glm::cross(right, direction)));
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// Camera

Camera::~Camera() = default;

void Camera::setDir (const glm::vec3 &dir) {
	setOrientation(dir, params().extrinsics.up);
}

void Camera::setUp (const glm::vec3 &up) {
	setOrientation(params().extrinsics.dir, up);
}

void Camera::setOrientation (const glm::vec3 &dir, const glm::vec3 &up) {
	const auto direction = detail::normalized(dir);
	const auto upwards = projectedUp(direction, up);
	const auto &before = params();
	detail::requireEye(before.extrinsics.eye, direction, upwards, before.intrinsics.f);
	setOrientationUnchecked(direction, upwards);
}

void Camera::setFrustumDiameterAtFocus (float diameter) {
	if (!std::isfinite(diameter))
		throw std::invalid_argument("Frustum diameter must be finite");
	auto intrinsics = params().intrinsics;
	intrinsics.setFovYForFrustumDiameterAtFocus(glm::max(diameter, .001f));
	setFovY(intrinsics.fovY);
}

void Camera::setFocusDistForFrustumDiameter (float diameter) {
	if (!std::isfinite(diameter))
		throw std::invalid_argument("Frustum diameter must be finite");
	auto intrinsics = params().intrinsics;
	intrinsics.setFocusDistForFrustumDiameter(glm::max(diameter, .001f));
	setFocalLength(intrinsics.f);
}



//////
//
// Module namespace close
//

} // namespace fcg
