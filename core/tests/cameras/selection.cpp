
//////
//
// Includes
//

// C++ STL
#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/applet/camera_2d.h>
#include <FCG/device.h>
#include <FCG/event.h>
#include <FCG/player.h>
#include <FCG/render_state.h>
#include <FCG/window.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Scenario selected by CTest.
std::string scenario;

/// Iteration observed at frame acquisition, including iterations without an image.
unsigned iteration = 0;

/// Input explicitly queued by the driver.
std::optional<SDL_Event> queuedEvent;

/// Whether the next acquisition should model a minimized window.
bool unavailable = false;

/// Reject a failed behavioral contract in every build configuration.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Supply deterministic input instead of operating-system event bursts.
auto pollEvent (SDL_Event *event) -> bool {
	if (!queuedEvent)
		return false;
	*event = *std::exchange(queuedEvent, std::nullopt);
	return true;
}

/// Fail promptly if the runtime loses the driver's wakeup or pending selection.
auto waitEvent (SDL_Event *event, int) -> bool {
	if (pollEvent(event))
		return true;
	throw std::runtime_error("Camera selection entered an unexpected event wait");
}

/// Exercise selection and maintenance with and without a drawable image.
auto acquireImage (
	SDL_GPUCommandBuffer *command, SDL_Window *window, SDL_GPUTexture **texture, Uint32 *width, Uint32 *height
) -> bool
{
	++iteration;
	if (std::exchange(unavailable, false)) {
		*texture = nullptr;
		*width = *height = 0;
		return true;
	}
	return SDL_WaitAndAcquireGPUSwapchainTexture(command, window, texture, width, height);
}

// Anonymous namespace end
}



//////
//
// Includes
//

// Substitute only acquisition and event boundaries of the production runtime
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

/// Callback observations for one camera.
struct Counts
{
	/// Completed initialization calls.
	unsigned init = 0;

	/// Pipeline preparation calls.
	unsigned pipelines = 0;

	/// Viewport notifications.
	unsigned resizes = 0;

	/// Started activation hooks.
	unsigned activated = 0;

	/// Completed deactivation hooks.
	unsigned deactivated = 0;

	/// Last acknowledged viewport size in pixels.
	glm::uvec2 viewport{0};
};

/// Observations retained after runtime teardown.
struct Results
{
	/// Borrowed cameras used for selections while the runtime owns them.
	std::array<fcg::Camera*, 4> cameras{};

	/// Per-camera lifecycle counts.
	std::array<Counts, 4> counts{};

	/// Callback names grouped by iteration.
	std::array<std::vector<std::string>, 16> trace;

	/// Destruction order, with scenes numbered four and five.
	std::vector<unsigned> destroyed;

	/// Initialization order across both applet categories.
	std::vector<unsigned> initialized;

	/// Whether noexcept cleanup observed balanced transitions and a valid player.
	bool cleanupValid = true;
};

/// A real camera whose callbacks expose delivery and maintenance contracts.
class CameraProbe final : public fcg::applet::Camera2D
{
public:

	////
	// Object construction/destruction

	/// Borrow observations and assign a possibly duplicated applet name.
	CameraProbe(Results &results, unsigned index, std::string label)
		: results(results), index(index), label(std::move(label))
	{}

	/// Record reverse destruction after transition cleanup.
	~CameraProbe() override {
		results.cleanupValid &= results.counts[index].activated == results.counts[index].deactivated;
		results.destroyed.push_back(index);
	}


	////
	// Interface: fcg::Applet

	/// Return the configured applet name.
	[[nodiscard]] auto name () const -> const std::string& override { return label; }

	/// Initialize even when another camera is selected.
	void init (fcg::Device &device, fcg::Player &player) override {
		Camera2D::init(device, player);
		++results.counts[index].init;
		results.counts[index].viewport = player.viewportSize();
		results.initialized.push_back(index);
	}

	/// Observe maintenance only for the effective camera.
	void recreatePipelines (fcg::Device&, fcg::Player &player) override {
		record(player, "pipelines");
		++results.counts[index].pipelines;
		if (scenario == "pipeline-throw" && index == 1)
			throw std::runtime_error("pipeline sentinel");
	}

