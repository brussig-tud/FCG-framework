
//////
//
// Includes
//

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/gtc/matrix_transform.hpp>

// Local includes
#include "FCG/player.h"
#include "FCG/applet/orbit_camera.h"

#include "FCG/render_state.h"


//////
//
// Module namespace open
//

/// Our module namespace.
namespace fcg::applet {



//////
//
// Class implementations
//

////
// Orbit Camera

OrbitCamera::OrbitCamera()
	: params(OrbitCameraParams{
		.intrinsics = {.fovY=45.f, .f=3.f, .zNear=.125f, .zFar=100.f},
		.extrinsics = {.eye={0, 0, 3}, .dir={0, 0, -1}, .up={0, 1, 0}}
	})
{}

OrbitCamera::~OrbitCamera() = default;

auto OrbitCamera::name () -> std::string& {
	static std::string name = "Orbit Camera";
	return name;
}

void OrbitCamera::init (Device &device, Player &player) {
	// Set the initial aspect ratio
	params.intrinsics.aspect = player.viewportSize().x / (float)player.viewportSize().y;
}

void OrbitCamera::onViewportResize (Device& device, const glm::uvec2& oldViewportSize, Player& player) {
	// Update the aspect ratio
	const auto &vp = player.viewportSize();
	params.intrinsics.aspect = vp.x / (float)vp.y;
	projMatrix.reset();
}

void OrbitCamera::gui (Device& device, Player& player) {
	// TODO: implement GUI for all camera parameters (except aspect ratio, which is automatically determined)
}

void OrbitCamera::update (Device& device, Player& player)
{
	if (!projMatrix.has_value()) {
		// Recompute the projection matrix
		const auto &vp = player.viewportSize();
		projMatrix = glm::perspectiveFov<float>(
			glm::radians(params.intrinsics.fovY), vp.x, vp.y, params.intrinsics.zNear,
			params.intrinsics.zFar
		);
	}
	if (!viewMatrix.has_value()) {
		// Recompute the view matrix
		viewMatrix = glm::lookAt(
			params.extrinsics.eye, params.extrinsics.eye + params.intrinsics.f*params.extrinsics.dir,
			params.extrinsics.up
		);
	}
}

void OrbitCamera::render (Device& device, RenderState& renderState, SDL_GPURenderPass* renderPass, Player& player) {
	// We assume to be the first to touch the stack, so no pushing or popping, we just replace the respective initial
	// matrices. Would need a beforeRender/afterRender pair of hooks to push/pop if we wanted to do it properly.
	renderState.loadModelviewMatrix(*viewMatrix);
	renderState.loadProjectionMatrix(*projMatrix);
}



//////
//
// Module namespace close
//

// Our module namespace
} // namespace applet::fcg
