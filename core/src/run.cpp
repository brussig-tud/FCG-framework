
//////
//
// Includes
//

// C++ STL
#include <cstdlib>
#include <exception>
#include <memory>
#include <vector>
#include <ranges>
#include <chrono>
#include <stdexcept>
#include <utility>

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
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Handle a single SDL event: feed it to the GUI, dispatch main-window input events to the applets unless the GUI
/// captures them, and react to close requests and Escape.
void handleEvent (
	const SDL_Event &event, fcg::Window &window, fcg::Gui &gui, fcg::Player &player,
	std::vector<std::unique_ptr<fcg::Applet>> &applets
){
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
	// - dispatch to the applets, remembering whether one of them consumed the event
	bool handledByApplet = false;
	if (mainWindowEvent && inputEvent && !(keyboard && gui.wantsKeyboard()) && !(mouse && gui.wantsMouse()))
	{
		fcg::Event normalized = fcg::makeEvent(event);
		fcg::EventContext context;
		for (auto &applet : applets) {
			applet->onEvent(normalized, context, player);
			if (context.wasHandled()) {
				handledByApplet = true;
				break;
			}
		}
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

		// A key was pressed – the escape key closes our window, unless an applet marked it as handled or the GUI is
		// currently capturing text input (in which case the key belongs to the text field being edited)
		case SDL_EVENT_KEY_DOWN:
			if (   event.key.windowID == window.id() && event.key.key == SDLK_ESCAPE && !handledByApplet
			    && !gui.wantsTextInput())
				player.requestClose();
			break;

		default:
			break;
	}
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Function implementations
//

FCG_FRAMEWORK_EXPORT auto run (
	std::vector<std::unique_ptr<Applet>> _applets, PlayerSettings &&settings
) -> int
{
	// Retain reverse destruction order even when startup stops before ownership moves into the player scope.
	auto destroyApplets = [] (std::vector<std::unique_ptr<Applet>> *owned) noexcept {
		for (auto &applet : std::views::reverse(*owned))
			applet.reset();
	};
	std::unique_ptr<
		std::vector<std::unique_ptr<Applet>>, decltype(destroyApplets)
	> unstartedApplets(&_applets, destroyApplets);

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
	auto shutdownSDL = [&] (void*) noexcept {
		unstartedApplets.reset();
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: exiting...");
		SDL_Quit();
	};
	std::unique_ptr<void, decltype(shutdownSDL)> sdlOwner(&_applets, shutdownSDL);

	// Destroy the central window and shared device before the SDL guard above runs, by opening a new scope here.
	int exitCode = EXIT_SUCCESS;
	{
		// Create the GPU device shared by all windows and applets
		auto maybeDevice = Device::create();

		// Create the central window that all applets will render into
		auto window = maybeDevice ? Window::create(WindowSettings {
			.title = std::move(settings.mainWindowTitle).value_or("FCG Player"),
			.scaleToDisplay = settings.mainWindowScaleToDisplay
		}) : nullptr;
		if (!window)
			exitCode = EXIT_FAILURE;
		else
		{
			// We now have a working device
			auto &device = maybeDevice.value();

			// Moving the vector preserves the element addresses referenced by the player.
			Player player(device, window.get(), std::span(_applets));

			// Acquired frames finish inside the loop, before applet and GUI teardown releases the claim.
			auto releaseClaim = [&] (Window*) {
				device.unclaimWindow(window);
			};
			std::unique_ptr<Window, decltype(releaseClaim)> windowClaim(nullptr, releaseClaim);

			// Claim the window for the GPU device, then create the framework GUI on top of it. The GUI instance is
			// destroyed before releasing the window claim, including during exception unwinding.
			std::unique_ptr<Gui> gui; auto detachGui = [](Player *owner) { owner->m_gui = nullptr; };
			std::unique_ptr<Player, decltype(detachGui)> guiBinding(&player, detachGui);
			// - take ownership of the applets vector and install a guard to guarantee reverse destruction order. We
			//   need to do this now to have the guarantee already active in case any step of window-device marriage or
			//   GUI initialization fails.
			std::vector<std::unique_ptr<Applet>> applets = std::move(_applets);
			auto shutdownApplets = [&] (std::vector<std::unique_ptr<Applet>> *owned) noexcept {
				SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: shutting down...");
				player.shutdownPresentation();
				destroyApplets(owned);
			};
			std::unique_ptr<std::vector<std::unique_ptr<Applet>>, decltype(shutdownApplets)> appletOwner(
				&applets, shutdownApplets
			);
			// - now claim the window and initialize the GUI
			if (device.claimWindow(window))
			{
				windowClaim.reset(window.get());
				if (auto result = player.initializePresentation(); !result) {
					SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Initializing presentation failed: %s",
						result.error().message.c_str());
					exitCode = EXIT_FAILURE;
					player.requestClose();
				}
				else
					gui = Gui::create(device, *window);
				if (!gui && !player.shouldClose()) {
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
				player.invalidateReadbacks();
			}

			// Initialize the applets
			if (gui)
			{
				player.m_gui = gui.get();
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
				// Store un-refreshed viewport size for detecting size changes after the new frame has been acquired
				const auto oldViewportSize = window->viewportSize();

				// Begin the new rendering frame. We need it now because the applets might need to interact with the
				// player in ways that require the current target textures during their update() or gui() hooks (like
				// scheduling readbacks upon user interaction).
				player.frame = window->beginFrame(device);

				/// Finish this iteration's frame on both nominal completion/exit and unwinding, preserving any original
				/// exception
				struct FrameCompletion
				{
					/// Borrow the window and the player's current-frame pointer for this iteration.
					FrameCompletion(Window &window, Frame *&frame)
						: window(window), frame(frame), exceptions(std::uncaught_exceptions())
					{}

					/// Submit once; secondary submission failures are already logged by the frame.
					~FrameCompletion() noexcept(false)
					{
						if (!std::exchange(frame, nullptr))
							// no frame was active
							return;
						try { window.endFrame(); }
						catch (...) {
							if (std::uncaught_exceptions() == exceptions)
								throw;
						}
					}

				private:
					Window &window;
					Frame *&frame;
					int exceptions;
				} frameCompletion(*window, player.frame);

				// Any readbacks should be done now
				player.collectReadbackResults();

				// Frame acquisition updates the viewport and may replace the depth texture. Detect that change before
				// applets handle input, and keep an acquired frame's dimensions authoritative for this iteration.
				if (!player.frame) {
					std::optional<glm::uvec2> ignoredOldSize;
					window->pollViewportSize(ignoredOldSize);
				}
				if (window->viewportSize() != oldViewportSize) {
					player.invalidateReadbacks();
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
						pendingRedraws = 2; // ImGui reacts to input with one frame latency, schedule follow-up redraws
					}
					else if (waitTimeoutMs < 0)
					{
						// A false result from an indefinite wait reports an error. Timed waits with no event are treated
						// as timeouts, since SDL does not reliably distinguish them from failures.
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
				const auto frameDur = std::chrono::duration_cast<std::chrono::nanoseconds>(
					now - lastFrameTime
				);
				lastFrameTime = now;
				const auto dt = (float)(double(frameDur.count()) / 1000000000.);

				// Events may have changed the display after frame acquisition. Resize before ImGui lays out widgets;
				// the acquired frame's extent remains authoritative until the next acquisition.
				window->updateDisplayScale();

				// Begin the GUI frame, then update applet state and let them define their GUI
				gui->newFrame(player.frame);
				for (auto &applet : applets) {
					applet->gui(device, player);
					applet->update(device, player, dt);
				}

				// Render a frame: all applets draw into the primary render pass (with depth buffer), then the GUI is
				// rendered on top in a separate overlay pass without depth attachment.
				gui->prepareRender(player.frame);
				if (player.frame)
				{
					// Common logic to both scene render and overlay pass guards
					auto endPass = [](Frame *frame) { frame->endRenderPass(); };

					// Render the scene
					auto rs = RenderState(device);
					if (auto *renderPass = player.frame->beginRenderPass(player.clearColor())) {
						std::unique_ptr<Frame, decltype(endPass)> passGuard(player.frame, endPass);
						for (auto &applet : applets)
							applet->render(device, rs, renderPass, player.frame->commandBuffer(), player);
					}

					// Present the scene
					if (auto result = player.presentFrame(); !result)
						throw std::runtime_error(result.error().message);

					// Render the GUI overlay
					if (auto *overlayPass = player.frame->beginOverlayRenderPass()) {
						std::unique_ptr<Frame, decltype(endPass)> passGuard(player.frame, endPass);
						gui->renderDrawData(player.frame->commandBuffer(), overlayPass);
					}
				}
			}
		}
	}

	return exitCode;
}



//////
//
// Module namespace close
//

} // namespace fcg
