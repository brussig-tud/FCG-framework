
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/applet/camera_2d.h>
#include <FCG/applet/orbit_camera.h>
#include <FCG/camera_focus.h>
#include <FCG/device.h>
#include <FCG/player.h>
#include <FCG/res.h>
#include <FCG/run.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

[[nodiscard]] auto close (const glm::vec3 &a, const glm::vec3 &b) -> bool {
	return glm::length(a - b) < 1e-4f;
}

// Anonymous namespace end
}



//////
//
// Classes
//

/// Runs real camera callbacks against rendered depth, with deterministic animation ticks.
class CameraProbe : public fcg::Applet
{
public:
	CameraProbe(bool &passed, bool interactive) : passed(passed), interactive(interactive) {}

	~CameraProbe() override {
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(gpu, pipeline);
		if (logicalWindow)
			SDL_DestroyWindow(logicalWindow);
		if (driverRedraw)
			player->popContinuousRedraw();
	}

	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string name = "Camera lifecycle probe";
		return name;
	}

	void init (fcg::Device &device, fcg::Player &player) override
	{
		this->player = &player;
		player.pushContinuousRedraw();
		driverRedraw = true;
		camera.init(device, player);
		orbit.init(device, player);
		int count = 0;
		auto **windows = SDL_GetWindows(&count);
		require(windows && count == 1, "Expected the main window");
		mainWindow = windows[0];
		SDL_free(windows);
		const auto resource = fcg::res::shader("triangle");
		require(resource.has_value(), "Missing triangle shader");
		const auto vertex = resource->stage(fcg::ShaderStage::VERTEX);
		const auto fragment = resource->stage(fcg::ShaderStage::FRAGMENT);
		require(vertex.has_value() && fragment.has_value(), "Missing triangle stages");
		auto *vs = device.createShader(fcg::ShaderStage::VERTEX, vertex->spirv, 0);
		auto *fs = device.createShader(fcg::ShaderStage::FRAGMENT, fragment->spirv, 0);
		require(vs && fs, "Could not create shaders");
		SDL_GPUColorTargetDescription color{.format=player.swapchainFormat()};
		SDL_GPUGraphicsPipelineCreateInfo info{};
		info.vertex_shader = vs;
		info.fragment_shader = fs;
		info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		info.target_info.color_target_descriptions = &color;
		info.target_info.num_color_targets = 1;
		info.target_info.has_depth_stencil_target = true;
		info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
		info.depth_stencil_state.enable_depth_test = true;
		info.depth_stencil_state.enable_depth_write = true;
		info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
		pipeline = SDL_CreateGPUGraphicsPipeline(device.handle(), &info);
		SDL_ReleaseGPUShader(device.handle(), vs);
		SDL_ReleaseGPUShader(device.handle(), fs);
		require(pipeline != nullptr, "Could not create depth-writing pipeline");
		gpu = device.handle();
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldSize, fcg::Player &player) override {
		camera.onViewportResize(device, oldSize, player);
		orbit.onViewportResize(device, oldSize, player);
		if (step >= 8)
			resized = true;
	}

	void onEvent (const fcg::Event &event, fcg::EventContext &context, fcg::Player &player) override {
		if (interactive)
			camera.onEvent(event, context, player);
	}

	void gui (fcg::Device &device, fcg::Player &player) override {
		camera.gui(device, player);
	}

	[[nodiscard]] auto button (
		fcg::Applet &applet, Uint32 type, Uint8 button, float x = 0, float y = 0, Uint8 clicks = 1,
		SDL_Window *window = nullptr
	) -> bool {
		SDL_Event raw{};
		raw.type = type;
		raw.button.windowID = SDL_GetWindowID(window ? window : mainWindow);
		raw.button.button = button;
		raw.button.x = x;
		raw.button.y = y;
		raw.button.clicks = clicks;
		fcg::EventContext context;
		applet.onEvent(fcg::makeEvent(raw), context, *player);
		return context.wasHandled();
	}

	[[nodiscard]] auto motion (float dx, float dy) -> bool {
		SDL_Event raw{};
		raw.type = SDL_EVENT_MOUSE_MOTION;
		raw.motion.windowID = SDL_GetWindowID(mainWindow);
		raw.motion.xrel = dx;
		raw.motion.yrel = dy;
		fcg::EventContext context;
		camera.onEvent(fcg::makeEvent(raw), context, *player);
		return context.wasHandled();
	}

	void click (fcg::Applet &applet, float x = .55f, float y = .55f, SDL_Window *window = nullptr) {
		int width = 0, height = 0;
		SDL_GetWindowSize(window ? window : mainWindow, &width, &height);
		require(button(applet, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, x * width, y * height, 2, window),
			"Double click was not handled");
		(void)button(applet, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT);
	}

	void redraw (bool expected) {
		player->popContinuousRedraw();
		require(player->continuousRedrawRequested() == expected, "Unbalanced camera redraw request");
		player->pushContinuousRedraw();
	}

	[[nodiscard]] auto picked (const glm::mat4 &inverse, float x = .55f, float y = .55f) -> glm::vec3 {
		const auto viewport = player->viewportSize();
		const auto pixel = glm::uvec2(glm::vec2(x, y) * glm::vec2(viewport));
		const auto xy = (glm::vec2(pixel) + .5f) / glm::vec2(viewport);
		const auto world = inverse * glm::vec4(2.f * xy.x - 1.f, 1.f - 2.f * xy.y, 0, 1);
		return glm::vec3(world) / world.w;
	}

	void controls ()
	{
		const auto before = camera.params();
		SDL_SetModState(SDL_KMOD_NONE);
		require(!button(camera, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT), "Ordinary LMB handled");
		require(!motion(10, 20), "Ordinary LMB drag handled");
		(void)button(camera, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT);
		require(camera.params().extrinsics.eye == before.extrinsics.eye, "LMB drag changed eye");
		SDL_SetModState(SDL_KMOD_SHIFT);
		require(button(camera, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT), "Rotation drag ignored");
		SDL_SetModState(SDL_KMOD_NONE);
		require(motion(20, 0), "Latched shift drag ignored");
		require(std::abs(camera.rotation() - glm::degrees(.2f)) < 1e-4f, "Incorrect drag rotation");
		(void)button(camera, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT);
		camera.setRotation(90);
		int width = 0, height = 0;
		SDL_GetWindowSize(mainWindow, &width, &height);
		const auto viewport = player->viewportSize();
		const float xScale = 4.f * (float)viewport.x / (float)viewport.y / width;
		const float yScale = 4.f / height;
		(void)button(camera, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT);
		require(motion(10, 20), "Pan drag ignored");
		require(close(camera.params().extrinsics.eye, {-20 * yScale, -10 * xScale, 3}), "Incorrect rotated pan scale");
		(void)button(camera, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_RIGHT);
		camera.resetFocus();
		const auto eye = camera.params().extrinsics.eye;
		SDL_Event wheel{};
		wheel.type = SDL_EVENT_MOUSE_WHEEL;
		wheel.wheel.y = 1;
		fcg::EventContext context;
		camera.onEvent(fcg::makeEvent(wheel), context, *player);
		require(context.wasHandled() && std::abs(camera.frustumDiameterAtFocus() - 4.f / 1.125f) < 1e-5f, "Incorrect wheel zoom");
		(void)button(camera, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_MIDDLE);
		require(motion(0, 100), "MMB zoom ignored");
		(void)button(camera, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_MIDDLE);
		require(std::abs(camera.frustumDiameterAtFocus() - 4.f / 1.125f * std::exp(.5f)) < 1e-4f, "Incorrect MMB zoom");
		require(camera.params().extrinsics.eye == eye && camera.params().intrinsics.f == 3, "Zoom changed pose or focus distance");
		camera.setViewHeight(4);
		camera.resetRotation();
	}

	[[nodiscard]] auto pick (fcg::CameraFocus &focus, float x = .55f, float y = .55f) -> bool
	{
		int width = 0, height = 0;
		SDL_GetWindowSize(mainWindow, &width, &height);
		SDL_Event raw{};
		raw.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
		raw.button.windowID = SDL_GetWindowID(mainWindow);
		raw.button.button = SDL_BUTTON_LEFT;
		raw.button.x = x * width;
		raw.button.y = y * height;
		return focus.pick(fcg::makeEvent(raw), *player);
	}

	void publicFocus (fcg::Player &player)
	{
		for (auto *applet : std::array<fcg::Applet*, 2>{&orbit, &camera})
		{
			auto &common = *dynamic_cast<fcg::Camera*>(applet);
			const bool planar = applet == &camera;
			const auto view = common.modelviewMatrix();
			const auto projection = common.projectionMatrix(player.viewportSize());
			auto target = picked(glm::inverse(projection * view));
			const auto start = common.focalPoint();
			if (planar)
				target.z = start.z;
			fcg::CameraFocus focus(common, planar);
			require(!pick(focus), "Public controller picked without a rendered snapshot");
			focus.rendered(view, projection, player.viewportSize());
			require(pick(focus), "Public controller rejected a focus pick");
			focus.update(player, .25f);
			require(close(common.focalPoint(), glm::mix(start, target, .5f)), "Public controller midpoint differs");
			redraw(true);
			focus.cancel();
			const auto stopped = common.focalPoint();
			focus.update(player, .5f);
			require(close(common.focalPoint(), stopped), "Public controller cancellation failed");
			redraw(false);
			require(pick(focus), "Public controller rejected repeated focus pick");
			focus.update(player, .5f);
			require(close(common.focalPoint(), target), "Public controller final focus differs");
			redraw(false);
			require(pick(focus), "Public controller rejected pending cancellation pick");
			focus.cancel();
			focus.update(player, .5f);
			require(close(common.focalPoint(), target), "Public cancelled readback changed focus");
			focus.resized();
			require(!pick(focus), "Public controller retained snapshot after resize");
			focus.rendered(view, projection, player.viewportSize());
			require(pick(focus), "Public controller rejected destructor-cleanup pick");
			// Destruction collects the pending readback through the exported public API.
		}
		redraw(false);
	}

	void mutations (fcg::Device &device, fcg::Player &player)
	{
		for (fcg::Applet *applet : std::array<fcg::Applet*, 2>{&camera, &orbit})
		{
			auto &common = *dynamic_cast<fcg::Camera*>(applet);
			const std::array<std::function<void()>, 12> setters{
				[&] { common.setFovY(common.params().intrinsics.fovY); },
				[&] { common.setFocalLength(common.params().intrinsics.f); },
				[&] { common.setNearPlane(.125f); },
				[&] { common.setFarPlane(100.f); },
				[&] { common.setEye(common.params().extrinsics.eye); },
				[&] { common.setDir(common.params().extrinsics.dir); },
				[&] { common.setUp(common.params().extrinsics.up); },
				[&] { common.setFocalPoint(common.focalPoint()); },
				[&] { common.translateToFocalPoint(common.focalPoint()); },
				[&] { common.resetRotation(); },
				[&] { common.resetFocus(); },
				[&] { common.setFrustumDiameterAtFocus(common.frustumDiameterAtFocus()); }
			};
			for (const auto &set : setters)
			{
				click(*applet);
				applet->update(device, player, .1f);
				redraw(true);
				set();
				const auto focus = common.focalPoint();
				redraw(false);
				applet->update(device, player, .5f);
				require(close(common.focalPoint(), focus), "Shared setter did not cancel focus animation");
			}
		}
		click(camera);
		camera.update(device, player, .1f);
		bool rejected = false;
		try {
			camera.setUp({0, 0, 1});
		}
		catch (const std::invalid_argument&) {
			rejected = true;
		}
		require(rejected, "Tilted up was accepted during animation");
		redraw(true); // Rejected mutations must leave the animation active.
		camera.resetRotation();
		redraw(false);
		camera.resetFocus();
	}

	void update (fcg::Device &device, fcg::Player &player, float dt) override
	{
		try {
			advance(device, player, dt);
		}
		catch (const std::exception &error) {
			std::fprintf(stderr, "Camera lifecycle: %s at step %u\n", error.what(), step);
			player.requestClose();
		}
	}

	void advance (fcg::Device &device, fcg::Player &player, float dt)
	{
		camera.update(device, player, interactive ? dt : 0.f);
		orbit.update(device, player, 0.f);
		if (interactive)
			return;
		++step;
		if (step == 3)
			controls();
		if (step == 4)
		{
			// Simulate a display whose logical dimensions are half its drawable viewport.
			const auto viewport = player.viewportSize();
			logicalWindow = SDL_CreateWindow("logical coordinates", viewport.x / 2, viewport.y / 2, SDL_WINDOW_HIDDEN);
			require(logicalWindow != nullptr, "Could not create scaled input window");
			auto target = picked(lastPlanarInverse);
			target.z = camera.focalPoint().z;
			camera.setViewHeight(8); // Picking must still use the previous rendered height 4.
			click(camera, .55f, .55f, logicalWindow);
			camera.update(device, player, .25f);
			require(close(camera.focalPoint(), .5f * target), "Snapshot or scaled-coordinate focus differs");
			redraw(true);
			camera.update(device, player, .25f);
			require(close(camera.focalPoint(), target), "Focus animation did not finish");
			redraw(false);
		}
		if (step == 5)
		{
			const auto focus = camera.focalPoint();
			click(camera, .05f, .05f);
			camera.update(device, player, .5f);
			require(close(camera.focalPoint(), focus), "Background click changed focus");
			redraw(false);
			click(camera);
			camera.resetFocus(); // Cancel pending application but still collect its token.
			camera.update(device, player, .5f);
			require(close(camera.focalPoint(), glm::vec3(0)), "Cancelled readback changed focus");
			redraw(false);
		}
		if (step == 6)
		{
			click(camera);
			camera.update(device, player, .1f);
			redraw(true);
			const auto focus = camera.focalPoint();
			camera.setRotation(17);
			camera.update(device, player, .5f);
			require(close(camera.focalPoint(), focus), "Cancelled animation continued");
			redraw(false);
			camera.resetFocus();
			camera.resetRotation();
		}
		if (step == 7)
		{
			click(camera, .5f, .5f);
			click(camera, .55f, .55f); // Two readbacks in one update, both collected.
			auto target = picked(lastPlanarInverse);
			target.z = 0;
			camera.update(device, player, .5f);
			require(close(camera.focalPoint(), target), "Repeated click did not use latest target");
			redraw(false);
			const auto orbitTarget = picked(lastOrbitInverse);
			click(orbit);
			orbit.update(device, player, .5f);
			require(close(orbit.focalPoint(), orbitTarget), "Orbit focus depth unprojection differs");
			redraw(false);
		}
		if (step == 8) {
			click(camera);
			beforeResize = camera.focalPoint();
			require(SDL_SetWindowSize(mainWindow, 640, 360), "Could not resize camera viewport");
		}
		if (step >= 11 && resized)
		{
			require(close(camera.focalPoint(), beforeResize), "Resize-invalidated readback changed focus");
			require(camera.frustumDiameterAtFocus() == 8, "Resize changed vertical extent");
			const auto matrix = camera.projectionMatrix(player.viewportSize());
			require(std::abs(matrix[1][1] - .25f) < 1e-5f, "Resize changed vertical projection");
			click(camera);
			camera.update(device, player, .1f);
			redraw(true);
			camera.resetRotation();
			redraw(false);
			mutations(device, player);
			camera.setEyeZ(1000000.f);
			const float configuredDepth = camera.focalPoint().z;
			click(camera);
			for (unsigned tick = 0; tick < 3; ++tick) {
				camera.update(device, player, .17f);
				require(camera.focalPoint().z == configuredDepth, "Planar animation rounded configured depth");
			}
			redraw(false);
			publicFocus(player);
			// Zero-viewports must skip projection and picking without scheduling a download.
			fcg::Player empty(device, nullptr);
			fcg::applet::Camera2D unrendered;
			unrendered.update(device, empty, .1f);
			fcg::RenderState state(device);
			unrendered.render(device, state, nullptr, nullptr, empty);
			SDL_Event raw{};
			raw.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
			raw.button.button = SDL_BUTTON_LEFT;
			raw.button.clicks = 2;
			fcg::EventContext context;
			unrendered.onEvent(fcg::makeEvent(raw), context, empty);
			require(!context.wasHandled(), "Empty viewport accepted a pick");
			// Destruction during an active animation releases its redraw request.
			{
				fcg::applet::Camera2D temporary;
				temporary.update(device, player, 0);
				temporary.render(device, state, nullptr, nullptr, player);
				click(temporary);
				temporary.update(device, player, .1f);
				redraw(true);
			}
			redraw(false);
			{
				fcg::applet::Camera2D temporary;
				temporary.render(device, state, nullptr, nullptr, player);
				click(temporary); // Destruction collects even a pending, cancelled readback.
			}
			passed = true;
			player.requestClose();
		}
		require(step < 40, "Camera lifecycle did not reach completion");
	}

	void render (
		fcg::Device &device, fcg::RenderState &state, SDL_GPURenderPass *pass,
		SDL_GPUCommandBuffer *command, fcg::Player &player
	) override {
		orbit.render(device, state, pass, command, player);
		lastOrbitInverse = glm::inverse(state.projectionMatrix() * state.modelviewMatrix());
		camera.render(device, state, pass, command, player);
		lastPlanarInverse = glm::inverse(state.projectionMatrix() * state.modelviewMatrix());
		SDL_BindGPUGraphicsPipeline(pass, pipeline);
		SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
	}

private:
	fcg::applet::Camera2D camera;
	fcg::applet::OrbitCamera orbit;
	fcg::Player *player = nullptr;
	SDL_Window *mainWindow = nullptr, *logicalWindow = nullptr;
	SDL_GPUDevice *gpu = nullptr;
	SDL_GPUGraphicsPipeline *pipeline = nullptr;
	glm::mat4 lastPlanarInverse{1}, lastOrbitInverse{1};
	glm::vec3 beforeResize{0};
	unsigned step = 0;
	bool &passed;
	bool interactive, driverRedraw = false, resized = false;
};



//////
//
// Functions
//

[[nodiscard]] auto main () -> int {
	bool passed = false;
	const bool interactive = std::getenv("FCG_CAMERA_PROBE_INTERACTIVE") != nullptr;
	try {
		std::vector<std::unique_ptr<fcg::Applet>> applets;
		applets.push_back(std::make_unique<CameraProbe>(passed, interactive));
		const auto result = fcg::run(std::move(applets), {.mainWindowTitle="FCG Camera Probe"});
		return result ? result : (passed || interactive ? 0 : 1);
	}
	catch (const std::exception &error) {
		std::fprintf(stderr, "%s\n", error.what());
		return 1;
	}
}
