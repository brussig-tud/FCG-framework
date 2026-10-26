
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui library
#include <imgui.h>

// FCG Framework
#include <FCG/applet/camera_2d.h>
#include <FCG/applet/orbit_camera.h>
#include <FCG/event.h>
#include <FCG/frame.h>
#include <FCG/gui.h>
#include <FCG/player.h>
#include <FCG/res.h>
#include <FCG/run.h>
#include <FCG/window.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// The selected bounded runtime scenario.
std::string scenario;

/// The current GUI/update iteration, also visible to the event-wait observers.
unsigned step = 0;

/// Synthetic input consumed by the production event loop.
std::optional<SDL_Event> queuedEvent;

/// Number of acquisitions that must report ordinary image unavailability.
unsigned unavailableImages = 0;

/// Whether this iteration has no acquired window image.
bool noImage = false;

/// Number of waits reached while a redraw remains pending without a drawable image.
unsigned unavailableWaits = 0;

/// Number of real GUI caret timeouts requested by the runtime.
unsigned periodicWaits = 0;

/// Number of frame command buffers submitted by production cleanup.
unsigned submissions = 0;

/// Fail a probe independently of build-time assertion settings.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Supply only explicitly queued events, keeping OS event bursts out of redraw counts.
auto pollEvent (SDL_Event *event) -> bool {
	if (!queuedEvent)
		return false;
	*event = *std::exchange(queuedEvent, std::nullopt);
	return true;
}

/// Observe real scheduling decisions without hanging a failed test in an indefinite wait.
auto waitEvent (SDL_Event *event, int timeout) -> bool
{
	if (pollEvent(event))
		return true;
	if (scenario == "caret") {
		require(timeout == fcg::Gui::periodicRedrawIntervalMs, "Caret redraw did not use a timed wait");
		++periodicWaits;
		return false;
	}
	if (scenario == "unavailable" && noImage) {
		require(timeout == -1, "Unavailable scene redraw unexpectedly polled on a timer");
		++unavailableWaits;
		event->type = SDL_EVENT_USER;
		return true;
	}
	throw std::runtime_error("Pending scene redraw entered an event wait without a wakeup");
}

/// Inject unavailable images while retaining real acquisitions for drawable iterations.
auto acquireImage (
	SDL_GPUCommandBuffer *command, SDL_Window *window, SDL_GPUTexture **texture, Uint32 *width, Uint32 *height
) -> bool
{
	noImage = unavailableImages > 0;
	if (noImage) {
		--unavailableImages;
		*texture = nullptr;
		*width = *height = 0;
		return true;
	}
	return SDL_WaitAndAcquireGPUSwapchainTexture(command, window, texture, width, height);
}

/// Inject failure before the first required scene pass records any work.
auto beginPass (
	SDL_GPUCommandBuffer *command, const SDL_GPUColorTargetInfo *colors, Uint32 count,
	const SDL_GPUDepthStencilTargetInfo *depth
) -> SDL_GPURenderPass*
{
	if (scenario == "failure" && depth) {
		SDL_SetError("scene pass sentinel");
		return nullptr;
	}
	return SDL_BeginGPURenderPass(command, colors, count, depth);
}

/// Count submissions without changing their normal consumption or error behavior.
auto submitFrame (SDL_GPUCommandBuffer *command) -> bool {
	++submissions;
	return SDL_SubmitGPUCommandBuffer(command);
}

// Anonymous namespace end
}



//////
//
// Includes
//

// Substitute narrow SDL boundaries while exercising the production frame loop and GPU work
#define SDL_BeginGPURenderPass beginPass
#define SDL_SubmitGPUCommandBuffer submitFrame
#include "../../src/frame.cpp"
#undef SDL_SubmitGPUCommandBuffer
#undef SDL_BeginGPURenderPass
#define SDL_WaitAndAcquireGPUSwapchainTexture acquireImage
#include "../../src/window.cpp"
#undef SDL_WaitAndAcquireGPUSwapchainTexture
#define SDL_PollEvent pollEvent
#define SDL_WaitEventTimeout waitEvent
#include "../../src/run.cpp"
#undef SDL_WaitEventTimeout
#undef SDL_PollEvent



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Counts that remain available after the applet has been destroyed.
struct Results
{
	/// Scene passes that actually invoked the applet.
	unsigned renders = 0;

