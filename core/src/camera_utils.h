
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

namespace fcg::detail {



//////
//
// Functions
//

inline void requireFinite (float value) {
	if (!std::isfinite(value))
		throw std::invalid_argument("Camera input must be finite");
}

inline void requireFinite (const glm::vec3 &value) {
	requireFinite(value.x);
	requireFinite(value.y);
	requireFinite(value.z);
}

[[nodiscard]] inline auto normalized (const glm::vec3 &value) -> glm::vec3
{
	requireFinite(value);
	const float scale = glm::max(glm::max(glm::abs(value.x), glm::abs(value.y)), glm::abs(value.z));
	if (scale == 0.f)
		throw std::invalid_argument("Camera axis must be nonzero");
	return glm::normalize(value / scale);
}

inline void requirePose (const glm::vec3 &dir, const glm::vec3 &up) {
	if (glm::length(glm::cross(dir, up)) < 1e-5f)
		throw std::invalid_argument("Camera axes must be nonparallel");
}

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

[[nodiscard]] inline auto minimumFar (float zNear) -> float {
	const float result = glm::max(zNear + .001f, std::nextafter(zNear, std::numeric_limits<float>::infinity()));
	requireFinite(result);
	return result;
}

/// Logical units corresponding to the event's window; fallback permits windowless event probes.
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

} // namespace fcg::detail


#endif // ifndef __FCG_CAMERA_UTILS_H__
