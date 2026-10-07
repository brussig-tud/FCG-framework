
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <vector>

// FCG Framework
#include <FCG/applet/camera_2d.h>
#include <FCG/applet/orbit_camera.h>
#include <FCG/viewing.h>



//////
//
// Functions
//

/// [shared camera]
void configureCamera (fcg::Applet &applet)
{
	if (auto *camera = dynamic_cast<fcg::Camera*>(&applet)) {
		camera->setNearPlane(.1f);
		camera->setFarPlane(100.f);
		camera->setFrustumDiameterAtFocus(6.f);
		const glm::vec3 focus = camera->focalPoint();
		camera->translateToFocalPoint({2, 1, focus.z});
	}
}
/// [shared camera]

/// [orbit camera]
[[nodiscard]] auto orbitApplets () -> std::vector<std::unique_ptr<fcg::Applet>> {
	auto orbit = std::make_unique<fcg::applet::OrbitCamera>();
	orbit->setFovY(60.f);
	configureCamera(*orbit);
	std::vector<std::unique_ptr<fcg::Applet>> applets;
	applets.push_back(std::move(orbit)); // Add scene applets after the camera.
	return applets;
}
/// [orbit camera]

/// [planar camera]
[[nodiscard]] auto planarApplets () -> std::vector<std::unique_ptr<fcg::Applet>> {
	auto planar = std::make_unique<fcg::applet::Camera2D>();
	planar->setViewHeight(8.f);
	planar->setRotation(30.f);
	planar->setFocalPoint(glm::vec2(2, 1)); // Preserves focus depth and zoom.
	fcg::Camera &camera = *planar;
	camera.resetRotation();
	std::vector<std::unique_ptr<fcg::Applet>> applets;
	applets.push_back(std::move(planar));
	return applets;
}
/// [planar camera]