	/// Verify coalesced old dimensions and forward actual cache invalidation.
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldSize, fcg::Player &player) override
	{
		record(player, "resize");
		auto &counts = results.counts[index];
		require(oldSize == counts.viewport, "Camera received an intermediate old viewport");
		counts.viewport = player.viewportSize();
		++counts.resizes;
		Camera2D::onViewportResize(device, oldSize, player);
	}

	/// Observe unconsumed input after both scenes.
	void onEvent (const fcg::Event&, fcg::EventContext&, fcg::Player &player) override {
		record(player, "event");
	}

	/// Observe camera-first GUI delivery without creating widgets that capture test input.
	void gui (fcg::Device&, fcg::Player &player) override
	{ record(player, "gui"); }

	/// Advance the real camera's matrices before the scenes update.
	void update (fcg::Device &device, fcg::Player &player, float dt) override {
		record(player, "update");
		Camera2D::update(device, player, dt);
	}

	/// Install distinguishable matrices before ordinary applets render.
	void render (
		fcg::Device &device, fcg::RenderState &state, SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *command,
		fcg::Player &player
	) override {
		record(player, "render");
		Camera2D::render(device, state, pass, command, player);
	}


	////
	// Interface: fcg::Camera

	/// Observe prepared resources and optionally fail partway through activation.
	void onActivate (fcg::Player &player) override
	{
		record(player, "activate");
		auto &counts = results.counts[index];
		require(counts.pipelines > 0 && counts.viewport == player.viewportSize(), "Activation preceded maintenance");
		++counts.activated;
		if (scenario == "activate-throw" && index == 1)
			throw std::runtime_error("activation sentinel");
		if (scenario == "hook" && index == 1)
			player.setActiveCamera(results.cameras[2]);
	}

	/// Pair activations while the selected camera and player are still valid.
	void onDeactivate (fcg::Player &player) noexcept override {
		results.cleanupValid &= player.activeCamera() == std::optional<fcg::Camera*>(this);
		++results.counts[index].deactivated;
		Camera2D::onDeactivate(player);
	}


private:

	////
	// Methods

	/// Reject any callback delivered to a dormant camera and retain its order.
	void record (fcg::Player &player, const char *phase) {
		require(player.activeCamera() == std::optional<fcg::Camera*>(this), "Dormant camera received a callback");
		results.trace.at(iteration).push_back("c" + std::to_string(index) + "." + phase);
	}


	////
	// Fields

	/// Caller-owned observations.
	Results &results;

	/// Position in camera registration order.
	unsigned index;

	/// Stable applet name, including duplicate and empty names.
	std::string label;
};

