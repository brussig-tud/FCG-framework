
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
#include <FCG/player.h>



//////
//
// Functions
//

/// [shared camera]
void configureCamera (fcg::Applet &applet)
{
	if (auto *camera = dynamic_cast<fcg::Camera*>(&applet)) {
		camera->setDir({0, 0, -1});
		camera->setUp({0, 1, 1}); // Projects onto the viewing plane and normalizes.
		camera->setNearPlane(.1f);
		camera->setFarPlane(100.f);
		camera->setFrustumDiameterAtFocus(6.f);
		const glm::vec3 focus = camera->focalPoint();
		camera->translateToFocalPoint({2, 1, focus.z});
	}
}
/// [shared camera]

/// [quarter turn]
void pitchCameraUp (fcg::applet::OrbitCamera &camera) {
	const auto axes = camera.params().extrinsics;
	camera.setOrientation(axes.up, -axes.dir);
}
/// [quarter turn]

/// [orbit camera]
[[nodiscard]] auto orbitApplets () -> std::vector<std::unique_ptr<fcg::Applet>> {
	auto orbit = std::make_unique<fcg::applet::OrbitCamera>();
	orbit->setFovY(60.f);
	configureCamera(*orbit);
	std::vector<std::unique_ptr<fcg::Applet>> applets;
	applets.push_back(std::move(orbit)); // Combine with scene applets in the same run.
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

/// Enumerate cameras and queue selections from an applet callback.
/// [select cameras]
void selectCameras (fcg::Player &player)
{
	for (auto *camera : player.cameras())
		camera->setNearPlane(.1f);

	if (const auto camera = player.findCamera("2D Camera"))
		player.setActiveCamera(*camera);
	if (player.findCamera("Orbit Camera"))
		player.setActiveCamera("Orbit Camera"); // Latest selection wins.

	if (const auto camera = player.activeCamera())
		(*camera)->setFarPlane(100.f); // Queries still describe the effective camera.
	player.setActiveCamera(nullptr); // Run without an active camera on the next iteration.
}
/// [select cameras]
