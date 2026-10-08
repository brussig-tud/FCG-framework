
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <exception>

// Local includes
#include "FCG/camera_focus.h"
#include "camera_utils.h"
#include "FCG/player.h"
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
// CameraFocus

CameraFocus::CameraFocus(Camera &camera, bool planar) : camera(camera), planar(planar) {}

CameraFocus::~CameraFocus()
{
	if (pending)
		try {
			(void)pending->player->getDepthReadbackResult(pending->token);
		}
		catch (const std::exception &error) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Camera: collecting final depth readback: %s", error.what());
		}
	releaseRedraw();
}

void CameraFocus::releaseRedraw () {
	if (redrawPlayer)
		redrawPlayer->popContinuousRedraw();
	redrawPlayer = nullptr;
}

void CameraFocus::cancel ()
{
	if (applying)
		return;
	animation.reset();
	if (pending)
		pending->apply = false;
	releaseRedraw();
}

void CameraFocus::resized () {
	cancel();
	// Player has already replaced the download storage and invalidated its tokens.
	pending.reset();
	lastRendered.reset();
}

void CameraFocus::rendered (
	const glm::mat4 &view, const glm::mat4 &projection, const glm::uvec2 &viewport
) {
	if (viewport.x && viewport.y)
		lastRendered = Snapshot{glm::inverse(projection * view), viewport};
}

auto CameraFocus::pick (const Event &event, Player &player) -> bool
{
	const auto *click = event.data<MouseButtonEvent>();
	if (!click || !lastRendered || lastRendered->viewport != player.viewportSize())
		return false;
	const auto logical = detail::windowSize(event, lastRendered->viewport);
	if (logical.x <= 0.f || logical.y <= 0.f || !std::isfinite(click->x) || !std::isfinite(click->y)
		|| click->x < 0.f || click->y < 0.f || click->x >= logical.x || click->y >= logical.y)
		return false;
	const auto pixel = glm::uvec2(glm::vec2(click->x, click->y) * glm::vec2(lastRendered->viewport) / logical);
	cancel();
	// Repeated clicks may arrive before update; collect the previous token before scheduling another.
	if (pending) {
		(void)player.getDepthReadbackResult(pending->token);
		pending.reset();
	}
	pending = Pending{&player, player.scheduleDepthReadback(), *lastRendered, pixel, true};
	return true;
}

void CameraFocus::update (Player &player, float dt)
{
	if (pending)
	{
		const auto request = *pending;
		const auto &depth = player.getDepthReadbackResult(request.token);
		pending.reset();
		if (request.apply && glm::uvec2(depth.extent()) == request.snapshot.viewport
			&& glm::all(glm::lessThan(request.pixel, glm::uvec2(depth.extent()))))
		{
			const float z = depth.readTexel<float>(glm::uvec3(request.pixel, 0)).value();
			if (std::isfinite(z) && z >= 0.f && z < 1.f)
			{
				const auto xy = (glm::vec2(request.pixel) + .5f) / glm::vec2(request.snapshot.viewport);
				const auto world = request.snapshot.inverse * glm::vec4(2.f * xy.x - 1.f, 1.f - 2.f * xy.y, z, 1.f);
				if (std::isfinite(world.w) && std::abs(world.w) > 1e-7f)
				{
					auto target = glm::vec3(world) / world.w;
					const auto start = camera.focalPoint();
					if (planar)
						target.z = start.z;
					if (std::isfinite(target.x) && std::isfinite(target.y) && std::isfinite(target.z)) {
						animation = Animation{start, target, 0.f};
						player.pushContinuousRedraw();
						redrawPlayer = &player;
					}
				}
			}
		}
	}
	if (!animation)
		return;
	animation->elapsed += std::isfinite(dt) ? glm::max(dt, 0.f) : 0.f;
	const float t = glm::clamp(animation->elapsed / .5f, 0.f, 1.f);
	applying = true;
	try {
		auto focus = glm::mix(animation->start, animation->target, glm::smoothstep(0.f, 1.f, t));
		if (planar)
			focus.z = animation->start.z;
		camera.translateToFocalPoint(focus);
	}
	catch (...) {
		applying = false;
		cancel();
		throw;
	}
	applying = false;
	if (t >= 1.f) {
		animation.reset();
		releaseRedraw();
	}
}



//////
//
// Module namespace close
//

} // namespace fcg
