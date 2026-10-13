
//////
//
// Includes
//

// C++ STL
#include <array>
#include <expected>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <type_traits>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui
#include <imgui.h>

// FCG Framework
#include <FCG/device.h>
#include <FCG/fullscreen.h>
#include <FCG/player.h>
#include <FCG/frame.h>
#include <FCG/run.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

static_assert(std::is_same_v<fcg::PresentationRecorder,
	fcg::MoveOnlyFunction<std::expected<void, fcg::FullscreenError>(const fcg::PresentationContext &)>>);
static_assert(!std::is_constructible_v<fcg::PresentationRecorder, decltype([] (const fcg::PresentationContext &) {})>);
static_assert(std::is_same_v<
	std::invoke_result_t<fcg::PresentationRecorder &, const fcg::PresentationContext &>,
	std::expected<void, fcg::FullscreenError>>);
static_assert(!std::is_invocable_v<const fcg::PresentationRecorder &, const fcg::PresentationContext &>);

/// Fail a probe without depending on assertions being enabled.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Counters retained after applet and capture destruction.
struct Results
{
	/// Recorder invocations for the initial and replacement registrations.
	std::array<unsigned, 2> calls{};

	/// Exactly-once destruction counts for owned captures.
	std::array<unsigned, 2> destroyed{};

	/// Number of scene renders.
	unsigned renders = 0;

	/// Number of actual ImGui backend callback executions.
	unsigned overlays = 0;

	/// Number of scene frames with drawable ImGui output.
	unsigned drawableOverlays = 0;

	/// Number of rejected recorder mutations.
	unsigned rejected = 0;

	/// Whether every capture was released while its applet was alive.
	bool ordered = true;

	/// Whether the applet has been destroyed.
	bool ended = false;

	/// Whether a resized context was recorded.
	bool resized = false;

	/// Whether a minimized iteration skipped scene rendering and presentation.
	bool noImage = false;
};

/// Owned state whose destruction must precede applet destruction.
class Capture
{
public:
	////
	// Object construction/destruction

	/// Retain caller-owned counters and the registration index.
	Capture(Results &results, unsigned index) : results(results), index(index) {}

	/// Check teardown ordering and count capture destruction.
	~Capture() {
		results.ordered = results.ordered && !results.ended;
		++results.destroyed[index];
	}


private:

	////
	// Fields

	/// Counters that outlive the recorder and applet.
	Results &results;

	/// Registration whose lifetime is being measured.
	unsigned index;
};

/// Distinguishable exception that must propagate unchanged from the recorder.
class PresentationFailure final : public std::runtime_error
{
public:
	////
	// Object construction

	/// Supply the expected diagnostic.
	PresentationFailure() : std::runtime_error("presentation throw sentinel") {}
};

/// Exercise registration, dispatch and teardown through the real runtime.
class PresentationProbe final : public fcg::Applet
{
public:
	////
	// Object construction/destruction

	/// Store the selected scenario and caller-owned counters.
	PresentationProbe(Results &results, std::string scenario)
		: results(results), scenario(std::move(scenario)) {}

	/// Mark applet destruction after checking capture release.
	~PresentationProbe() override
	{
		results.ordered = results.ordered && results.destroyed[0] == (scenario == "default" ? 0u : 1u);
		if (scenario == "lifecycle" || scenario == "empty")
			results.ordered = results.ordered && results.destroyed[1] == 1;
		results.ended = true;
	}


	////
	// Interface: fcg::Applet

	/// Return the scenario name for runtime diagnostics.
	[[nodiscard]] auto name () const -> const std::string& override { return scenario; }

	/// Install a move-only pass and optionally fail after registration.
	void init (fcg::Device&, fcg::Player &player) override
	{
		int count = 0;
		auto **windows = SDL_GetWindows(&count);
		if (windows && count == 1)
			window = windows[0];
		SDL_free(windows);
		require(window, "Expected one runtime window");
		initialExtent = player.viewportSize();
		player.pushContinuousRedraw();
		if (scenario != "default")
			install(player, 0);
		if (scenario == "close")
		{
			SDL_Event event{};
			event.type = SDL_EVENT_KEY_DOWN;
			event.key.windowID = SDL_GetWindowID(window);
			event.key.key = SDLK_SPACE;
			require(SDL_PushEvent(&event), "Could not queue early-close event");
		}
		if (scenario == "init")
			throw PresentationFailure();
	}

