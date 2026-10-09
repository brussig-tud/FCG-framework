
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui
#include <imgui.h>

// Local includes
#include "FCG/applet/camera_2d.h"
#include "FCG/player.h"
#include "FCG/camera_focus.h"
#include "../camera_utils.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

[[nodiscard]] auto wrappedAngle (float angle) -> float {
	return (float)(std::fmod(std::fmod((double)angle + 180., 360.) + 360., 360.) - 180.);
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
// Camera2D

Camera2D::Camera2D()
	: m_params{
		.intrinsics = {.fovY=CameraParameters::Intrinsics::OrthoExtend{4}, .f=3, .zNear=.125f, .zFar=100},
		.extrinsics = {.eye={0, 0, 3}, .dir={0, 0, -1}, .up={0, 1, 0}}
	}, focusChange(std::make_unique<CameraFocus>(*this, true))
{}

Camera2D::~Camera2D() = default;

auto Camera2D::name () const -> const std::string& {
	const static std::string name = "2D Camera";
	return name;
}

void Camera2D::init (Device&, Player&) {}

void Camera2D::onViewportResize (Device&, const glm::uvec2&, Player&) {
	projMatrix.reset();
	focusChange->resized();
}

void Camera2D::setFovY (const CameraParameters::Intrinsics::FovY &fovY)
{
	const auto *ortho = std::get_if<CameraParameters::Intrinsics::OrthoExtend>(&fovY);
	if (!ortho)
		throw std::invalid_argument("Camera2D requires orthographic projection");
	detail::requireFinite(ortho->size);
	const float height = glm::clamp(ortho->size, .001f, 100000.f);
	focusChange->cancel();
	m_params.intrinsics.fovY = CameraParameters::Intrinsics::OrthoExtend{height};
	projMatrix.reset();
}

void Camera2D::setViewHeight (float height) {
	setFovY(CameraParameters::Intrinsics::OrthoExtend{height});
}

void Camera2D::setFocalLength (float f) {
	detail::requireFinite(f);
	f = glm::max(f, .001f);
	detail::requireEye(m_params.extrinsics.eye, m_params.extrinsics.dir, m_params.extrinsics.up, f);
	focusChange->cancel();
	m_params.intrinsics.f = f;
}

void Camera2D::setNearPlane (float zNear)
{
	detail::requireFinite(zNear);
	zNear = glm::max(zNear, .001f);
	const float zFar = glm::max(m_params.intrinsics.zFar, detail::minimumFar(zNear));
	focusChange->cancel();
	m_params.intrinsics.zNear = zNear;
	m_params.intrinsics.zFar = zFar;
	projMatrix.reset();
}

void Camera2D::setFarPlane (float zFar) {
	detail::requireFinite(zFar);
	focusChange->cancel();
	m_params.intrinsics.zFar = glm::max(zFar, detail::minimumFar(m_params.intrinsics.zNear));
	projMatrix.reset();
}

void Camera2D::setEye (const glm::vec3 &eye) {
	detail::requireEye(eye, m_params.extrinsics.dir, m_params.extrinsics.up, m_params.intrinsics.f);
	focusChange->cancel();
	m_params.extrinsics.eye = eye;
	viewMatrix.reset();
}

void Camera2D::setEyeZ (float z) {
	setEye({m_params.extrinsics.eye.x, m_params.extrinsics.eye.y, z});
}

void Camera2D::setDir (const glm::vec3 &dir)
{
	const auto direction = detail::normalized(dir);
	if (glm::length(direction - glm::vec3(0, 0, -1)) > 1e-5f)
		throw std::invalid_argument("Camera2D direction must be -Z");
	focusChange->cancel();
	m_params.extrinsics.dir = {0, 0, -1};
	viewMatrix.reset();
}

void Camera2D::setUp (const glm::vec3 &up)
{
	auto upwards = detail::normalized(up);
	if (std::abs(upwards.z) > 1e-5f)
		throw std::invalid_argument("Camera2D up must lie in XY");
	upwards.z = 0.f;
	upwards = glm::normalize(upwards);
	focusChange->cancel();
	m_params.extrinsics.up = upwards;
	viewMatrix.reset();
}

void Camera2D::setRotation (float angle) {
	detail::requireFinite(angle);
	const auto radians = glm::radians(wrappedAngle(angle));
	setUp({-std::sin(radians), std::cos(radians), 0});
}

auto Camera2D::rotation () const -> float {
	const auto &up = m_params.extrinsics.up;
	return wrappedAngle(glm::degrees(std::atan2(-up.x, up.y)));
}

void Camera2D::setFocalPoint (const glm::vec3 &focalPoint) {
	translateToFocalPoint(focalPoint);
}

void Camera2D::setFocalPoint (const glm::vec2 &focalPoint) {
	setFocalPoint(glm::vec3(focalPoint, this->focalPoint().z));
}

void Camera2D::translateToFocalPoint (const glm::vec3 &focalPoint) {
	detail::requireFinite(focalPoint);
	if (std::abs((double)focalPoint.z - (double)this->focalPoint().z) > 1e-5)
		throw std::invalid_argument("Camera2D focus movement must preserve depth");
	setEye({focalPoint.x, focalPoint.y, m_params.extrinsics.eye.z});
}

void Camera2D::resetRotation () {
	setUp({0, 1, 0});
}

void Camera2D::resetFocus () {
	setFocalPoint(glm::vec2(0));
}

void Camera2D::onEvent (const Event &event, EventContext &context, Player &player)
{
	if (event.type() == EventType::MouseButtonDown)
	{
		const auto *mouse = event.data<MouseButtonEvent>();
		if (!mouse)
			return;
		const auto modifiers = SDL_GetModState();
		const bool shift = (modifiers & SDL_KMOD_SHIFT) != 0;
		if (mouse->button == MouseButton::Left && !(modifiers & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)))
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
		if (mouse->button == MouseButton::Middle || mouse->button == MouseButton::Right
			|| (mouse->button == MouseButton::Left && shift)) {
			activeDragButton = mouse->button;
			context.markHandled();
		}
		return;
	}
	if (event.type() == EventType::MouseButtonUp)
	{
		const auto *mouse = event.data<MouseButtonEvent>();
		if (mouse && mouse->button == activeDragButton) {
			activeDragButton = MouseButton::Unknown;
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
		if (activeDragButton == MouseButton::Left)
			setRotation(rotation() + glm::degrees(.01f * dx));
		else if (activeDragButton == MouseButton::Right)
		{
			const auto viewport = player.viewportSize();
			const auto logical = detail::windowSize(event, viewport);
			if (!viewport.x || !viewport.y || logical.x <= 0.f || logical.y <= 0.f)
				return;
			const auto up = m_params.extrinsics.up;
			const auto right = glm::cross(m_params.extrinsics.dir, up);
			const float height = frustumDiameterAtFocus();
			const float xScale = height * ((float)viewport.x / (float)viewport.y) / logical.x;
			const float yScale = height / logical.y;
			setEye(m_params.extrinsics.eye + (-dx * xScale * right + dy * yScale * up) * speedFactor);
		}
		else if (activeDragButton == MouseButton::Middle)
			setViewHeight((float)std::clamp((double)frustumDiameterAtFocus() * std::exp(.005 * dy * speedFactor), .001, 100000.));
		context.markHandled();
		return;
	}
	if (event.type() == EventType::MouseWheel)
	{
		const auto *wheel = event.data<MouseWheelEvent>();
		if (!wheel || !std::isfinite(wheel->y) || wheel->y == 0.f)
			return;
		const double height = frustumDiameterAtFocus() * std::pow(1. + .125 * speedFactor, -wheel->y);
		setViewHeight((float)std::clamp(height, .001, 100000.));
		context.markHandled();
	}
}

void Camera2D::gui (Device&, Player&)
{
	ImGui::SetNextWindowSize({0, 0}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
	ImGui::Begin("2D Camera");
	ImGui::PushItemWidth(10.f * ImGui::GetFontSize());
	const auto &in = m_params.intrinsics;
	if (ImGui::CollapsingHeader("Intrinsics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		float height = frustumDiameterAtFocus(), f = in.f;
		if (ImGui::DragFloat("View height", &height, .01f, .001f, 100000.f, "%.3f"))
			setViewHeight(height);
		if (ImGui::DragFloat("Focus distance", &f, .01f, .001f, 100000.f, "%.3f"))
			setFocalLength(f);
		float zNear = in.zNear, zFar = in.zFar;
		if (ImGui::DragFloat("Near plane", &zNear, .01f, .001f, zFar - .001f, "%.3f"))
			setNearPlane(zNear);
		if (ImGui::DragFloat("Far plane", &zFar, .1f, zNear + .001f, 100000.f, "%.1f"))
			setFarPlane(zFar);
	}
	if (ImGui::CollapsingHeader("Extrinsics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto eye = m_params.extrinsics.eye;
		if (ImGui::DragFloat3("Eye", &eye.x, .01f))
			setEye(eye);
		auto focus = glm::vec2(focalPoint());
		if (ImGui::DragFloat2("Focus XY", &focus.x, .01f))
			setFocalPoint(focus);
		float angle = rotation();
		if (ImGui::DragFloat("Rotation (deg)", &angle, .5f))
			setRotation(angle);
		if (ImGui::Button("Reset rotation"))
			resetRotation();
		ImGui::SameLine();
		if (ImGui::Button("Reset focus"))
			resetFocus();
	}
	if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Speed factor", &speedFactor, .01f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::TextUnformatted("RMB drag: pan, Shift+LMB drag: rotate");
		ImGui::TextUnformatted("Wheel/MMB drag: zoom");
		ImGui::TextUnformatted("Double-click an object to center XY");
	}
	ImGui::PopItemWidth();
	ImGui::End();
}

void Camera2D::update (Device&, Player &player, float dt)
{
	focusChange->update(player, dt);
	if (!viewMatrix)
		viewMatrix = m_params.extrinsics.modelviewMatrix();
	const auto viewport = player.viewportSize();
	if (!projMatrix && viewport.x && viewport.y)
		projMatrix = m_params.intrinsics.projectionMatrix(viewport);
}

void Camera2D::render (
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
