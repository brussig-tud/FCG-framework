
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui
#include <imgui.h>

// Local includes
#include "FCG/applet/orbit_camera.h"
#include "FCG/player.h"
#include "FCG/camera_focus.h"
#include "../camera_utils.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

[[nodiscard]] auto rotateAround (const glm::vec3 &v, float angle, const glm::vec3 &axis) -> glm::vec3 {
	const float c = std::cos(angle), s = std::sin(angle);
	return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.f - c);
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

namespace fcg::applet {



//////
//
// Class implementations
//

////
// OrbitCamera

OrbitCamera::OrbitCamera()
	: m_params{
		.intrinsics = {.fovY=CameraParameters::Intrinsics::PerspectiveFov{60}, .f=3, .zNear=.125f, .zFar=100},
		.extrinsics = {.eye={0, 0, 3}, .dir={0, 0, -1}, .up={0, 1, 0}}
	}, focusChange(std::make_unique<CameraFocus>(*this))
{}

OrbitCamera::~OrbitCamera() = default;

auto OrbitCamera::name () const -> const std::string& {
	const static std::string name = "Orbit Camera";
	return name;
}

void OrbitCamera::init (Device&, Player&) {}

void OrbitCamera::onViewportResize (Device&, const glm::uvec2&, Player&) {
	projMatrix.reset();
	focusChange->resized();
}

void OrbitCamera::setFovY (const CameraParameters::Intrinsics::FovY &fovY) {
	const auto *perspective = std::get_if<CameraParameters::Intrinsics::PerspectiveFov>(&fovY);
	if (!perspective)
		throw std::invalid_argument("OrbitCamera requires perspective projection");
	setFovY(perspective->angle);
}

void OrbitCamera::setFovY (float fovY) {
	detail::requireFinite(fovY);
	focusChange->cancel();
	m_params.intrinsics.fovY = CameraParameters::Intrinsics::PerspectiveFov{glm::clamp(fovY, 1.f, 179.f)};
	projMatrix.reset();
}

void OrbitCamera::setFocalLength (float f) {
	detail::requireFinite(f);
	f = glm::max(f, .001f);
	detail::requireEye(m_params.extrinsics.eye, m_params.extrinsics.dir, m_params.extrinsics.up, f);
	focusChange->cancel();
	m_params.intrinsics.f = f;
}

void OrbitCamera::setNearPlane (float zNear)
{
	detail::requireFinite(zNear);
	zNear = glm::max(zNear, .001f);
	const float zFar = glm::max(m_params.intrinsics.zFar, detail::minimumFar(zNear));
	focusChange->cancel();
	m_params.intrinsics.zNear = zNear;
	m_params.intrinsics.zFar = zFar;
	projMatrix.reset();
}

void OrbitCamera::setFarPlane (float zFar) {
	detail::requireFinite(zFar);
	focusChange->cancel();
	m_params.intrinsics.zFar = glm::max(zFar, detail::minimumFar(m_params.intrinsics.zNear));
	projMatrix.reset();
}

void OrbitCamera::setEye (const glm::vec3 &eye) {
	detail::requireEye(eye, m_params.extrinsics.dir, m_params.extrinsics.up, m_params.intrinsics.f);
	focusChange->cancel();
	m_params.extrinsics.eye = eye;
	viewMatrix.reset();
}

void OrbitCamera::setDir (const glm::vec3 &dir)
{
	const auto direction = detail::normalized(dir);
	detail::requirePose(direction, m_params.extrinsics.up);
	detail::requireEye(m_params.extrinsics.eye, direction, m_params.extrinsics.up, m_params.intrinsics.f);
	focusChange->cancel();
	m_params.extrinsics.dir = direction;
	viewMatrix.reset();
}

void OrbitCamera::setUp (const glm::vec3 &up)
{
	const auto upwards = detail::normalized(up);
	detail::requirePose(m_params.extrinsics.dir, upwards);
	detail::requireEye(m_params.extrinsics.eye, m_params.extrinsics.dir, upwards, m_params.intrinsics.f);
	focusChange->cancel();
	m_params.extrinsics.up = upwards;
	viewMatrix.reset();
}

void OrbitCamera::setFocalPoint (const glm::vec3 &focalPoint)
{
	detail::requireFinite(focalPoint);
	const auto v = glm::dvec3(focalPoint) - glm::dvec3(m_params.extrinsics.eye);
	const float f = (float)glm::length(v);
	detail::requireFinite(f);
	if (f < .001f)
		throw std::invalid_argument("OrbitCamera focus must differ from the eye");
	const auto dir = glm::vec3(v / (double)f);
	auto right = glm::cross(dir, m_params.extrinsics.up);
	if (glm::length(right) < .0001f) {
		const auto fallback = std::abs(dir.y) < .99f ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1);
		right = glm::cross(dir, fallback);
	}
	const auto up = glm::normalize(glm::cross(glm::normalize(right), dir));
	detail::requireEye(m_params.extrinsics.eye, glm::normalize(dir), up, f);
	focusChange->cancel();
	m_params.intrinsics.f = f;
	m_params.extrinsics.dir = glm::normalize(dir);
	m_params.extrinsics.up = up;
	viewMatrix.reset();
}

