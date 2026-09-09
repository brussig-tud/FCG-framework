
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
	: m_params(OrbitCameraParams{
		.intrinsics = {.fovY=60, .f=3, .zNear=.125f, .zFar=100},
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
	// Potentially updated aspect ratio will require matrix recomputation
	projMatrix.reset();
}

void OrbitCamera::setFovY (float fovY) {
	m_params.intrinsics.fovY = glm::clamp(fovY, 1.f, 179.f);
	projMatrix.reset();
}

void OrbitCamera::setFocalLength (float f) {
	m_params.intrinsics.f = glm::max(f, 0.001f);
	m_focalPoint.reset();
	viewMatrix.reset();
}

void OrbitCamera::setNearPlane (float zNear) {
	m_params.intrinsics.zNear = glm::max(zNear, 0.001f);
	m_params.intrinsics.zFar = glm::max(m_params.intrinsics.zFar, m_params.intrinsics.zNear + 0.001f);
	projMatrix.reset();
}

void OrbitCamera::setFarPlane (float zFar) {
	m_params.intrinsics.zFar = glm::max(zFar, m_params.intrinsics.zNear + 0.001f);
	projMatrix.reset();
}

void OrbitCamera::setEye (const glm::vec3& eye) {
	m_params.extrinsics.eye = eye;
	m_focalPoint.reset();
	viewMatrix.reset();
}

void OrbitCamera::setDir (const glm::vec3& dir) {
	m_params.extrinsics.dir = glm::normalize(dir);
	m_focalPoint.reset();
	viewMatrix.reset();
}

void OrbitCamera::setUp (const glm::vec3& up) {
	m_params.extrinsics.up = glm::normalize(up);
	viewMatrix.reset();
}

void OrbitCamera::setFocalPoint (const glm::vec3& focalPoint)
{
	// Bail out on degenerate case
	const glm::vec3 v = focalPoint - m_params.extrinsics.eye;
	const float f = glm::length(v);
	if (f < 0.001f)
		return;

	// Update the camera parameters
	m_params.intrinsics.f = f;
	m_params.extrinsics.dir = v / f;
	glm::vec3 right = glm::cross(m_params.extrinsics.dir, m_params.extrinsics.up);
	if (glm::length(right) < 0.0001f) {
		const glm::vec3 fallback = std::abs(m_params.extrinsics.dir.y) < 0.99f ?
			glm::vec3(0,1,0) : glm::vec3(0,0,1);
		right = glm::cross(m_params.extrinsics.dir, fallback);
	}
	m_params.extrinsics.up = glm::cross(glm::normalize(right), m_params.extrinsics.dir);

	// Invalidate caches
	m_focalPoint.reset();
	viewMatrix.reset();
	projMatrix.reset();
}

void OrbitCamera::translateToFocalPoint (const glm::vec3& focalPoint) {
	const glm::vec3 v = focalPoint - this->focalPoint();
	m_params.extrinsics.eye += v;
	viewMatrix.reset();
	m_focalPoint.reset();
}

namespace {

// Build a rotation of `angle` radians around `axis` and apply it to a vector.
inline auto rotateAround (const glm::vec3& v, float angle, const glm::vec3& axis) -> glm::vec3 {
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.f - c);
}

// Duration of the smooth focus-point transition, in seconds.
constexpr float focusAnimDuration = 0.5f;

} // namespace