	/// Custom presentation recorder invocations.
	unsigned presentations = 0;

	/// Executed GUI backend callbacks, including GUI-only frames.
	unsigned overlays = 0;

	/// Completed pipeline recreation callbacks.
	unsigned recreations = 0;

	/// Observed viewport changes.
	unsigned resizes = 0;
};

/// Exercise conditional redraw with actual presentation, overlays, readbacks and camera mutations.
class RedrawProbe final : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Borrow counters that outlive the runtime.
	explicit RedrawProbe(Results &results) : results(results) {}

	/// Release the scene pipeline before the device is destroyed.
	~RedrawProbe() override {
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(gpu, pipeline);
	}


	////
	// Interface: fcg::Applet

	/// Return the scenario name for failure diagnostics.
	[[nodiscard]] auto name () const -> const std::string& override { return scenario; }

	/// Set a distinguishable background and install real sRGB presentation.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		gpu = device.handle();
		this->player = &player;
		player.clearColor() = {0, 0, 0, 1};
		installPresentation(player);
		if (scenario == "cameras") {
			planar.init(device, player);
			orbit.init(device, player);
		}
	}

	/// Create a depth-writing triangle compatible with the effective scene format.
	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		++results.recreations;
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(gpu, std::exchange(pipeline, nullptr));
		const auto shader = fcg::res::shader("triangle").value();
		const auto release = [&] (SDL_GPUShader *stage) { SDL_ReleaseGPUShader(gpu, stage); };
		const std::unique_ptr<SDL_GPUShader, decltype(release)> vertex(
			device.createShader(fcg::ShaderStage::VERTEX, shader.stage(fcg::ShaderStage::VERTEX)->spirv, 0), release
		);
		const std::unique_ptr<SDL_GPUShader, decltype(release)> fragment(
			device.createShader(fcg::ShaderStage::FRAGMENT, shader.stage(fcg::ShaderStage::FRAGMENT)->spirv, 0), release
		);
		require(vertex && fragment, "Could not create depth-test shaders");
		const auto target = player.mainRenderTargetInfo().value();
		const SDL_GPUColorTargetDescription color{.format = target.colorFormat};
		SDL_GPUGraphicsPipelineCreateInfo info{};
		info.vertex_shader = vertex.get();
		info.fragment_shader = fragment.get();
		info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		info.target_info = {&color, 1, target.depthStencilFormat, true};
		info.depth_stencil_state.enable_depth_test = true;
		info.depth_stencil_state.enable_depth_write = true;
		info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
		pipeline = SDL_CreateGPUGraphicsPipeline(gpu, &info);
		require(pipeline, "Could not create depth-test pipeline");
	}

	/// Observe resize notifications while the runtime owns invalidation.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {
		++results.resizes;
	}

	/// Request a scene refresh from an input callback after the first four iterations.
	void onEvent (const fcg::Event &event, fcg::EventContext&, fcg::Player &player) override {
		if (event.type() == fcg::EventType::DropComplete)
			player.requireSceneRedraw();
	}

	/// Build drawable widgets on every iteration and request refreshes from GUI and overlay callbacks.
	void gui (fcg::Device&, fcg::Player &player) override
	{
		++step;
		ImGui::Begin("Redraw probe");
		if (scenario == "caret") {
			if (step == 1)
				ImGui::SetKeyboardFocusHere();
			ImGui::InputText("Editor", text.data(), text.size());
		}
		ImGui::Text("Iteration %u", step);
		ImGui::End();
		if (scenario == "requests" && step == 2) {
			player.requireSceneRedraw();
			player.requireSceneRedraw();
		}
		auto *draw = ImGui::GetForegroundDrawList();
		draw->AddRectFilled(ImVec2(0, 0), ImVec2(8, 8), IM_COL32_WHITE);
		draw->AddCallback([] (const ImDrawList*, const ImDrawCmd *command) {
			auto &probe = *static_cast<RedrawProbe*>(command->UserCallbackData);
			++probe.results.overlays;
			if (scenario == "requests" && step == 13)
				probe.player->requireSceneRedraw();
		}, this);
	}

	/// Advance bounded scenarios without using continuous redraw to drive GUI-only frames.
	void update (fcg::Device &device, fcg::Player &player, float) override
	{
		// Apply scene, presentation and input changes at distinguishable iterations
		if (scenario == "requests")
		{
			if (step == 3)
				player.clearColor() = {1, 0, 0, 1};
			if (step == 4 || step == 10) {
				player.requireSceneRedraw();
				player.requireSceneRedraw();
			}
			if (step == 6)
			{
				player.gammaCorrection().gamma = glm::vec3(2);
				const auto token = player.scheduleDepthReadback();
				const auto &depth = player.getDepthReadbackResult(token);
				require(depth.readTexel<float>(glm::uvec3(player.viewportSize() / 2u, 0)).value() == 0.f,
					"GUI-only frames lost rendered depth contents");
			}
			if (step == 7 || step == 9)
				installPresentation(player);
			if (step == 8)
				player.clearPresentationRecorder();
		}

		// Keep a request pending through enough unavailable iterations to exhaust ordinary GUI follow-ups
		if (scenario == "unavailable")
		{
			if (step == 2)
				unavailableImages = 6;
			if (step == 3 || step == 4)
				player.requireSceneRedraw();
			if (step == 4) {
				require(player.setSceneFormat(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM).has_value(), "Format rejected");
				resize(player);
			}
		}

		// Force independent size and format refreshes without explicit redraw requests
		if (scenario == "attachments") {
			if (step == 2)
				resize(player);
			if (step == 3)
				require(player.setSceneFormat(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM).has_value(), "Format rejected");
		}

		// Preserve continuous drawing through the callback that releases its final request
		if (scenario == "continuous") {
			if (step == 1)
				player.pushContinuousRedraw();
			if (step == 3)
				player.popContinuousRedraw();
		}
		if (scenario == "cameras")
			advanceCameras(device, player);

		// Drive GUI-only iterations with explicit events, leaving late requests to wake the loop themselves
		const bool wake = scenario == "requests" ? step < 9 || step == 14
			: scenario == "caret" ? step < 4 : scenario == "unavailable" ? step < 2 || step >= 9 : true;
		if (wake) {
			SDL_Event event{};
			event.type = SDL_EVENT_USER;
			if (scenario == "requests" && step == 4) {
				event.type = SDL_EVENT_DROP_COMPLETE;
				event.drop.windowID = player.mainWindow()->id();
			}
			queuedEvent = event;
		}
		const auto finalStep = scenario == "requests" ? 15u : scenario == "unavailable" ? 10u
			: scenario == "attachments" ? 6u : scenario == "continuous" ? 5u : scenario == "cameras" ? 14u : 20u;
		if (step == finalStep || (scenario == "caret" && periodicWaits >= 2))
			player.requestClose();
	}

	/// Draw geometry whose color and depth remain usable after scene rendering is skipped.
	void render (
		fcg::Device &device, fcg::RenderState &state, SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *command,
		fcg::Player &player
	) override
	{
		++results.renders;
		if (scenario == "cameras") {
			planar.render(device, state, pass, command, player);
			orbit.render(device, state, pass, command, player);
		}
		SDL_BindGPUGraphicsPipeline(pass, pipeline);
		SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
		if (scenario == "requests" && (step == 10 || step == 11))
			player.requireSceneRedraw();
	}


