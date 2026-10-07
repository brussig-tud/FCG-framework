
//////
//
// Includes
//

// C++ STL
#include <cstdlib>
#include <memory>
#include <vector>
#include <ranges>
#include <chrono>

// SDL3
#include <SDL3/SDL.h>

// Local includes
#include "FCG/run.h"
#include "FCG/event.h"
#include "FCG/device.h"
#include "FCG/render_state.h"
#include "FCG/gui.h"
#include "FCG/player.h"
#include "FCG/window.h"



//////
//
// Module namespace open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Functions
//

// Local anonymous namespace
namespace {

/// Handle a single SDL event for the central framework window.
void handleEvent (const SDL_Event &event, Window &window, Gui &gui, Player &player, std::vector<std::unique_ptr<Applet>> &applets)
{
	// Always feed the GUI so ImGui can react to input
	gui.processEvent(event);

	// Dispatch logic
	// - categorize the event context as basis for dispatch decisions below
	const bool windowEvent = event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST;
	const bool inputEvent = event.type >= SDL_EVENT_KEY_DOWN && event.type <= SDL_EVENT_DROP_POSITION;
	bool mainWindowEvent = !windowEvent && !inputEvent;
	if (event.type >= SDL_EVENT_KEY_DOWN && event.type <= SDL_EVENT_TEXT_EDITING_CANDIDATES)
		mainWindowEvent = event.key.windowID == window.id();
	else if (event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL)
		mainWindowEvent = event.motion.windowID == window.id();
	else if (event.type >= SDL_EVENT_DROP_FILE && event.type <= SDL_EVENT_DROP_POSITION)
		mainWindowEvent = event.drop.windowID == window.id();
	const bool keyboard = event.type >= SDL_EVENT_KEY_DOWN && event.type <= SDL_EVENT_TEXT_EDITING_CANDIDATES;
	const bool mouse = event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL;
	// - dispatch to the applets
	if (mainWindowEvent && inputEvent && !(keyboard && gui.wantsKeyboard()) && !(mouse && gui.wantsMouse())) {
		Event normalized = makeEvent(event);
		EventContext context;
		for (auto &applet : applets)
			applet->onEvent(normalized, context, player);
		if (   event.type == SDL_EVENT_KEY_DOWN && event.key.windowID == window.id() && event.key.key == SDLK_ESCAPE
		    && !context.wasHandled() && !gui.wantsTextInput())
			player.requestClose();
	}

	switch (event.type)
	{
		// Global quit request (e.g. from the OS)
		case SDL_EVENT_QUIT:
			player.requestClose();
			break;

		// Our window was asked to close (e.g. via its title bar close button)
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			if (event.window.windowID == window.id())
				player.requestClose();
			break;

		// A key was pressed – the escape key closes our window, unless the GUI is currently capturing text input
		// (in which case the key belongs to the text field being edited)
		case SDL_EVENT_KEY_DOWN:
			if (event.key.windowID == window.id() && event.key.key == SDLK_ESCAPE && !gui.wantsTextInput())
				player.requestClose();
			break;

		// The display scale changed (system DPI setting or window moved to a monitor with different scaling) –
		// re-apply the content scale to the GUI style
		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
		case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
			if (event.window.windowID == window.id())
				gui.updateContentScale(window);
			break;

		default:
			break;
	}
}

// Local anonymous namespace close
}