void OrbitCamera::onEvent (const Event& event, EventContext& context, Player& player)
{
	constexpr float orbitSpeed = .01f;
	constexpr float panScale = .001f;
	constexpr float dollyScale = .005f;
	constexpr float zoomFraction = 0.125f;
	constexpr float rollSpeed = .01f;

	if (event.type() == EventType::MouseButtonDown)
	{
		const auto* mouse = event.data<MouseButtonEvent>();
		if (!mouse)
			return;
		if (mouse->button == MouseButton::Left && !lastLeftClickTime) {
			lastLeftClickTime = std::chrono::steady_clock::now();
		}
		else if (mouse->button == MouseButton::Left && lastLeftClickTime) {
			const auto now = std::chrono::steady_clock::now();
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				now - *lastLeftClickTime
			);
			if (elapsed.count() < 250 && std::holds_alternative<std::monostate>(focusChange_old)) {
				focusChange_old = PendingReadbackInfo{
					.token=player.scheduleDepthReadback(), .clickPos=glm::vec2(mouse->x, mouse->y)
				};
				context.markHandled();
				lastLeftClickTime.reset();
			}
			else
				lastLeftClickTime = std::chrono::steady_clock::now();
		}
		else {
			lastLeftClickTime.reset();
		}
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

	if (event.type() == EventType::MouseMotion)
	{
		auto handleTranslation = [this](const glm::vec3& translation) {
			m_params.extrinsics.eye += translation;
			m_focalPoint.reset();
			viewMatrix.reset();
		};
		const auto* motion = event.data<MouseMotionEvent>();
		if (!motion || activeDragButton == MouseButton::Unknown)
			return;

		const float dx = motion->relativeX;
		const float dy = motion->relativeY;
		const float f = m_params.intrinsics.f;
		const auto& dir = m_params.extrinsics.dir;
		const auto& up = m_params.extrinsics.up;
		const auto& eye = m_params.extrinsics.eye;

		if (activeDragButton == MouseButton::Left)
		{
			if (dragStartedWithShift) {
				// Roll the up vector around the viewing direction.
				const float angle = -dx * rollSpeed;
				setUp(rotateAround(up, angle, dir));
			}
			else
			{
				// Orbit around the focal point.
				const auto &focus = focalPoint();
				auto right = glm::normalize(
					glm::cross(m_params.extrinsics.dir, m_params.extrinsics.up)
				);
				right = rotateAround(right, -dx*orbitSpeed, m_params.extrinsics.up);
				m_params.extrinsics.up = rotateAround(m_params.extrinsics.up, dy*-orbitSpeed, right);
				m_params.extrinsics.dir = glm::cross(m_params.extrinsics.up, right);
				m_params.extrinsics.eye = focus - m_params.intrinsics.f*m_params.extrinsics.dir;
				viewMatrix.reset();
			}
			context.markHandled();
		}
		else if (activeDragButton == MouseButton::Right) {
			// Pan in the camera plane.
			const glm::vec3 right = glm::normalize(glm::cross(dir, up));;
			handleTranslation((-dx*right + dy*up) * f*speedFactor*panScale);
			context.markHandled();
		}
		else if (activeDragButton == MouseButton::Middle) {
			// Dolly forward/backward along the viewing direction.
			handleTranslation(-dy*dir * f*speedFactor*dollyScale);
			context.markHandled();
		}
		return;
	}

	if (event.type() == EventType::MouseWheel)
	{
		const auto* wheel = event.data<MouseWheelEvent>();
		if (!wheel)
			return;
		const auto effectiveZoomFraction = zoomFraction*speedFactor;
		const auto zoomRatio = wheel->y < 0 ? 1+effectiveZoomFraction : 1/(1+effectiveZoomFraction);
		const auto effectiveZoomRatio = std::pow(zoomRatio, std::abs(wheel->y));
		m_params.intrinsics.f *= effectiveZoomRatio;
		m_params.extrinsics.eye = focalPoint() - m_params.intrinsics.f*m_params.extrinsics.dir;
		viewMatrix.reset();
		projMatrix.reset();
		context.markHandled();
		return;
	}
}

void OrbitCamera::gui (Device& device, Player& player)
{
	ImGui::SetNextWindowSize({0, 0}, ImGuiCond_FirstUseEver);
	ImGui::Begin("Orbit Camera");

	auto& in = m_params.intrinsics;
	auto& ex = m_params.extrinsics;

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
	}

	if (ImGui::CollapsingHeader("Combined", ImGuiTreeNodeFlags_DefaultOpen)) {
		glm::vec3 focus = focalPoint();
		if (ImGui::DragFloat3("focus", &focus.x, 0.01f)) {
			setFocalPoint(focus);
		}
		if (ImGui::Button("Reset focus")) {
			setFocalPoint(glm::vec3(0));
		}
	}

	if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Speed factor", &speedFactor, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::Text("LMB drag: orbit, Shift+LMB drag: roll");
		ImGui::Text("RMB drag: pan, Wheel/MMB drag: dolly");
		ImGui::Text("double click object to move focus");
	}

	ImGui::End();
}