	/// Close from event dispatch, after acquisition but before scene rendering, in the early-exit scenario.
	void onEvent (const fcg::Event&, fcg::EventContext&, fcg::Player &player) override {
		if (scenario == "close")
			player.requestClose();
	}

	/// Track the current viewport announced before presentation.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player &player) override {
		notifiedExtent = player.viewportSize();
	}

	/// Add a backend callback to distinguish GUI construction from actual rendering.
	void gui (fcg::Device&, fcg::Player&) override
	{
		auto *draw = ImGui::GetForegroundDrawList();
		draw->AddRectFilled(ImVec2(0, 0), ImVec2(8, 8), IM_COL32_WHITE);
		draw->AddCallback([] (const ImDrawList*, const ImDrawCmd *command) {
			++static_cast<Results*>(command->UserCallbackData)->overlays;
		}, &results);
	}

	/// Replace and reset on frame boundaries, and bound every scenario.
	void update (fcg::Device&, fcg::Player &player, float) override
	{
		++updates;
		if (scenario == "lifecycle" || scenario == "empty")
		{
			if (phase == 0 && results.calls[0] >= 3) {
				install(player, 1);
				require(results.destroyed[0] == 1, "Replacement did not destroy initial capture");
				phase = 1;
			}
			else if (phase == 1 && results.calls[1] >= 3) {
				require(SDL_SetWindowSize(window, 480, 320), "Could not resize presentation window");
				phase = 2;
			}
			else if (phase == 2 && results.resized)
			{
				if (scenario == "empty")
					player.setPresentationRecorder({});
				else
					player.clearPresentationRecorder();
				require(results.destroyed[1] == 1, "Reset did not destroy replacement capture");
				resetRenders = results.renders;
				phase = 3;
			}
			else if (phase == 3 && results.renders >= resetRenders + 3)
				player.requestClose();
		}
		else if (scenario == "minimized")
		{
			if (updates == 4)
				minimizeAccepted = SDL_MinimizeWindow(window);
			if (updates > 5 && updates < 20)
			{
				if (minimizeAccepted && (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED))
					if (results.renders == previousRenders) {
						require(results.calls[0] == previousCalls, "Presentation ran without a scene frame");
						results.noImage = true;
					}
				SDL_Delay(10);
			}
			if (updates == 20)
				require(SDL_RestoreWindow(window), "Could not restore minimized window");
			if (updates == 25)
				player.requestClose();
			previousRenders = results.renders;
			previousCalls = results.calls[0];
		}
		else if (scenario == "default" && results.renders >= 3)
			player.requestClose();
		else if (scenario == "mutation" && results.calls[0] >= 3)
			player.requestClose();
		if (updates > 120)
			throw std::runtime_error("Presentation probe exceeded iteration bound");
	}

	/// Count scene rendering before the presentation recorder.
	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, fcg::Player&
	) override
	{
		++results.renders;
		const auto *draw = ImGui::GetDrawData();
		if (draw && int(draw->DisplaySize.x * draw->FramebufferScale.x) > 0
			&& int(draw->DisplaySize.y * draw->FramebufferScale.y) > 0)
			++results.drawableOverlays;
	}


