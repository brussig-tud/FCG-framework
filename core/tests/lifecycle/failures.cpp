
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui library
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

// Framework implementation helpers (before substituting ImGui entry points)
#include "../../src/gui_scaling.h"

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

/// Caller-owned context that must survive the runtime.
ImGuiContext *callerContext = nullptr;

/// Context acquired by the production GUI factory.
ImGuiContext *createdContext = nullptr;

/// Number of attempts to create the framework context.
unsigned contextCreationAttempts = 0;

/// Number of owned contexts released by production cleanup.
unsigned contextDestructions = 0;

/// Number of successful platform backend initializations.
unsigned platformInitializations = 0;

/// Number of successful renderer backend initializations.
unsigned rendererInitializations = 0;

/// Number of platform backend shutdowns.
unsigned platformShutdowns = 0;

/// Number of renderer backend shutdowns.
unsigned rendererShutdowns = 0;

/// Whether cleanup selected the owned context and released its backends before destruction.
bool guiCleanupValid = true;

/// Whether the next frame submission should report failure after consuming its command buffer.
bool failSubmission = false;

/// Whether the next frame acquisition should succeed without an image.
bool unavailableImage = false;

/// Whether the next swapchain acquisition should fail before acquiring an image.
bool failAcquisition = false;

/// Whether the next window attachment allocation should fail after acquiring an image.
bool failAttachment = false;

/// Whether the next cancellation should report failure after consuming its command buffer.
bool failCancellation = false;

/// Number of frame submissions, excluding resource uploads made by the library.
unsigned submissions = 0;

/// Number of acquisition command buffers cancelled.
unsigned cancellations = 0;

/// Submission diagnostics captured from the real SDL logger.
std::vector<std::string> submissionLogs;

/// Acquisition and secondary cleanup diagnostics captured in emission order.
std::vector<std::string> acquisitionLogs;

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

/// Cancel real commands, then optionally report a backend failure without permitting a retry.
auto cancelFrame (SDL_GPUCommandBuffer *command) -> bool {
	++cancellations;
	const bool cancelled = SDL_CancelGPUCommandBuffer(command);
	if (std::exchange(failCancellation, false))
		return SDL_SetError("cancellation sentinel");
	return cancelled;
}

/// Inject temporary unavailability or an acquisition error without acquiring a real swapchain image.
auto acquireImage (
	SDL_GPUCommandBuffer *command, SDL_Window *window, SDL_GPUTexture **texture, Uint32 *width, Uint32 *height
) -> bool
{
	if (std::exchange(unavailableImage, false)) {
		*texture = nullptr;
		*width = *height = 0;
		return true;
	}
	if (std::exchange(failAcquisition, false))
		return SDL_SetError("acquisition sentinel");
	return SDL_WaitAndAcquireGPUSwapchainTexture(command, window, texture, width, height);
}

