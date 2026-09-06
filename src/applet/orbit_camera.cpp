
//////
//
// Includes
//

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/gtc/matrix_transform.hpp>

// Dear ImGui
#include <imgui.h>

// Local includes
#include "FCG/player.h"
#include "FCG/applet/orbit_camera.h"
#include "FCG/event.h"

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

void OrbitCamera::init (Device&, Player&) {}

void OrbitCamera::onViewportResize (Device& device, const glm::uvec2& oldViewportSize, Player& player) {
	// Update the aspect ratio
	projMatrix.reset();
}

void OrbitCamera::setFovY (float fovY) {
	params.intrinsics.fovY = glm::clamp(fovY, 1.f, 179.f);
	projMatrix.reset();
}

void OrbitCamera::setFocalLength (float f) {
	params.intrinsics.f = glm::max(f, 0.001f);
	viewMatrix.reset();
}

void OrbitCamera::setNearPlane (float zNear) {
	params.intrinsics.zNear = glm::max(zNear, 0.001f);
	params.intrinsics.zFar = glm::max(params.intrinsics.zFar, params.intrinsics.zNear + 0.001f);
	projMatrix.reset();
}

void OrbitCamera::setFarPlane (float zFar) {
	params.intrinsics.zFar = glm::max(zFar, params.intrinsics.zNear + 0.001f);
	projMatrix.reset();
}

void OrbitCamera::setEye (const glm::vec3& eye) {
	params.extrinsics.eye = eye;
	viewMatrix.reset();
}

void OrbitCamera::setDir (const glm::vec3& dir) {
	params.extrinsics.dir = glm::normalize(dir);
	viewMatrix.reset();
}

void OrbitCamera::setUp (const glm::vec3& up) {
	params.extrinsics.up = glm::normalize(up);
	viewMatrix.reset();
}

namespace {

// Build a rotation of `angle` radians around `axis` and apply it to a vector.
inline auto rotateAround (const glm::vec3& v, float angle, const glm::vec3& axis) -> glm::vec3 {
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.f - c);
}

} // namespace