void OrbitCamera::update (Device& device, Player& player, float dt)
{
	if (std::holds_alternative<PendingReadbackInfo>(focusChange_old))
	{
		const auto &rbInfo = std::get<PendingReadbackInfo>(focusChange_old);
		const auto texel = player.getDepthReadbackResult(rbInfo.token).texel(glm::uvec2(rbInfo.clickPos));
		if (texel < 1.f && projMatrix && viewMatrix)
		{
			const glm::vec4 ndc = glm::vec4(
				2*((float)rbInfo.clickPos.x/(float)player.viewportSize().x) - 1.f,
				1.f - 2*((float)rbInfo.clickPos.y/(float)player.viewportSize().y),
				texel, 1.f
			);
			glm::vec4 worldPos = glm::inverse(projMatrix.value()*viewMatrix.value()) * ndc; worldPos /= worldPos.w;
			SDL_Log(
				"OrbitCamera: depth readback at %u,%u: depth=%f -> world=(%f,%f,%f)",
				(unsigned)rbInfo.clickPos.x, (unsigned)rbInfo.clickPos.y, texel, worldPos.x, worldPos.y, worldPos.z
			);
			focusChange_old = worldPos / worldPos.w;
			player.pushContinuousRedraw();
		}
		else if (texel < 1.f) {
			// If we don't have valid matrices, we can't compute the world position, so we just discard the readback.
			SDL_LogWarn(
				SDL_LOG_CATEGORY_APPLICATION,
				"OrbitCamera: matrices are dirty, discarding focus change from depth readback"
			);
			focusChange_old = std::monostate{};
		}
		else {
			SDL_Log(
				"OrbitCamera: depth readback at %u,%u: depth=%f -> no fragment, discard",
				(unsigned)rbInfo.clickPos.x, (unsigned)rbInfo.clickPos.y, texel
			);
			focusChange_old = std::monostate{};
		}
	}
	if (std::holds_alternative<glm::vec3>(focusChange_old))
	{
		const auto &newFocus = std::get<glm::vec3>(focusChange_old);
		focusChange_old = FocusAnimation{.start=focalPoint(), .target=newFocus, .elapsed=0.f};
	}
	if (std::holds_alternative<FocusAnimation>(focusChange_old))
	{
		auto &anim = std::get<FocusAnimation>(focusChange_old);
		anim.elapsed += dt;
		const float t = glm::clamp(anim.elapsed / focusAnimDuration, 0.f, 1.f);
		const float e = glm::smoothstep(0.f, 1.f, t);
		translateToFocalPoint(glm::mix(anim.start, anim.target, e));
		if (t >= 1.f) {
			translateToFocalPoint(anim.target);
			focusChange_old = std::monostate{};
			player.popContinuousRedraw();
		}
	}
	if (!projMatrix.has_value()) {
		// Recompute the projection matrix
		const auto &vp = player.viewportSize();
		projMatrix = glm::perspectiveFov<float>(
			glm::radians(m_params.intrinsics.fovY), vp.x, vp.y, m_params.intrinsics.zNear,
			m_params.intrinsics.zFar
		);
	}
	if (!viewMatrix.has_value()) {
		// Recompute the view matrix
		viewMatrix = glm::lookAt(
			m_params.extrinsics.eye, m_params.extrinsics.eye + m_params.intrinsics.f*m_params.extrinsics.dir,
			m_params.extrinsics.up
		);
	}
}

void OrbitCamera::render (
	Device&, RenderState &renderState, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, Player&
){
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
