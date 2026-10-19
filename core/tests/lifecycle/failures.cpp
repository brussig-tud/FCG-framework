
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui library
#include <imgui.h>

// FCG Framework
#include <FCG/device.h>
#include <FCG/frame.h>
#include <FCG/player.h>
#include <FCG/run.h>
#include <FCG/window.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Selected subprocess scenario.
std::string scenario;

/// Whether the next frame submission should report failure after consuming its command buffer.
bool failSubmission = false;

/// Number of frame submissions, excluding resource uploads made by the library.
unsigned submissions = 0;

/// Submission diagnostics captured from the real SDL logger.
std::vector<std::string> submissionLogs;

/// Original SDL log output forwarded by the observer.
SDL_LogOutputFunction originalLog = nullptr;

/// User data of the original SDL log output.
void *originalLogData = nullptr;

/// Fail the test with the given diagnostic.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Consume real GPU work, then inject a recoverable backend failure at the frame boundary.
auto submitFrame (SDL_GPUCommandBuffer *command) -> bool {
	++submissions;
	const bool submitted = SDL_SubmitGPUCommandBuffer(command);
	if (std::exchange(failSubmission, false))
		return SDL_SetError("submission sentinel");
	return submitted;
}

/// Inject an SDL startup failure without modifying the process environment.
auto initializeSDL (SDL_InitFlags flags) -> bool {
	if (scenario == "startup")
		return SDL_SetError("startup sentinel");
	return SDL_Init(flags);
}

/// Inject a failure after device creation but before a runtime window exists.
auto createWindow (const char *title, int width, int height, SDL_WindowFlags flags) -> SDL_Window* {
	if (scenario == "window") {
		SDL_SetError("window sentinel");
		return nullptr;
	}
	return SDL_CreateWindow(title, width, height, flags);
}

/// Observe production diagnostics while retaining validation output for CTest.
void SDLCALL observeLog (void*, int category, SDL_LogPriority priority, const char *message) {
	if (std::string(message).starts_with("Submitting the GPU command buffer failed:"))
		submissionLogs.emplace_back(message);
	if (originalLog)
		originalLog(originalLogData, category, priority, message);
}

// Anonymous namespace end
}



//////
//
// Includes
//

// Compile the production implementations with only these SDL boundaries substituted. The rest of Core, including
// its GPU resources, GUI and player, is linked normally. This also exercises the sources with shared dependencies
// without introducing test entry points into the public API.
#define SDL_SubmitGPUCommandBuffer submitFrame
#include "../../src/frame.cpp"
#undef SDL_SubmitGPUCommandBuffer
#define SDL_CreateWindow createWindow
#include "../../src/window.cpp"
#undef SDL_CreateWindow
#define SDL_Init initializeSDL
#include "../../src/run.cpp"
#undef SDL_Init



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Observations retained after runtime ownership has been released.
struct Results
{
	/// Applet destruction indices in observed order.
	std::vector<unsigned> destroyed;

	/// Runtime player borrowed until all applets have been destroyed.
	fcg::Player *player = nullptr;

	/// Runtime device borrowed until all applets have been destroyed.
	fcg::Device *device = nullptr;

	/// GUI context that must stay current throughout applet destruction.
	ImGuiContext *context = nullptr;

	/// Whether all lifetime checks succeeded.
	bool valid = true;

	/// Whether presentation captures were released before applet destruction.
	bool capturesReleased = false;

	/// Whether an active-pass logic error left the frame usable.
	bool logicChecked = false;
};

/// Distinct exception used to check unchanged propagation from callbacks.
class ProbeFailure final : public std::runtime_error
{
public:

	////
	// Object construction

	/// Construct the callback sentinel.
	ProbeFailure() : std::runtime_error("callback sentinel") {}
};

