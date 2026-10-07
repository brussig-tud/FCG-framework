
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <stdexcept>

// Local includes
#include "FCG/viewing.h"



//////
//
// Module namespace open
//

namespace fcg {



//////
//
// Class implementations
//

////
// Camera

Camera::~Camera() = default;

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