private:

	////
	// Methods

	/// Return the cumulative number of scene passes expected at this presentation point.
	[[nodiscard]] auto expectedRenders () const -> unsigned
	{
		if (scenario == "requests")
			return std::array{1u, 2u, 2u, 3u, 4u, 4u, 4u, 4u, 4u, 5u, 6u, 7u, 8u, 9u, 9u}.at(step - 1);
		if (scenario == "continuous")
			return std::array{1u, 2u, 3u, 3u, 3u}.at(step - 1);
		if (scenario == "attachments")
			return std::array{1u, 1u, 2u, 3u, 3u, 3u}.at(step - 1);
		if (scenario == "cameras")
			return std::array{1u, 2u, 2u, 3u, 4u, 5u, 5u, 6u, 7u, 7u, 8u, 8u, 9u, 9u}.at(step - 1);
		return scenario == "unavailable" && step >= 9 ? 2u : 1u;
	}

	/// Install a recorder that checks rendering decisions before sampling the scene.
	void installPresentation (fcg::Player &player)
	{
		auto pass = fcg::FullscreenPass::linearToSRGB(player.device(), player.swapchainFormat());
		require(pass.has_value(), "Could not create presentation pass");
		player.setPresentationRecorder([this, pass = std::move(*pass)] (const fcg::PresentationContext &context)
			-> std::expected<void, fcg::FullscreenError>
		{
			++results.presentations;
			require(results.renders == expectedRenders(), "Scene render count disagrees with redraw requests");
			require(context.scene.info().format == this->player->mainRenderTargetInfo()->colorFormat,
				"Presentation sampled an obsolete scene format");
			require(context.target.extent == this->player->viewportSize(), "Presentation used an obsolete viewport");
			if (scenario == "requests" && step >= 3 && step <= 5)
				checkBackground(context.scene, step == 5);
			if (scenario == "requests" && step == 12)
				this->player->requireSceneRedraw();
			const fcg::FullscreenTextureBinding binding{&context.scene, &context.sceneSampler};
			const auto &gamma = std::as_const(*this->player).gammaCorrection();
			const std::span<const std::byte> uniforms[]{std::as_bytes(std::span(&gamma, 1))};
			return pass.record(context.cmdBuffer, context.target, {.samplers = {&binding, 1}, .uniforms = uniforms});
		});
	}

	/// Check submitted scene pixels before the current frame's commands are submitted.
	void checkBackground (const fcg::Texture &scene, bool red)
	{
		const SDL_GPUTextureRegion region{scene.handle(), 0, 0, 0, 0, 0, 1, 1, 1};
		auto readback = fcg::TextureReadback::create(player->device(), region, scene.info().format);
		require(readback.has_value() && readback->wait().has_value(), "Scene pixel download failed");
		auto mapping = readback->map();
		require(mapping.has_value(), "Scene pixel mapping failed");
		const auto pixel = mapping->readTexel<std::array<std::uint8_t, 4>>({0, 0, 0}).value();
		require(pixel == std::array<std::uint8_t, 4>{std::uint8_t(red ? 255 : 0), 0, 0, 255},
			"Clear-color edit changed retained pixels without a redraw, or redraw failed to apply it");
	}

	/// Synchronize a size change before the next acquisition or unavailable-image size poll.
	void resize (fcg::Player &player) {
		require(SDL_SetWindowSize(player.mainWindow()->handle(), 640, 360), "Resize failed");
		require(SDL_SyncWindow(player.mainWindow()->handle()), "Resize synchronization failed");
	}

	/// Change cameras after their updates, pick retained depth, finish focus animation and navigate with the mouse.
	void advanceCameras (fcg::Device &device, fcg::Player &player)
	{
		planar.update(device, player, 0.f);
		orbit.update(device, player, 0.f);
		if (step == 2)
			planar.setViewHeight(8.f);
		if (step == 4)
			orbit.setFovY(45.f);
		if (step == 5)
			planar.setFocalLength(4.f);
		if (step == 6)
			orbit.setFocalLength(4.f);
		if (step == 7) {
			try { planar.setViewHeight(std::nanf("")); }
			catch (const std::invalid_argument&) { return; }
			throw std::runtime_error("Invalid camera mutation was accepted");
		}
		if (step == 8)
		{
			int width = 0, height = 0;
			require(SDL_GetWindowSize(player.mainWindow()->handle(), &width, &height), "Could not query pick coordinates");
			SDL_Event click{};
			click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
			click.button.windowID = player.mainWindow()->id();
			click.button.button = SDL_BUTTON_LEFT;
			click.button.clicks = 2;
			click.button.x = .52f * width;
			click.button.y = .52f * height;
			fcg::EventContext context;
			planar.onEvent(fcg::makeEvent(click), context, player);
			planar.update(device, player, .25f);
			require(player.continuousRedrawRequested(), "Picking retained depth did not start a focus animation");
		}
		if (step == 9) {
			planar.update(device, player, .25f);
			require(!player.continuousRedrawRequested(), "Focus animation did not release continuous redraw");
		}

		// Exercise the orbit path that changes its pose directly, and the planar wheel path through a setter
		fcg::EventContext context;
		if (step == 11)
		{
			SDL_Event input{};
			input.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
			input.button.windowID = player.mainWindow()->id();
			input.button.button = SDL_BUTTON_LEFT;
			input.button.clicks = 1;
			orbit.onEvent(fcg::makeEvent(input), context, player);
			input = {};
			input.type = SDL_EVENT_MOUSE_MOTION;
			input.motion.windowID = player.mainWindow()->id();
			input.motion.xrel = input.motion.yrel = 1.f;
			orbit.onEvent(fcg::makeEvent(input), context, player);
		}
		if (step == 13) {
			SDL_Event wheel{};
			wheel.type = SDL_EVENT_MOUSE_WHEEL;
			wheel.wheel.windowID = player.mainWindow()->id();
			wheel.wheel.y = 1.f;
			planar.onEvent(fcg::makeEvent(wheel), context, player);
		}
	}


	////
	// Fields

	/// Caller-owned observations.
	Results &results;

	/// The runtime player, borrowed until applet destruction.
	fcg::Player *player = nullptr;

	/// Device handle used for scene-pipeline ownership.
	SDL_GPUDevice *gpu = nullptr;

	/// The current depth-writing triangle pipeline.
	SDL_GPUGraphicsPipeline *pipeline = nullptr;

	/// A camera whose public mutations and depth-based focus are exercised after GUI-only frames.
	fcg::applet::Camera2D planar;

	/// A camera whose projection and focal-distance mutations also request scene refreshes.
	fcg::applet::OrbitCamera orbit;

	/// Editable text keeping the GUI caret active for periodic redraws.
	std::array<char, 32> text{"retained scene"};
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run one deterministic scenario through the actual runtime and verify its final observations.
auto main (int argc, char **argv) -> int
{
	try
	{
		require(argc == 2, "Expected one scene redraw scenario");
		scenario = argv[1];
		require(scenario == "requests" || scenario == "caret" || scenario == "unavailable"
			|| scenario == "attachments" || scenario == "continuous" || scenario == "cameras"
			|| scenario == "failure", "Unknown scene redraw scenario");
		Results results;
		bool failed = false;
		try {
			require(fcg::run({}, std::make_unique<RedrawProbe>(results)) == 0, "Runtime returned failure");
		} catch (const std::runtime_error &error) {
			require(scenario == "failure" && std::string(error.what()) == "Beginning the scene render pass failed",
				error.what());
			failed = true;
		}
		if (scenario == "failure") {
			require(failed && submissions == 1 && results.renders == 0 && results.presentations == 0
				&& results.overlays == 0, "Failed scene pass did not stop presentation and finish its frame");
			return 0;
		}
		require(!failed && submissions == step - (scenario == "unavailable" ? 6u : 0u),
			"Drawable frame completion count is wrong");
		require(results.overlays == submissions && results.presentations == submissions - (scenario == "requests" ? 1u : 0u),
			"GUI-only frames skipped presentation or GUI overlay rendering");
		if (scenario == "caret")
			require(periodicWaits >= 2 && results.renders == 1, "Caret timeouts refreshed the scene");
		if (scenario == "unavailable") {
			require(unavailableWaits > 0, "Pending scene redraw spun while no drawable image was available");
			require(results.renders == 2 && results.recreations == 2 && results.resizes > 0,
				"Unavailable redraw was lost or skipped attachment changes");
		}
		if (scenario == "attachments")
			require(results.recreations == 2 && results.resizes > 0, "Attachment changes were not exercised");
		return 0;
	}
	catch (const std::exception &error) {
		std::cerr << "Scene redraw " << scenario << " at iteration " << step << ": " << error.what() << '\n';
		return 1;
	}
}