/// An ordinary applet controlling the bounded scenario or observing it after the driver.
class SceneProbe final : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Borrow observations and choose the ordinary applet's relative position.
	SceneProbe(Results &results, unsigned index) : results(results), index(index) {}

	/// Retain original reverse ownership order across camera and scene applets.
	~SceneProbe() override { results.destroyed.push_back(index + 4); }


	////
	// Interface: fcg::Applet

	/// Use a camera's name to check that lookup ignores ordinary applets.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string label = "same";
		return label;
	}

	/// Exercise lookup and startup selection before cameras initialize.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		results.initialized.push_back(index + 4);
		if (index)
			return;
		fcg::Player empty(device, nullptr, {});
		require(empty.cameras().empty() && !empty.activeCamera() && !empty.findCamera("same"), "Empty lookup failed");
		if (scenario == "none") {
			require(player.cameras().empty() && !player.activeCamera(), "Camera-free run selected a camera");
			return;
		}
		const auto &readOnly = std::as_const(player);
		require(readOnly.cameras() == std::vector<fcg::Camera*>(results.cameras.begin(), results.cameras.end()),
			"Camera enumeration lost registration order");
		require(readOnly.findCamera("same") == results.cameras[0] && readOnly.findCamera("") == results.cameras[3]
			&& !readOnly.findCamera("absent"), "Camera name lookup failed");
		require(player.activeCamera() == results.cameras[0], "First registered camera was not selected");
		if (scenario == "startup")
			player.setActiveCamera("other");
		if (scenario == "startup-none")
			player.setActiveCamera(nullptr);
	}

	/// Observe camera-first pipeline creation.
	void recreatePipelines (fcg::Device&, fcg::Player&) override { record("pipelines"); }

	/// Observe camera-first viewport maintenance.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override { record("resize"); }

	/// Change selection from input without redirecting the current event to another camera.
	void onEvent (const fcg::Event&, fcg::EventContext &context, fcg::Player &player) override {
		record("event");
		if (scenario == "switch" && !index && iteration == 3)
			player.setActiveCamera("other");
		if (!index && iteration == 4)
			context.markHandled();
	}

	/// Request selection from GUI and check that the effective camera remains unchanged.
	void gui (fcg::Device&, fcg::Player &player) override
	{
		record("gui");
		if (scenario == "switch" && !index && (iteration == 1 || iteration == 10)) {
			const auto before = player.activeCamera();
			player.setActiveCamera(iteration == 1 ? "other" : "");
			require(player.activeCamera() == before, "Selection took effect inside GUI");
		}
	}

	/// Advance selection, invalid-input, coalescing and minimized-window cases.
	void update (fcg::Device&, fcg::Player &player, float) override
	{
		record("update");
		if (index)
			return;
		if (scenario == "switch")
			advance(player);
		else if (scenario == "hook") {
			const unsigned expected = iteration == 1 ? 0 : iteration == 2 ? 1 : 2;
			require(player.activeCamera() == results.cameras[expected], "Hook request applied in the same iteration");
			if (iteration == 1)
				player.setActiveCamera("other");
		} else if (scenario == "activate-throw" || scenario == "pipeline-throw")
			player.setActiveCamera("other");
		else if (scenario == "startup")
			require(player.activeCamera() == results.cameras[1], "Initialization selection was not applied");
		else
			require(!player.activeCamera(), "Explicit or implicit camera-free state was lost");

		// Feed deterministic events while allowing transitions themselves to schedule unavailable iterations
		if (iteration != 5 && iteration != 6) {
			SDL_Event event{};
			event.type = iteration == 2 || iteration == 3 ? SDL_EVENT_DROP_COMPLETE : SDL_EVENT_USER;
			event.drop.windowID = player.mainWindow()->id();
			queuedEvent = event;
		}
		if (iteration == (scenario == "switch" ? 12u : 3u))
			player.requestClose();
	}

	/// Check viewing state and request a switch from rendering.
	void render (
		fcg::Device&, fcg::RenderState &state, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, fcg::Player &player
	) override
	{
		record("render");
		const auto camera = player.activeCamera();
		require(state.modelviewMatrix() == (camera ? (*camera)->modelviewMatrix() : glm::mat4(1)),
			"Scene rendered before the active camera or retained an old view");
		if (scenario == "switch" && !index && iteration == 4)
			player.setActiveCamera(results.cameras[2]);
	}


