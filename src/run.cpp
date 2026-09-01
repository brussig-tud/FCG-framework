
//////
//
// Includes
//

// C++ STL
#include <cstdlib>
#include <memory>
#include <initializer_list>

// SDL3
#include <SDL3/SDL.h>

// Local includes
#include "FCG/run.h"
#include "FCG/device.h"
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
// Local functions
//

namespace {

	/// Handle a single SDL event for the central framework window.
	void handleEvent (const SDL_Event &event, Window &window)
	{
		switch (event.type)
		{
			// Global quit request (e.g. from the OS)
			case SDL_EVENT_QUIT:
				window.requestClose();
				break;

			// Our window was asked to close (e.g. via its title bar close button)
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (event.window.windowID == window.id())
					window.requestClose();
				break;

			// A key was pressed – the escape key closes our window
			case SDL_EVENT_KEY_DOWN:
				if (event.key.windowID == window.id() && event.key.key == SDLK_ESCAPE)
					window.requestClose();
				break;

			default:
				break;
		}
	}

} // unnamed namespace



//////
//
// Functions
//

/// Run the given application(s).
FCG_FRAMEWORK_EXPORT int run (std::initializer_list<std::unique_ptr<fcg::Applet>> applets, PlayerSettings &&settings)
{
	// Info trace
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: starting up...");

	// Bail out early if there is nothing to run
	if (applets.size() < 1) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "fcg::run() was called without applets – nothing to do");
		return EXIT_SUCCESS;
	}

	// Initialize SDL – only the video subsystem is required for window creation and the SDL GPU API
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Initializing SDL failed: %s", SDL_GetError());
		return EXIT_FAILURE;
	}

	// Scope for the shared GPU device and the central window – they are guaranteed to be cleaned up before
	// SDL_Quit() is called below. Note the declaration order: the window must be destroyed before the GPU
	// device it renders with.
	int exitCode = EXIT_SUCCESS;
	{
		// Create the GPU device shared by all windows and applets
		auto maybeDevice = Device::create();

		// Create the central window that all applets will render into
		auto window = maybeDevice ? Window::create(WindowSettings {
			.title = std::move(settings.mainWindowTitle.value_or("FCG Player"))
		}) : nullptr;
		if (!window)
			exitCode = EXIT_FAILURE;
		else
		{
			auto &device = maybeDevice.value();
			Player player(window.get());
			if (device.claimWindow(window))
			{
				// Create the player that the applets will interact with, and initialize all applets
				for (auto &applet : applets) {
					SDL_LogInfo(
						SDL_LOG_CATEGORY_APPLICATION, "Player: initializing applet %x (\"%s\")",
						applet.get(), applet->name().c_str()
					);
					applet->init(device, player);
				}

				// Info trace
				SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: startup complete.");
			}
			else {
				exitCode = EXIT_FAILURE;
				window->requestClose();
			}

			// The main loop – runs until the window is closed. By default it blocks while waiting for
			// events; while at least one applet requests continuous redraws, the next iteration is
			// instead started as soon as possible.
			while (!window->shouldClose())
			{
				// Event handling
				if (player.continuousRedrawRequested())
				{
					// Continuous redraw mode: process all pending events without blocking
					SDL_Event event;
					while (SDL_PollEvent(&event))
						handleEvent(event, *window);
				}
				else
				{
					// Blocking mode: wait for the next event, then drain any burst of events that has
					// accumulated in the queue in the meantime
					SDL_Event event;
					if (SDL_WaitEvent(&event)) {
						handleEvent(event, *window);
						while (SDL_PollEvent(&event))
							handleEvent(event, *window);
					}
					else {
						SDL_LogError(
							SDL_LOG_CATEGORY_APPLICATION, "Waiting for events failed: %s", SDL_GetError()
						);
						exitCode = EXIT_FAILURE;
						window->requestClose();
					}
				}

				// Stop early if the window was requested to close while handling events
				if (window->shouldClose())
					break;

				// Update applet state and let them define their GUI
				for (auto &applet : applets) {
					applet->gui(player);
					applet->update(player);
				}

				// Render a frame: all applets draw one after another into the same render pass
				// targeting the window's swapchain texture
				auto frame = window->beginFrame(device);
				if (frame)
				{
					if (auto *renderPass = frame->beginRenderPass(player.clearColor())) {
						for (auto &applet : applets)
							applet->render(device, renderPass, player);
						frame->endRenderPass();
					}
					window->endFrame();
				}
			}

			// Clean up
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Player: shutting down...");
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