void OrbitCamera::onEvent (const Event& event, EventContext& context, Player& player)
{
	constexpr float orbitSpeed = 0.01f;
	constexpr float panScale = 0.001f;
	constexpr float dollyScale = 0.005f;
	constexpr float zoomScale = 0.1f;
	constexpr float rollSpeed = 0.01f;

	if (event.type() == EventType::MouseButtonDown) {
		const auto* mouse = event.data<MouseButtonEvent>();
		if (!mouse) return;
		if (mouse->button == MouseButton::Left || mouse->button == MouseButton::Middle || mouse->button == MouseButton::Right) {
			activeDragButton = mouse->button;
			dragStartedWithShift = (event.raw().type == SDL_EVENT_MOUSE_BUTTON_DOWN)
				&& (SDL_GetModState() & SDL_KMOD_SHIFT);
			context.markHandled();
		}
		return;
	}

	if (event.type() == EventType::MouseButtonUp) {
		const auto* mouse = event.data<MouseButtonEvent>();
		if (mouse && mouse->button == activeDragButton) {
			activeDragButton = MouseButton::Unknown;
			dragStartedWithShift = false;
			context.markHandled();
		}
		return;
	}

	if (event.type() == EventType::MouseMotion) {
		const auto* motion = event.data<MouseMotionEvent>();
		if (!motion || activeDragButton == MouseButton::Unknown) return;

		const float dx = motion->relativeX;
		const float dy = motion->relativeY;
		const float f = params.intrinsics.f;
		const auto& dir = params.extrinsics.dir;
		const auto& up = params.extrinsics.up;
		const auto& eye = params.extrinsics.eye;

		if (activeDragButton == MouseButton::Left) {
			if (dragStartedWithShift) {
				// Roll the up vector around the viewing direction.
				const float angle = -dx * rollSpeed;
				setUp(rotateAround(up, angle, dir));
			}
			else {
				// Orbit around the focal point.
				const glm::vec3 focus = focalPoint();
				glm::vec3 right = glm::normalize(glm::cross(dir, up));

				// Clamp pitch to avoid flipping past the poles.
				const float pitchAngle = glm::clamp(-dy * orbitSpeed, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
				const float yawAngle = -dx * orbitSpeed;

				glm::vec3 offset = eye - focus;
				offset = rotateAround(offset, yawAngle, up);
				offset = rotateAround(offset, pitchAngle, right);
				setEye(focus + offset);

				setDir(rotateAround(dir, yawAngle, up));
				setUp(rotateAround(up, pitchAngle, right));
			}
		}
		else if (activeDragButton == MouseButton::Right) {
			// Pan in the camera plane.
			const glm::vec3 right = glm::normalize(glm::cross(dir, up));
			const glm::vec3 translation = (-dx * right + dy * up) * f * speedFactor * panScale;
			setEye(eye + translation);
		}
		else if (activeDragButton == MouseButton::Middle) {
			// Dolly forward/backward along the viewing direction.
			const glm::vec3 translation = dir * (-dy) * f * speedFactor * dollyScale;
			setEye(eye + translation);
		}

		context.markHandled();
		return;
	}

	if (event.type() == EventType::MouseWheel) {
		const auto* wheel = event.data<MouseWheelEvent>();
		if (!wheel) return;

		const float f = params.intrinsics.f;
		const glm::vec3 translation = params.extrinsics.dir * wheel->y * f * speedFactor * zoomScale;
		setEye(params.extrinsics.eye + translation);
		context.markHandled();
		return;
	}
}

void OrbitCamera::gui (Device& device, Player& player) {
	ImGui::SetNextWindowSize({0, 0}, ImGuiCond_FirstUseEver);
	ImGui::Begin("Orbit Camera");

	auto& in = params.intrinsics;
	auto& ex = params.extrinsics;

	if (ImGui::CollapsingHeader("Intrinsics", ImGuiTreeNodeFlags_DefaultOpen)) {
		float fovY = in.fovY;
		if (ImGui::SliderFloat("FoV Y (deg)", &fovY, 1.0f, 179.0f, "%.1f")) {
			setFovY(fovY);
		}

		float f = in.f;
		if (ImGui::DragFloat("Focal length##focal", &f, 0.01f, 0.001f, 10000.0f, "%.3f")) {
			setFocalLength(f);
		}

		float zNear = in.zNear;
		float zFar = in.zFar;
		if (ImGui::DragFloat("Near plane", &zNear, 0.01f, 0.001f, zFar - 0.001f, "%.3f")) {
			setNearPlane(zNear);
		}
		if (ImGui::DragFloat("Far plane", &zFar, 0.1f, zNear + 0.001f, 100000.0f, "%.1f")) {
			setFarPlane(zFar);
		}
	}

	if (ImGui::CollapsingHeader("Extrinsics", ImGuiTreeNodeFlags_DefaultOpen)) {
		glm::vec3 eye = ex.eye;
		glm::vec3 d = ex.dir;
		glm::vec3 u = ex.up;
		if (ImGui::DragFloat3("Eye", &eye.x, 0.01f)) {
			setEye(eye);
		}
		if (ImGui::DragFloat3("Direction", &d.x, 0.01f)) {
			setDir(d);
		}
		if (ImGui::DragFloat3("Up", &u.x, 0.01f)) {
			setUp(u);
		}

		const glm::vec3 focus = focalPoint();
		ImGui::Text("Focal point: %.3f, %.3f, %.3f", focus.x, focus.y, focus.z);
	}

	if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Speed factor", &speedFactor, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::Text("LMB drag: orbit, Shift+LMB drag: roll");
		ImGui::Text("RMB drag: pan, Wheel/MMB drag: dolly");
	}

	ImGui::End();
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

void OrbitCamera::render (
	Device& device, RenderState& renderState, SDL_GPURenderPass* renderPass,
	SDL_GPUCommandBuffer* /*commandBuffer*/, Player& player
) {
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