/// Run the given application(s).
FCG_FRAMEWORK_EXPORT auto run (
	std::vector<std::unique_ptr<Applet>> _applets, PlayerSettings &&settings
) -> int
{
	// Info trace
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: starting up...");

	// Bail out early if there is nothing to run
	if (_applets.size() < 1) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "fcg::run() was called without applets – nothing to do");
		return EXIT_SUCCESS;
	}

	// Initialize SDL – only the video subsystem is required for window creation and the SDL GPU API
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Initializing SDL failed: %s", SDL_GetError());
		return EXIT_FAILURE;
	}

	// Scope for the shared GPU device and the central window – they are guaranteed to be cleaned up before SDL_Quit()
	// is called below. Note the declaration order: the window must be destroyed before the GPU device it renders with.
	int exitCode = EXIT_SUCCESS;
	{
		// Create the GPU device shared by all windows and applets
		auto maybeDevice = Device::create();

		// Create the central window that all applets will render into
		auto window = maybeDevice ? Window::create(WindowSettings {
			.title = std::move(settings.mainWindowTitle).value_or("FCG Player")
		}) : nullptr;
		if (!window)
			exitCode = EXIT_FAILURE;
		else
		{
			// We now have a working device
			auto &device = maybeDevice.value();

			// Create the player that the applets will interact with
			Player player(device, window.get());

			// Claim the window for the GPU device, then create the framework GUI on top of it. The GUI instance is
			// destroyed at scope exit, before the window is unclaimed below.
			std::unique_ptr<Gui> gui;
			if (device.claimWindow(window)) {
				gui = Gui::create(device, *window);
				if (!gui) {
					SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Initializing the framework GUI failed");
					exitCode = EXIT_FAILURE;
					player.requestClose();
				}
			}
			else {
				exitCode = EXIT_FAILURE;
				player.requestClose();
			}
			/* First-time window-related state initialization */ {
				std::optional<glm::uvec2> dummy;
				window->pollViewportSize(dummy);
				player.recreateReadbackBuffers();
			}
			std::vector<std::unique_ptr<Applet>> applets = std::move(_applets);
			if (gui)
			{
				// Initialize all applets
				for (auto &applet : applets) {
					SDL_LogInfo(
						SDL_LOG_CATEGORY_APPLICATION, "Player: initializing applet %p (\"%s\")",
						(void*)applet.get(), applet->name().c_str()
					);
					applet->init(device, player);
				}

				// Info trace
				SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: startup complete.");
			}

			// The main loop – runs until the window is closed. By default it blocks while waiting for events; while at
			// least one applet requests continuous redraws, the next iteration is instead started as soon as possible.
			// ImGui is immediate-mode, so input events processed in one frame only manifest in the next frame (and
			// the resulting layout settles one frame after that) – hence every event burst is followed by additional
			// redraws, tracked via the pending redraws counter. It is initialized to 2 so the first frames are drawn
			// right after startup instead of only after some external event unblocks the loop.
			unsigned pendingRedraws = 2;
			auto lastFrameTime = std::chrono::high_resolution_clock::now();
			while (!player.shouldClose())
			{
				const auto oldViewportSize = window->viewportSize();
				// Begin the new rendering frame. We need it now because the applets might need to interact with the
				// player in ways that require the current target textures during their update() or gui() hooks (like
				// scheduling readbacks upon user interaction).
				player.frame = window->beginFrame(device);
				// - any readbacks should be done now
				player.collectReadbackResults();

				// Frame acquisition updates the viewport and may replace the depth texture. Detect that change before
				// applets handle input, and keep an acquired frame's dimensions authoritative for this iteration.
				if (!player.frame) {
					std::optional<glm::uvec2> ignoredOldSize;
					window->pollViewportSize(ignoredOldSize);
				}
				if (window->viewportSize() != oldViewportSize) {
					player.recreateReadbackBuffers();
					for (auto &applet : applets)
						applet->onViewportResize(device, oldViewportSize, player);
				}

				// Event handling
				if (player.continuousRedrawRequested() || pendingRedraws > 0)
				{
					// Non-blocking mode: process all pending events without waiting. Any event could change GUI
					// state, so schedule follow-up redraws to let the GUI fully manifest the results.
					SDL_Event event;
					bool handledAnyEvent = false;
					while (SDL_PollEvent(&event)) {
						handleEvent(event, *window, *gui, player, applets);
						handledAnyEvent = true;
					}
					if (handledAnyEvent)
						pendingRedraws = 2;
					else if (pendingRedraws > 0)
						--pendingRedraws;
				}
				else
				{
					// Blocking mode: wait for the next event, then drain any burst of events that has accumulated in
					// the queue in the meantime. While the GUI needs periodic redraws (e.g. for a blinking text caret),
					// the wait times out after the redraw interval so the GUI stays animated even without input.
					const int waitTimeoutMs = gui->needsPeriodicRedraw() ? Gui::periodicRedrawIntervalMs : -1;
					SDL_Event event;
					if (SDL_WaitEventTimeout(&event, waitTimeoutMs)) {
						lastFrameTime = std::chrono::high_resolution_clock::now();
						handleEvent(event, *window, *gui, player, applets);
						while (SDL_PollEvent(&event))
							handleEvent(event, *window, *gui, player, applets);
						// The GUI reacts to input with one frame of latency, so schedule follow-up redraws
						pendingRedraws = 2;
					}
					else if (waitTimeoutMs < 0) {
						// An error while waiting without timeout is fatal – a timeout is not, it just means the
						// periodic redraw interval expired with no events pending
						SDL_LogError(
							SDL_LOG_CATEGORY_APPLICATION, "Waiting for events failed: %s", SDL_GetError()
						);
						exitCode = EXIT_FAILURE;
						player.requestClose();
					}
				}

				// Stop early if the window was requested to close while handling events
				if (player.shouldClose())
					break;

				// Update frame stats
				auto now = std::chrono::high_resolution_clock::now();
				const auto frameDur = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastFrameTime);
				lastFrameTime = now;
				const auto dt = (float)(double(frameDur.count()) / 1000000000.);

				// Begin the GUI frame, then update applet state and let them define their GUI
				gui->newFrame();
				for (auto &applet : applets) {
					applet->gui(device, player);
					applet->update(device, player, dt);
				}

				// Render a frame: all applets draw into the primary render pass (with depth buffer), then the GUI is
				// rendered on top in a separate overlay pass without depth attachment. The ImGui SDL GPU backend
				// creates pipelines without a depth-stencil target, so it must be recorded into a pass that has none.
				gui->prepareRender(player.frame);
				if (player.frame)
				{
					auto rs = RenderState(device);
					if (auto *renderPass = player.frame->beginRenderPass(player.clearColor())) {
						for (auto &applet : applets)
							applet->render(
								device, rs, renderPass, player.frame->commandBuffer(), player
							);
						player.frame->endRenderPass();
					}
					if (auto *overlayPass = player.frame->beginOverlayRenderPass()) {
						gui->renderDrawData(player.frame->commandBuffer(), overlayPass);
						player.frame->endRenderPass();
					}
					player.frame = nullptr;
					window->endFrame();
				}
			}
			if (player.frame) {
				window->endFrame();
				player.frame = nullptr;
			}

			// Clean up
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: shutting down...");
			for (auto &applet : std::views::reverse(applets)) {
				SDL_LogInfo(
					SDL_LOG_CATEGORY_APPLICATION, "Player: ending applet %p (\"%s\")",
					(void*)applet.get(), applet->name().c_str()
				);
				applet.reset();
			}
			device.unclaimWindow(window);
		}
	}

	// Info trace
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: exiting...");

	// Shut down SDL and report our status
	SDL_Quit();
	return exitCode;
}



//////
//
// Module namespace close
//

} // namespace fcg