/// Fail attachment creation only after the window has acquired a real image.
auto createAttachment (SDL_GPUDevice *device, const SDL_GPUTextureCreateInfo *info) -> SDL_GPUTexture*
{
	if (std::exchange(failAttachment, false)) {
		SDL_SetError("attachment sentinel");
		return nullptr;
	}
	return SDL_CreateGPUTexture(device, info);
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
void SDLCALL observeLog (void*, int category, SDL_LogPriority priority, const char *message)
{
	if (std::string(message).starts_with("Submitting the GPU command buffer failed:"))
		submissionLogs.emplace_back(message);
	if (std::string(message).starts_with("Acquiring the swapchain texture failed:")
		|| std::string(message).starts_with("Creating the depth buffer failed:")
		|| std::string(message).starts_with("Cancelling the unused frame command buffer failed:")
		|| std::string(message).starts_with("Cleaning up frame acquisition by "))
		acquisitionLogs.emplace_back(message);
	if (originalLog)
		originalLog(originalLogData, category, priority, message);
}

/// Fail before platform initialization or initialize the real backend.
auto initializeGuiPlatform (SDL_Window *window) -> bool {
	if (scenario == "gui-platform")
		return false;
	const bool initialized = ImGui_ImplSDL3_InitForSDLGPU(window);
	platformInitializations += initialized;
	return initialized;
}

/// Inject a failure before renderer initialization or an allocation exception after it succeeds.
auto initializeGuiRenderer (ImGui_ImplSDLGPU3_InitInfo *info) -> bool
{
	if (scenario == "gui-renderer")
		return false;
	const bool initialized = ImGui_ImplSDLGPU3_Init(info);
	rendererInitializations += initialized;
	if (initialized && scenario == "gui-allocation")
		throw std::bad_alloc();
	return initialized;
}

/// Observe platform shutdown on the owned context after renderer shutdown.
void shutdownGuiPlatform () {
	guiCleanupValid &= ImGui::GetCurrentContext() == createdContext && !ImGui::GetIO().BackendRendererUserData;
	++platformShutdowns;
	ImGui_ImplSDL3_Shutdown();
}

/// Observe renderer shutdown on the owned context.
void shutdownGuiRenderer () {
	guiCleanupValid &= ImGui::GetCurrentContext() == createdContext;
	++rendererShutdowns;
	ImGui_ImplSDLGPU3_Shutdown();
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// Dear ImGui namespace, for test substitutes of qualified production calls.
namespace ImGui {



//////
//
// Functions
//

/// Acquire the real GUI context, or report an initialization failure without acquiring one.
auto createGuiContext (ImFontAtlas *atlas = nullptr) -> ImGuiContext* {
	++contextCreationAttempts;
	if (scenario == "gui-context")
		return nullptr;
	createdContext = CreateContext(atlas);
	return createdContext;
}

/// Verify explicit ownership and backend cleanup before destroying the real GUI context.
void destroyGuiContext (ImGuiContext *context)
{
	guiCleanupValid &= context && context == createdContext && context != callerContext
		&& GetCurrentContext() == context;
	if (context)
		guiCleanupValid &= !GetIO().BackendRendererUserData && !GetIO().BackendPlatformUserData;
	++contextDestructions;
	DestroyContext(context);
}



//////
//
// Module namespace close
//

} // namespace ImGui



//////
//
// Includes
//

// Compile the production implementations with narrow SDL and ImGui boundaries substituted. The rest of Core,
// including its GPU resources and player, is linked normally. This also exercises the sources with shared dependencies
// without introducing test entry points into the public API.
#define SDL_SubmitGPUCommandBuffer submitFrame
#include "../../src/frame.cpp"
#define SDL_CreateWindow createWindow
#define SDL_CancelGPUCommandBuffer cancelFrame
#define SDL_WaitAndAcquireGPUSwapchainTexture acquireImage
#define SDL_CreateGPUTexture createAttachment
#include "../../src/window.cpp"
#undef SDL_CreateGPUTexture
#undef SDL_WaitAndAcquireGPUSwapchainTexture
#undef SDL_CancelGPUCommandBuffer
#undef SDL_CreateWindow
#undef SDL_SubmitGPUCommandBuffer
#define SDL_Init initializeSDL
#include "../../src/run.cpp"
#undef SDL_Init
#define CreateContext createGuiContext
#define DestroyContext destroyGuiContext
#define ImGui_ImplSDL3_InitForSDLGPU initializeGuiPlatform
#define ImGui_ImplSDLGPU3_Init initializeGuiRenderer
#define ImGui_ImplSDL3_Shutdown shutdownGuiPlatform
#define ImGui_ImplSDLGPU3_Shutdown shutdownGuiRenderer
#include "../../src/gui.cpp"
#undef ImGui_ImplSDLGPU3_Shutdown
#undef ImGui_ImplSDL3_Shutdown
#undef ImGui_ImplSDLGPU3_Init
#undef ImGui_ImplSDL3_InitForSDLGPU
#undef DestroyContext
#undef CreateContext



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

	/// Whether direct frame acquisition returned ordinary unavailability without starting a frame.
	bool unavailableChecked = false;
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
			require(results.context && results.context == createdContext && results.context != callerContext,
				"Runtime did not select its own GUI context");
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
			if (scenario == "no-image" || scenario == "no-image-cancel" || scenario == "acquire"
				|| scenario == "acquire-cancel" || scenario == "attachment" || scenario == "attachment-submit")
			{
				unavailableImage = scenario.starts_with("no-image");
				failAcquisition = scenario.starts_with("acquire");
				failAttachment = scenario.starts_with("attachment");
				failCancellation = scenario.ends_with("-cancel");
				failSubmission = scenario.ends_with("-submit");
				auto *frame = player.mainWindow()->beginFrame(device);
				if (frame)
					player.mainWindow()->endFrame();
				require(!frame && scenario == "no-image", "Acquisition failure was not distinguished from unavailability");
				results.unavailableChecked = true;
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
		require(argc == 2 || (argc == 3 && std::string(argv[2]) == "previous-context"),
			"Expected a failure scenario and optional previous-context mode");
		scenario = argv[1];
		std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> previous(
			argc == 3 ? ImGui::CreateContext() : nullptr, &ImGui::DestroyContext
		);
		callerContext = previous.get();
		require(argc == 2 || callerContext, "Could not create caller context");
		ImFont *callerFont = nullptr;
		if (callerContext)
		{
			auto &io = ImGui::GetIO();
			io.IniFilename = nullptr;
			io.ConfigFlags = ImGuiConfigFlags_NavEnableGamepad;
			io.UserData = &scenario;
			callerFont = io.Fonts->AddFontDefaultBitmap();
			require(callerFont, "Could not create caller font");
			io.FontDefault = callerFont;
			ImGui::GetStyle().Alpha = .375f;
		}
		SDL_GetLogOutputFunction(&originalLog, &originalLogData);
		SDL_SetLogOutputFunction(observeLog, nullptr);
		Results results;
		results.destroyed.reserve(3);
		std::vector<std::unique_ptr<fcg::Applet>> applets;
		for (unsigned i = 0; i < 3; ++i)
			applets.push_back(std::make_unique<Probe>(results, i));
		bool callbackFailed = false;
		bool submissionFailed = false;
		bool allocationFailed = false;
		bool acquisitionFailed = false;
		int status = -1;
		try {
			status = fcg::run(std::move(applets));
		} catch (const ProbeFailure &error) {
			require(std::string(error.what()) == "callback sentinel", "Changed callback exception");
			callbackFailed = true;
		} catch (const std::bad_alloc&) {
			require(scenario == "gui-allocation", "Unexpected allocation exception");
			allocationFailed = true;
		} catch (const std::runtime_error &error) {
			if (scenario == "submit" || scenario == "close-submit") {
				require(submissionLogs == std::vector<std::string>{error.what()}, "Log and exception differ");
				submissionFailed = true;
			}
			else
			{
				require(scenario == "no-image-cancel" || scenario == "acquire" || scenario == "acquire-cancel"
					|| scenario == "attachment" || scenario == "attachment-submit", error.what());
				const std::string expected = scenario == "no-image-cancel"
					? "Cancelling the unused frame command buffer failed: cancellation sentinel"
					: scenario.starts_with("acquire")
						? "Acquiring the swapchain texture failed: acquisition sentinel"
						: "Creating the depth buffer failed: attachment sentinel";
				require(error.what() == expected && !acquisitionLogs.empty() && acquisitionLogs.front() == expected,
					"Acquisition exception changed or differed from its log");
				acquisitionFailed = true;
			}
		}
		require(results.destroyed == std::vector<unsigned>{2, 1, 0}, "Applets were not destroyed in reverse order");
		require(results.valid, "Applet or presentation teardown lost its required dependencies");
		require(SDL_WasInit(0) == 0, "Runtime did not release SDL");
		require(ImGui::GetCurrentContext() == callerContext, "Runtime did not restore the previous context");
		const bool reachesGui = scenario != "startup" && scenario != "window";
		require(contextCreationAttempts == (reachesGui ? 1u : 0u)
			&& bool(createdContext) == (reachesGui && scenario != "gui-context"),
			"Scenario did not reach the expected GUI initialization stage");
		require(guiCleanupValid && contextDestructions == (createdContext ? 1u : 0u),
			"GUI did not destroy exactly its owned context after backend cleanup");
		require(platformShutdowns == platformInitializations && rendererShutdowns == rendererInitializations,
			"GUI did not shut down each initialized backend exactly once");
		if (callerContext)
		{
			const auto &io = ImGui::GetIO();
			require(io.ConfigFlags == ImGuiConfigFlags_NavEnableGamepad && io.UserData == &scenario
				&& !io.IniFilename && io.FontDefault == callerFont && io.Fonts->Fonts.Size == 1
				&& io.Fonts->Fonts[0] == callerFont && ImGui::GetStyle().Alpha == .375f
				&& !io.BackendPlatformUserData && !io.BackendRendererUserData,
				"Runtime modified the caller's context");
		}
		if (scenario == "startup" || scenario == "window" || scenario == "gui-context"
			|| scenario == "gui-platform" || scenario == "gui-renderer")
			require(status == EXIT_FAILURE && !results.player && !submissions, "Startup failure invoked callbacks");
		else if (scenario == "gui-allocation")
			require(allocationFailed && !results.player && !submissions && rendererInitializations == 1,
				"GUI allocation exception was lost or invoked callbacks");
		else if (scenario == "normal" || scenario == "logic" || scenario == "no-image")
			require(status == EXIT_SUCCESS && submissions == 1, "Normal close did not finish exactly one frame");
		else if (scenario == "submit" || scenario == "close-submit")
			require(submissionFailed && submissions == 1, "Failed submission was retried or suppressed");
		else if (scenario == "no-image-cancel" || scenario == "acquire" || scenario == "acquire-cancel"
			|| scenario == "attachment" || scenario == "attachment-submit")
			require(acquisitionFailed && submissions == (scenario.starts_with("attachment") ? 1u : 0u),
				"Failed acquisition did not finish its commands exactly once");
		else
			require(callbackFailed && submissions == (scenario == "init" || scenario == "name" ? 0u : 1u),
				"Callback exception was lost");
		if (scenario == "render-submit")
			require(submissionLogs.size() == 1 && callbackFailed, "Secondary failure replaced the callback exception");
		if (scenario == "logic")
			require(results.logicChecked, "Active-pass rejection was not exercised");
		if (scenario == "no-image")
			require(results.unavailableChecked && acquisitionLogs.empty(), "Unavailable frame was not returned cleanly");
		const bool cancelledAcquisition = scenario.starts_with("no-image") || scenario.starts_with("acquire");
		require(cancellations == (cancelledAcquisition ? 1u : 0u), "Acquisition cancellation was missing or repeated");
		if (acquisitionFailed)
		{
			const bool secondaryFailure = scenario == "acquire-cancel" || scenario == "attachment-submit";
			require(acquisitionLogs.size() == (secondaryFailure ? 2u : 1u), "Acquisition diagnostics were missing or repeated");
			if (secondaryFailure) {
				const std::string expected = scenario == "acquire-cancel"
					? "Cleaning up frame acquisition by cancellation failed: cancellation sentinel"
					: "Cleaning up frame acquisition by submission failed: submission sentinel";
				require(acquisitionLogs.back() == expected, "Secondary cleanup failure was not logged correctly");
			}
		}
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