private:

	////
	// Methods

	/// Record the ordinary applets' relative callback order.
	void record (const char *phase) {
		results.trace.at(iteration).push_back("s" + std::to_string(index) + "." + phase);
	}

	/// Exercise all selection states without relying on wall-clock timing.
	void advance (fcg::Player &player)
	{
		// Match the camera frozen for this iteration, including camera-free and unavailable frames
		constexpr std::array expected{0, 1, 0, 1, 2, -1, 0, 1, 1, 2, 3, -1};
		const int selected = expected.at(iteration - 1);
		require(player.activeCamera() == (selected < 0 ? std::nullopt
			: std::optional(results.cameras[selected])), "Deferred selection used the wrong camera");
		if (iteration == 2) {
			player.setActiveCamera(results.cameras[2]);
			player.setActiveCamera(results.cameras[0]);
			fcg::applet::Camera2D unregistered;
			reject(player, &unregistered);
		}
		if (iteration == 4)
			player.requireSceneRedraw();
		if (iteration == 5 || iteration == 6)
		{
			if (iteration == 5)
				player.setActiveCamera(nullptr);
			else
				player.setActiveCamera("same");
			require(player.setSceneFormat(iteration == 5 ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
				: SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB).has_value(), "Format request failed");
			auto *window = player.mainWindow()->handle();
			require(SDL_SetWindowSize(window, iteration == 5 ? 640 : 660, iteration == 5 ? 360 : 380)
				&& SDL_SyncWindow(window), "Viewport resize failed");
			unavailable = true;
		}
		if (iteration == 7)
			player.setActiveCamera("other");
		if (iteration == 8) {
			player.setActiveCamera(results.cameras[2]);
			player.setActiveCamera("other");
		}
		if (iteration == 9) {
			player.setActiveCamera(results.cameras[2]);
			reject(player, std::string_view("absent"));
		}
		if (iteration == 11)
			player.setActiveCamera(nullptr);
	}

	/// Require invalid requests to fail synchronously without disturbing the queued valid selection.
	template <class T>
	void reject (fcg::Player &player, T invalid) {
		try { player.setActiveCamera(invalid); }
		catch (const std::invalid_argument&) { return; }
		throw std::runtime_error("Invalid camera selection was accepted");
	}


	////
	// Fields

	/// Caller-owned observations.
	Results &results;

	/// Ordinary applet position, zero for the driver.
	unsigned index;
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run one bounded runtime scenario and inspect observations after ownership teardown.
auto main (int argc, char **argv) -> int
{
	try
	{
		require(argc == 2, "Expected a camera selection scenario");
		scenario = argv[1];
		// Interleave camera and ordinary ownership to expose accidental order dependencies
		Results results;
		std::vector<std::unique_ptr<fcg::Applet>> applets;
		applets.push_back(std::make_unique<SceneProbe>(results, 0));
		for (unsigned index = 0; index < 4 && scenario != "none"; ++index)
		{
			auto camera = std::make_unique<CameraProbe>(results, index, index == 1 ? "other" : index == 3 ? "" : "same");
			camera->setEye({float(index), 0, 3});
			results.cameras[index] = camera.get();
			applets.push_back(std::move(camera));
			if (index == 0)
				applets.push_back(std::make_unique<SceneProbe>(results, 1));
		}

		// Retain callback failures and inspect teardown after the owning runtime returns
		std::string failure;
		try { require(fcg::run(std::move(applets), {}) == 0, "Runtime failed"); }
		catch (const std::runtime_error &error) { failure = error.what(); }
		const auto expectedFailure = scenario == "activate-throw" ? "activation sentinel"
			: scenario == "pipeline-throw" ? "pipeline sentinel" : "";
		require(failure == expectedFailure, failure.empty() ? "Expected failure did not propagate" : failure.c_str());
		require(results.cleanupValid, "Camera transition cleanup was unbalanced");
		if (scenario != "none") {
			require(results.initialized == std::vector<unsigned>{4, 0, 5, 1, 2, 3}, "Initialization order changed");
			require(results.destroyed == std::vector<unsigned>{3, 2, 1, 5, 0, 4}, "Destruction order changed");
			for (const auto &counts : results.counts)
				require(counts.init == 1, "Camera was not initialized exactly once");
		}

		// Check resource coalescing and delivery order across all selection phases
		if (scenario == "switch")
		{
			require(results.trace[0].front() == "c0.pipelines", "Startup prepared ordinary pipelines before the camera");
			for (unsigned index = 0; index < 4; ++index) {
				require(results.counts[index].pipelines == (index == 3 ? 1u : 2u), "Dormant pipeline changes did not coalesce");
				require(results.counts[index].resizes == 1, "Dormant viewport changes did not coalesce");
			}
			require(results.counts[1].activated == 3, "Selecting the current camera caused reactivation");
			for (unsigned frame = 1; frame <= 12; ++frame)
			{
				// Extract the independently ordered input and GUI/update phases
				const auto &trace = results.trace[frame];
				std::vector<std::string> updates, events;
				for (const auto &entry : trace) {
					if (entry.ends_with("gui") || entry.ends_with("update"))
						updates.push_back(entry);
					if (entry.ends_with("event"))
						events.push_back(entry);
				}

				// Require the same effective camera throughout each iteration
				constexpr std::array cameras{0, 1, 0, 1, 2, -1, 0, 1, 1, 2, 3, -1};
				std::vector<std::string> expected;
				if (cameras[frame - 1] >= 0) {
					const auto prefix = "c" + std::to_string(cameras[frame - 1]);
					expected = {prefix + ".gui", prefix + ".update"};
				}
				expected.insert(expected.end(), {"s0.gui", "s0.update", "s1.gui", "s1.update"});
				require(updates == expected, "GUI/update order violated camera priority");
				for (const std::string phase : {"pipelines", "resize", "render"})
					for (const auto &entry : trace)
						if (entry.ends_with(phase)) {
							require(entry.front() == (cameras[frame - 1] < 0 ? 's' : 'c'),
								"Maintenance or rendering delivered to ordinary applets before the camera");
							break;
						}
				if (frame == 3)
					require(events == std::vector<std::string>{"s0.event", "s1.event", "c0.event"}, "Input order changed during selection");
				if (frame == 4)
					require(events == std::vector<std::string>{"s0.event"}, "Consumed input reached a camera");
			}
		}
		return 0;
	}
	catch (const std::exception &error) {
		std::cerr << "Camera selection: " << error.what() << " at iteration " << iteration << '\n';
		return 1;
	}
}