void OrbitCamera::translateToFocalPoint (const glm::vec3 &focalPoint) {
	detail::requireFinite(focalPoint);
	const auto eye = focalPoint - m_params.intrinsics.f * m_params.extrinsics.dir;
	setEye(eye);
}

void OrbitCamera::resetRotation ()
{
	const auto focus = focalPoint();
	const auto eye = focus + glm::vec3(0, 0, m_params.intrinsics.f);
	detail::requireEye(eye, {0, 0, -1}, {0, 1, 0}, m_params.intrinsics.f);
	focusChange->cancel();
	m_params.extrinsics = {.eye=eye, .dir={0, 0, -1}, .up={0, 1, 0}};
	viewMatrix.reset();
}

void OrbitCamera::resetFocus () {
	setFocalPoint(glm::vec3(0));
}

void OrbitCamera::onEvent (const Event &event, EventContext &context, Player &player)
{
	if (event.type() == EventType::MouseButtonDown)
	{
		const auto *mouse = event.data<MouseButtonEvent>();
		if (!mouse)
			return;
		const bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
		if (mouse->button == MouseButton::Left && !shift)
		{
			const auto now = std::chrono::steady_clock::now();
			const bool doubleClick = mouse->clicks == 2
				|| (lastLeftClickTime && now - *lastLeftClickTime < std::chrono::milliseconds(250));
			if (doubleClick) {
				if (focusChange->pick(event, player))
					context.markHandled();
				lastLeftClickTime.reset();
			}
			else
				lastLeftClickTime = now;
		}
		else
			lastLeftClickTime.reset();
		if (mouse->button == MouseButton::Left || mouse->button == MouseButton::Middle || mouse->button == MouseButton::Right) {
			activeDragButton = mouse->button;
			dragStartedWithShift = shift;
			context.markHandled();
		}
		return;
	}
	if (event.type() == EventType::MouseButtonUp)
	{
		const auto *mouse = event.data<MouseButtonEvent>();
		if (mouse && mouse->button == activeDragButton) {
			activeDragButton = MouseButton::Unknown;
			dragStartedWithShift = false;
			context.markHandled();
		}
		return;
	}
	if (event.type() == EventType::MouseMotion)
	{
		const auto *motion = event.data<MouseMotionEvent>();
		if (!motion || activeDragButton == MouseButton::Unknown
			|| !std::isfinite(motion->relativeX) || !std::isfinite(motion->relativeY))
			return;
		const float dx = motion->relativeX, dy = motion->relativeY;
		const float f = m_params.intrinsics.f;
		const auto dir = m_params.extrinsics.dir, up = m_params.extrinsics.up;
		if (activeDragButton == MouseButton::Left)
		{
			if (dragStartedWithShift)
				setUp(rotateAround(up, -dx * .01f, dir));
			else
			{
				const auto focus = focalPoint();
				auto right = glm::normalize(glm::cross(dir, up));
				right = rotateAround(right, -dx * .01f, up);
				const auto newUp = glm::normalize(rotateAround(up, -dy * .01f, right));
				const auto newDir = glm::normalize(glm::cross(newUp, right));
				focusChange->cancel();
				m_params.extrinsics = {.eye=focus - f * newDir, .dir=newDir, .up=newUp};
				viewMatrix.reset();
			}
		}
		else if (activeDragButton == MouseButton::Right)
			setEye(m_params.extrinsics.eye + (-dx * glm::normalize(glm::cross(dir, up)) + dy * up) * f * speedFactor * .001f);
		else if (activeDragButton == MouseButton::Middle)
			setEye(m_params.extrinsics.eye - dy * dir * f * speedFactor * .005f);
		context.markHandled();
		return;
	}
	if (event.type() == EventType::MouseWheel)
	{
		const auto *wheel = event.data<MouseWheelEvent>();
		if (!wheel || !std::isfinite(wheel->y) || wheel->y == 0.f)
			return;
		const auto focus = focalPoint();
		const float f = glm::max(.001f, m_params.intrinsics.f * (float)std::pow(1. + .125 * speedFactor, -wheel->y));
		const auto eye = focus - f * m_params.extrinsics.dir;
		detail::requireFinite(f);
		detail::requireFinite(eye);
		setFocalLength(f);
		setEye(eye);
		context.markHandled();
	}
}