/// Probe applet used at three positions to check partial initialization and reverse destruction.
class Probe final : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Borrow observations and retain this applet's position.
	Probe(Results &results, unsigned index) : results(results), index(index) {}

	/// Check that presentation, GUI, player and device lifetimes meet the public contract.
	~Probe() override
	{
		if (results.player)
		{
			results.valid = results.valid && results.capturesReleased
				&& ImGui::GetCurrentContext() == results.context && results.device->handle()
				&& results.player->viewportSize().x > 0;
			if (ImGui::GetCurrentContext())
				(void)ImGui::GetIO();
		}
		results.destroyed.push_back(index);
	}


	////
	// Interface: fcg::Applet

	/// Return a stable name without consulting teardown state.
	[[nodiscard]] auto name () const -> const std::string& override {
		fail("name");
		static const std::string label = "Failure contracts";
		return label;
	}

	/// Register a real presentation pass and arrange input or resizing before the first frame.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		if (!index)
		{
			results.player = &player;
			results.device = &device;
			results.context = ImGui::GetCurrentContext();
			player.pushContinuousRedraw();
			auto pass = fcg::FullscreenPass::linearToSRGB(device, player.swapchainFormat());
			require(pass.has_value(), "Could not create probe presentation pass");
			auto release = [&] (Results *observed) {
				observed->valid = observed->valid && observed->destroyed.empty()
					&& ImGui::GetCurrentContext() == observed->context && device.handle();
				observed->capturesReleased = true;
			};
			std::unique_ptr<Results, decltype(release)> capture(&results, release);
			player.setPresentationRecorder([pass = std::move(*pass), capture = std::move(capture)]
				(const fcg::PresentationContext &context) -> std::expected<void, fcg::FullscreenError> {
				if (scenario == "presentation")
					throw ProbeFailure();
				const fcg::FullscreenTextureBinding input{&context.scene, &context.sceneSampler};
				return pass.record(context.cmdBuffer, context.target, {.samplers = {&input, 1}});
			});
			if (scenario == "event" || scenario == "close-submit")
			{
				SDL_Event event{};
				event.type = SDL_EVENT_KEY_DOWN;
				event.key.windowID = player.mainWindow()->id();
				event.key.key = SDLK_SPACE;
				require(SDL_PushEvent(&event), "Could not queue input");
			}
			if (scenario == "resize") {
				const auto extent = player.viewportSize();
				require(SDL_SetWindowSize(player.mainWindow()->handle(), (int)extent.x + 64, (int)extent.y + 32),
					"Could not resize probe window");
				SDL_SyncWindow(player.mainWindow()->handle());
			}
		}
		fail("init");
	}

	/// Fail during input dispatch with an acquired frame.
	void onEvent (const fcg::Event &event, fcg::EventContext&, fcg::Player &player) override
	{
		if (!index && scenario == "close-submit") {
			failSubmission = true;
			player.requestClose();
		}
		if (event.type() == fcg::EventType::KeyDown)
			fail("event");
	}

	/// Fail before input dispatch after acquiring a resized frame.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {
		fail("resize");
	}

	/// Fail during GUI construction or during an ImGui overlay draw callback.
	void gui (fcg::Device&, fcg::Player&) override
	{
		fail("gui");
		if (!index && scenario == "overlay") {
			auto *draw = ImGui::GetForegroundDrawList();
			draw->AddRectFilled(ImVec2(0, 0), ImVec2(8, 8), IM_COL32_WHITE);
			draw->AddCallback([] (const ImDrawList*, const ImDrawCmd*) { throw ProbeFailure(); }, nullptr);
		}
	}

	/// Close after this iteration, unless a preceding callback fails first.
	void update (fcg::Device&, fcg::Player &player, float) override {
		fail("update");
		if (!index)
			player.requestClose();
	}

	/// Exercise active-pass rejection and failures before scene-pass cleanup.
	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, fcg::Player &player
	) override
	{
		if (!index && (scenario == "submit" || scenario == "render-submit"))
			failSubmission = true;
		if (!index && scenario == "logic")
		{
			try {
				player.mainWindow()->endFrame();
				throw std::runtime_error("Active render pass was accepted");
			} catch (const std::logic_error&) {
				results.logicChecked = true;
			}
		}
		fail("render");
		if (scenario == "render-submit" && index == 1)
			throw ProbeFailure();
	}


private:

	////
	// Methods

	/// Fail only at the middle applet, leaving a later applet to test teardown ordering.
	void fail (const char *stage) const {
		if (index == 1 && scenario == stage)
			throw ProbeFailure();
	}


	////
	// Fields

	/// Observations owned by the test's main function.
	Results &results;

	/// Position in callback and destruction order.
	unsigned index;
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run one isolated failure scenario against real GPU and GUI lifetimes.
auto main (int argc, char **argv) -> int
{
	try
	{
		require(argc == 2, "Expected a failure scenario");
		scenario = argv[1];
		SDL_GetLogOutputFunction(&originalLog, &originalLogData);
		SDL_SetLogOutputFunction(observeLog, nullptr);
		Results results;
		results.destroyed.reserve(3);
		std::vector<std::unique_ptr<fcg::Applet>> applets;
		for (unsigned i = 0; i < 3; ++i)
			applets.push_back(std::make_unique<Probe>(results, i));
		bool callbackFailed = false;
		bool submissionFailed = false;
		int status = -1;
		try {
			status = fcg::run(std::move(applets));
		} catch (const ProbeFailure &error) {
			require(std::string(error.what()) == "callback sentinel", "Changed callback exception");
			callbackFailed = true;
		} catch (const std::runtime_error &error) {
			require(scenario == "submit" || scenario == "close-submit", error.what());
			require(submissionLogs == std::vector<std::string>{error.what()}, "Log and exception differ");
			submissionFailed = true;
		}
		require(results.destroyed == std::vector<unsigned>{2, 1, 0}, "Applets were not destroyed in reverse order");
		require(results.valid, "Applet or presentation teardown lost its required dependencies");
		require(SDL_WasInit(0) == 0 && !ImGui::GetCurrentContext(), "Runtime did not release SDL and GUI");
		if (scenario == "startup" || scenario == "window")
			require(status == EXIT_FAILURE && !results.player && !submissions, "Startup failure invoked callbacks");
		else if (scenario == "normal" || scenario == "logic")
			require(status == EXIT_SUCCESS && submissions == 1, "Normal close did not finish exactly one frame");
		else if (scenario == "submit" || scenario == "close-submit")
			require(submissionFailed && submissions == 1, "Failed submission was retried or suppressed");
		else
			require(callbackFailed && submissions == (scenario == "init" || scenario == "name" ? 0u : 1u),
				"Callback exception was lost");
		if (scenario == "render-submit")
			require(submissionLogs.size() == 1 && callbackFailed, "Secondary failure replaced the callback exception");
		if (scenario == "logic")
			require(results.logicChecked, "Active-pass rejection was not exercised");
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
