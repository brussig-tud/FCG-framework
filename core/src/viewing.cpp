
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <limits>
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

/// \brief Validate the original eye-to-target distance for a framing adjustment.
///
/// \param distance The requested axial distance in world units, including for orthographic cameras.
///
/// \throws std::invalid_argument If the distance is nonfinite or nonpositive.
void requireDistance (float distance) {
	if (!std::isfinite(distance) || distance <= 0.f)
		throw std::invalid_argument("Camera target distance must be finite and positive");
}

/// \brief Convert a computed setter argument without overflowing single precision.
///
/// \param value The computed argument, which may be negative before camera clamping.
///
/// \return The single-precision argument.
///
/// \throws std::invalid_argument If the value is nonfinite or exceeds the finite float range.
[[nodiscard]] auto framingArgument (double value) -> float {
	if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
		throw std::invalid_argument("Camera framing result must be representable in single precision");
	return (float)value;
}

/// \brief Move a perspective camera to a computed target distance, preserving its original focal point.
///
/// \param camera The camera to adjust through its validated setters.
/// \param targetDistance The desired eye-to-target distance in world units.
/// \param distance The original eye-to-target distance in world units.
///
/// \throws std::invalid_argument
/// 	If the focus-distance argument is unrepresentable or a setter rejects the result. Translation failure leaves
/// 	the applied focus distance in effect.
void adjustTargetDistance (fcg::Camera &camera, double targetDistance, float distance)
{
	// Snapshot the focus before invoking setters, which can clamp and invalidate parameter references
	const auto &before = camera.params();
	const auto focalPoint = before.extrinsics.eye + before.intrinsics.f * before.extrinsics.dir;
	const float focusDistance = framingArgument(((double)before.intrinsics.f - distance) + targetDistance);

	// Restore the focus using the actual distance accepted by the camera
	camera.setFocalLength(focusDistance);
	camera.translateToFocalPoint(focalPoint);
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

void Camera::adjustForFrustumDiameterAtDistance (float diameter, float distance)
{
	// Validate all inputs before invoking any camera setters
	requireDistance(distance);
	if (!std::isfinite(diameter))
		throw std::invalid_argument("Frustum diameter must be finite");
	const auto clamped = glm::max(diameter, .001f);

	// Fit the fixed target while keeping the projection type and focal point
	if (const auto *perspective = std::get_if<CameraParameters::Intrinsics::PerspectiveFov>(&params().intrinsics.fovY)) {
		const double halfAngle = .5 * glm::radians((double)perspective->angle);
		adjustTargetDistance(*this, .5 * clamped / std::tan(halfAngle), distance);
	} else
		setFovY(CameraParameters::Intrinsics::OrthoExtend{clamped});
}

void Camera::adjustForFrustumDiameterAtFocus (float diameter) {
	adjustForFrustumDiameterAtDistance(diameter, params().intrinsics.f);
}

void Camera::adjustForSphereAtDistance (float radius, float distance)
{
	// Validate all inputs before invoking any camera setters
	requireDistance(distance);
	if (!std::isfinite(radius))
		throw std::invalid_argument("Sphere radius must be finite");
	const auto clamped = glm::max(radius, .0005f);

	// Fit tangent planes rather than the sphere's central vertical cross section
	if (const auto *perspective = std::get_if<CameraParameters::Intrinsics::PerspectiveFov>(&params().intrinsics.fovY)) {
		const double halfAngle = .5 * glm::radians((double)perspective->angle);
		adjustTargetDistance(*this, clamped / std::sin(halfAngle), distance);
	} else
		setFovY(CameraParameters::Intrinsics::OrthoExtend{framingArgument(2. * clamped)});
}

void Camera::adjustForSphereAtFocus (float radius) {
	adjustForSphereAtDistance(radius, params().intrinsics.f);
}



//////
//
// Module namespace close
//

} // namespace fcg