private:

	////
	// Methods

	/// Install a mutable lambda owning both a GPU pass and a unique capture.
	void install (fcg::Player &player, unsigned index)
	{
		auto pass = fcg::FullscreenPass::linearToSRGB(player.device(), player.swapchainFormat());
		if (!pass)
			throw std::runtime_error(pass.error().message);
		player.setPresentationRecorder([
			this, &player, index, pass = std::move(*pass), owned = std::make_unique<Capture>(results, index), count = 0u
		] (const fcg::PresentationContext &context) mutable -> std::expected<void, fcg::FullscreenError>
		{
			require(++count == ++results.calls[index], "Mutable capture state was not preserved");
			require(results.renders == results.calls[0] + results.calls[1], "Presentation preceded scene rendering");
			require(context.cmdBuffer && context.target.color.texture, "Missing presentation handles");
			require(context.target.device == &player.device(), "Wrong target device");
			require(context.target.format == player.swapchainFormat(), "Wrong target format");
			require(context.scene.device() == &player.device(), "Wrong scene device");
			require(context.sceneSampler.device() == &player.device(), "Wrong sampler device");
			require(context.target.extent == player.viewportSize(), "Stale target extent");
			require(context.scene.info().width == context.target.extent.x
				&& context.scene.info().height == context.target.extent.y, "Stale scene metadata");
			require(context.target.color.load_op == SDL_GPU_LOADOP_DONT_CARE
				&& context.target.color.store_op == SDL_GPU_STOREOP_STORE, "Wrong target operations");
			if (context.target.extent != initialExtent) {
				require(context.target.extent == notifiedExtent, "Resize notification disagrees with context");
				results.resized = true;
			}
			if (scenario == "error")
				return std::unexpected(fcg::FullscreenError{
					fcg::FullscreenErrorCode::InvalidState, "presentation error sentinel"
				});
			if (scenario == "throw")
				throw PresentationFailure();
			if (scenario == "mutation")
			{
				try { player.clearPresentationRecorder(); }
				catch (const std::logic_error&) { ++results.rejected; }
				try { player.setPresentationRecorder({}); }
				catch (const std::logic_error&) { ++results.rejected; }
				require(results.rejected == count * 2, "Self-mutation was not rejected");
				require(results.destroyed[0] == 0, "Self-mutation destroyed executing capture");
			}
			const std::array bindings{fcg::FullscreenTextureBinding{&context.scene, &context.sceneSampler}};
			return pass.record(context.cmdBuffer, context.target, {.samplers = bindings});
		});
	}


	////
	// Fields

	/// Caller-owned counters.
	Results &results;

	/// Selected runtime scenario.
	std::string scenario;

	/// Borrowed runtime window.
	SDL_Window *window = nullptr;

	/// Initial drawable extent.
	glm::uvec2 initialExtent{0};

	/// Last announced resized extent.
	glm::uvec2 notifiedExtent{0};

	/// Iteration counter enforcing bounded execution.
	unsigned updates = 0;

	/// Replacement, resize and reset state.
	unsigned phase = 0;

	/// Scene count at reset.
	unsigned resetRenders = 0;

	/// Scene count at the preceding minimized iteration.
	unsigned previousRenders = 0;

	/// Presentation count at the preceding minimized iteration.
	unsigned previousCalls = 0;

	/// Whether the backend accepted minimization.
	bool minimizeAccepted = false;
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Execute one bounded real-loop scenario and validate its retained counters.
auto main (int argc, char **argv) -> int
{
	try
	{
		require(argc == 2, "Expected one presentation scenario");
		const std::string scenario = argv[1];
		require(scenario == "lifecycle" || scenario == "mutation" || scenario == "error"
			|| scenario == "throw" || scenario == "init" || scenario == "minimized"
			|| scenario == "default" || scenario == "empty" || scenario == "close", "Unknown scenario");
		Results results;
		bool expectedFailure = false;
		try {
			const auto status = fcg::run({}, std::make_unique<PresentationProbe>(results, scenario));
			require(status == 0, "Runtime returned failure");
		}
		catch (const PresentationFailure &error) {
			require(scenario == "throw" || scenario == "init", error.what());
			expectedFailure = true;
		}
		catch (const std::runtime_error &error) {
			require(scenario == "error" && std::string(error.what()) == "presentation error sentinel", error.what());
			expectedFailure = true;
		}
		require(results.ended && results.ordered, "Captures outlived applet or were destroyed incorrectly");
		if (scenario == "error" || scenario == "throw") {
			require(expectedFailure && results.calls[0] == 1 && results.renders == 1, "Failure retried presentation");
			require(results.overlays == 0, "GUI backend rendered after presentation failure");
		}
		else if (scenario == "init")
			require(expectedFailure && results.calls[0] == 0 && results.renders == 0, "Init failure rendered a frame");
		else if (scenario == "close")
			require(!expectedFailure && results.calls[0] == 0 && results.renders == 0 && results.overlays == 0,
				"Early close recorded presentation or rendering");
		else
		{
			require(!expectedFailure && (scenario == "default" ? results.calls[0] == 0 && results.renders >= 3
				: results.calls[0] >= 3), "Too few successful presentations");
			require(results.drawableOverlays > 0 && results.overlays == results.drawableOverlays,
				"Successful presentation skipped drawable GUI rendering");
			if (scenario == "lifecycle" || scenario == "empty") {
				require(results.calls[1] >= 3 && results.resized, "Replacement or resize was not exercised");
				require(results.renders >= results.calls[0] + results.calls[1] + 3, "Reset did not render default frames");
			}
			if (scenario == "mutation")
				require(results.rejected == results.calls[0] * 2, "Missing mutation rejection");
			if (scenario == "minimized" && !results.noImage) {
				std::cout << "Minimized no-image acquisition unsupported by this backend\n";
				return 0;
			}
		}
		return 0;
	}
	catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}