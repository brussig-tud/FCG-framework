
#ifndef __FCG_CAMERA_UTILS_H__
#define __FCG_CAMERA_UTILS_H__


//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <limits>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/event.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// The library's namespace for internal implementation details shared between translation units.
namespace detail {



//////
//
// Functions
//

/// \brief Validate a scalar camera input.
///
/// \param value
/// 	The value to check.
///
/// \throws std::invalid_argument
/// 	If \c value is infinite or NaN.
inline void requireFinite (float value) {
	if (!std::isfinite(value))
		throw std::invalid_argument("Camera input must be finite");
}

/// \brief Validate a vector camera input component-wise.
///
/// \param value
/// 	The vector to check.
///
/// \throws std::invalid_argument
/// 	If any component of \c value is infinite or NaN.
inline void requireFinite (const glm::vec3 &value) {
	requireFinite(value.x);
	requireFinite(value.y);
	requireFinite(value.z);
}

/// \brief Normalize a camera axis robustly.
///
/// The vector is divided by its largest absolute component before normalizing, so tiny and huge vectors whose
/// squared length would underflow or overflow are still handled.
///
/// \param value
/// 	The axis to normalize.
///
/// \return The unit-length vector pointing in the direction of \c value.
///
/// \throws std::invalid_argument
/// 	If \c value is not finite or is the zero vector.
[[nodiscard]] inline auto normalized (const glm::vec3 &value) -> glm::vec3
{
	requireFinite(value);
	const float scale = glm::max(glm::max(glm::abs(value.x), glm::abs(value.y)), glm::abs(value.z));
	if (scale == 0.f)
		throw std::invalid_argument("Camera axis must be nonzero");
	return glm::normalize(value / scale);
}

/// \brief Validate that a viewing direction and up vector form a usable camera orientation.
///
/// \param dir
/// 	The normalized viewing direction.
/// \param up
/// 	The normalized up vector.
///
/// \throws std::invalid_argument
/// 	If the length of <tt>cross(dir, up)</tt> is below <tt>1e-5</tt>, i.e. the axes are (nearly) parallel.
inline void requirePose (const glm::vec3 &dir, const glm::vec3 &up) {
	if (glm::length(glm::cross(dir, up)) < 1e-5f)
		throw std::invalid_argument("Camera axes must be nonparallel");
}

/// \brief Validate a complete camera pose in single-precision arithmetic.
///
/// Checks that the eye, the look-at target <tt>eye + dir</tt> and the focal point <tt>eye + f * dir</tt> are finite,
/// that the target differs from the eye (\c dir is not absorbed by rounding at the eye's magnitude), and that the
/// direction actually representable as <tt>target - eye</tt> is not parallel to \c up. These are the conditions
/// under which \c CameraParameters::Extrinsics::modelviewMatrix and \c Camera::focalPoint produce valid results.
///
/// \param eye
/// 	The eye position.
/// \param dir
/// 	The normalized viewing direction.
/// \param up
/// 	The normalized up vector.
/// \param f
/// 	The focus distance.
///
/// \throws std::invalid_argument
/// 	If any of the conditions above is violated.
inline void requireEye (const glm::vec3 &eye, const glm::vec3 &dir, const glm::vec3 &up, float f)
{
	requireFinite(eye);
	const auto target = eye + dir;
	requireFinite(target);
	requireFinite(eye + f * dir);
	if (target == eye)
		throw std::invalid_argument("Camera eye cannot represent its viewing direction");
	requirePose(normalized(target - eye), up);
}

/// \brief Compute the smallest far clipping distance allowed for the given near clipping distance.
///
/// \param zNear
/// 	The near clipping distance.
///
/// \return
/// 	<tt>zNear + 0.001</tt>, or the next representable float above \c zNear if that addition is lost to
/// 	rounding.
///
/// \throws std::invalid_argument
/// 	If the result is not finite.
[[nodiscard]] inline auto minimumFar (float zNear) -> float {
	const float result = glm::max(zNear + .001f, std::nextafter(zNear, std::numeric_limits<float>::infinity()));
	requireFinite(result);
	return result;
}

/// \brief Determine the logical size of the window a mouse event refers to.
///
/// Mouse event coordinates are given in these logical window units, which can differ from drawable pixels on
/// high-DPI displays.
///
/// \param event
/// 	A mouse motion, wheel or button event.
/// \param fallback
/// 	The size to return if the event's window cannot be determined or has no valid size.
///
/// \return The logical window size, or \c fallback.
[[nodiscard]] inline auto windowSize (const Event &event, const glm::uvec2 &fallback) -> glm::vec2
{
	SDL_WindowID id = 0;
	if (event.type() == EventType::MouseMotion)
		id = event.raw().motion.windowID;
	else if (event.type() == EventType::MouseWheel)
		id = event.raw().wheel.windowID;
	else
		id = event.raw().button.windowID;
	int width = 0, height = 0;
	if (id)
		if (auto *window = SDL_GetWindowFromID(id))
			if (SDL_GetWindowSize(window, &width, &height) && width > 0 && height > 0)
				return {(float)width, (float)height};
	return glm::vec2(fallback);
}



//////
//
// Namespaces close
//

} // namespace detail

} // namespace fcg


#endif // ifndef __FCG_CAMERA_UTILS_H__