void OrbitCamera::gui (Device&, Player&)
{
	ImGui::SetNextWindowSize({0, 0}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
	ImGui::Begin("Orbit Camera");
	ImGui::PushItemWidth(10.f * ImGui::GetFontSize());
	const auto &in = m_params.intrinsics;
	const auto &ex = m_params.extrinsics;
	if (ImGui::CollapsingHeader("Intrinsics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		float fovY = std::get<CameraParameters::Intrinsics::PerspectiveFov>(in.fovY).angle;
		if (ImGui::SliderFloat("FoV Y (deg)", &fovY, 1.f, 179.f, "%.1f"))
			setFovY(fovY);
		float f = in.f;
		if (ImGui::DragFloat("Focal length##focal", &f, .01f, .001f, 10000.f, "%.3f"))
			setFocalLength(f);
		float zNear = in.zNear, zFar = in.zFar;
		if (ImGui::DragFloat("Near plane", &zNear, .01f, .001f, zFar - .001f, "%.3f"))
			setNearPlane(zNear);
		if (ImGui::DragFloat("Far plane", &zFar, .1f, zNear + .001f, 100000.f, "%.1f"))
			setFarPlane(zFar);
	}
	if (ImGui::CollapsingHeader("Extrinsics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		glm::vec3 eye = ex.eye, dir = ex.dir, up = ex.up;
		if (ImGui::DragFloat3("Eye", &eye.x, .01f))
			setEye(eye);
		try {
			if (ImGui::DragFloat3("Direction", &dir.x, .01f))
				setDir(dir);
			if (ImGui::DragFloat3("Up", &up.x, .01f))
				setUp(up);
		}
		catch (const std::invalid_argument&) {
			// Interactive axis editing can temporarily request a degenerate pose.
		}
		if (ImGui::Button("Reset rotation"))
			resetRotation();
	}
	if (ImGui::CollapsingHeader("Combined", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto focus = focalPoint();
		try {
			if (ImGui::DragFloat3("Focus", &focus.x, .01f))
				setFocalPoint(focus);
			if (ImGui::Button("Reset focus"))
				resetFocus();
		}
		catch (const std::invalid_argument&) {
			// Keep the existing pose when the requested focus coincides with the eye.
		}
	}
	if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Speed factor", &speedFactor, .01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::TextUnformatted("LMB drag: orbit, Shift+LMB drag: roll");
		ImGui::TextUnformatted("RMB drag: pan, Wheel/MMB drag: dolly");
		ImGui::TextUnformatted("Double-click an object to move focus");
	}
	ImGui::PopItemWidth();
	ImGui::End();
}

void OrbitCamera::update (Device&, Player &player, float dt)
{
	focusChange->update(player, dt);
	if (!viewMatrix)
		viewMatrix = m_params.extrinsics.modelviewMatrix();
	const auto viewport = player.viewportSize();
	if (!projMatrix && viewport.x && viewport.y)
		projMatrix = m_params.intrinsics.projectionMatrix(viewport);
}

void OrbitCamera::render (
	Device&, RenderState &renderState, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, Player &player
)
{
	const auto viewport = player.viewportSize();
	if (!viewport.x || !viewport.y)
		return;
	if (!viewMatrix)
		viewMatrix = m_params.extrinsics.modelviewMatrix();
	if (!projMatrix)
		projMatrix = m_params.intrinsics.projectionMatrix(viewport);
	renderState.loadModelviewMatrix(*viewMatrix);
	renderState.loadProjectionMatrix(*projMatrix);
	focusChange->rendered(*viewMatrix, *projMatrix, viewport);
}



//////
//
// Module namespace close
//

} // namespace fcg::applet
