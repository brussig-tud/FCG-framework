
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

// FCG Framework
#include <FCG/applet/camera_2d.h>
#include <FCG/player.h>
#include <FCG/run.h>



//////
//
// Structs and enums
//

/// Results retained after the runtime destroys its applets.
struct Results
{
	/// Number of Escape key presses delivered to the probe.
	unsigned escapes = 0;

	/// Whether updating continued after the handled Escape event.
	bool continued = false;

	/// Whether the runtime failed to close after the unhandled Escape event.
	bool timedOut = false;
};



//////
//
// Classes
//

/// Exercise camera lookup and Escape handling through the public runtime interfaces.
class RuntimeProbe final : public fcg::Applet
{
public:

	////
	// Object construction

	/// Store caller-owned results and the name used by camera lookup.
	RuntimeProbe(Results &results, std::string name = "Runtime contracts")
		: results(results), label(std::move(name))
	{}


	////
	// Interface: fcg::Applet

	/// Return the name used to look up this non-camera applet.
	[[nodiscard]] auto name () const -> const std::string& override { return label; }

	/// Check camera lookup and obtain the main window for synthetic keyboard input.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		checkCameras(device);
		int count = 0;
		auto **windows = SDL_GetWindows(&count);
		if (windows && count == 1)
			window = SDL_GetWindowID(windows[0]);
		SDL_free(windows);
		if (!window)
			throw std::runtime_error("Expected one runtime window");
		player.pushContinuousRedraw();
	}

	/// Consume the first Escape; leave the second one to the runtime's close handling.
	void onEvent (const fcg::Event &event, fcg::EventContext &context, fcg::Player&) override
	{
		if (event.type() == fcg::EventType::KeyDown)
			if (const auto *key = event.data<fcg::KeyEvent>(); key && key->key == SDLK_ESCAPE)
				if (++results.escapes == 1)
					context.markHandled();
	}

	/// This probe does not allocate viewport-dependent resources.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {}

	/// This probe does not capture keyboard input through a GUI.
	void gui (fcg::Device&, fcg::Player&) override {}

	/// Send Escape events on separate frames and bound failure by a frame count.
	void update (fcg::Device&, fcg::Player &player, float) override
	{
		if (++updates == 1)
			pushEscape();
		if (results.escapes == 1 && !results.continued) {
			results.continued = true;
			pushEscape();
		}
		if (updates > 120) {
			results.timedOut = true;
			player.requestClose();
		}
	}

	/// The framework's empty render pass suffices for this runtime probe.
	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, fcg::Player&
	) override {}


private:

	////
	// Methods

	/// Check absent names, non-camera matches, and selection of the first matching camera.
	void checkCameras (fcg::Device &device)
	{
		std::vector<std::unique_ptr<fcg::Applet>> applets;
		auto first = std::make_unique<fcg::applet::Camera2D>();
		auto *expected = static_cast<fcg::Camera*>(first.get());
		const auto cameraName = first->name();
		applets.push_back(std::make_unique<RuntimeProbe>(results, cameraName));
		{
			fcg::Player lookup(device, nullptr, applets);
			if (lookup.findCamera("absent") || lookup.findCamera(cameraName))
				throw std::runtime_error("Camera lookup returned a non-camera applet");
		}
		applets.push_back(std::move(first));
		applets.push_back(std::make_unique<fcg::applet::Camera2D>());
		fcg::Player lookup(device, nullptr, applets);
		const auto found = lookup.findCamera(cameraName);
		if (!found || *found != expected)
			throw std::runtime_error("Camera lookup did not return the first matching camera");
	}

	/// Queue a complete Escape press/release pair addressed to the runtime window.
	void pushEscape ()
	{
		SDL_Event event{};
		event.type = SDL_EVENT_KEY_DOWN;
		event.key.windowID = window;
		event.key.key = SDLK_ESCAPE;
		event.key.scancode = SDL_SCANCODE_ESCAPE;
		event.key.down = true;
		if (!SDL_PushEvent(&event))
			throw std::runtime_error(SDL_GetError());
		event.type = SDL_EVENT_KEY_UP;
		event.key.down = false;
		if (!SDL_PushEvent(&event))
			throw std::runtime_error(SDL_GetError());
	}


	////
	// Fields

	/// Caller-owned results that outlive this probe.
	Results &results;

	/// Name returned to the runtime and camera lookup.
	std::string label;

	/// Main window receiving synthetic input.
	SDL_WindowID window = 0;

	/// Number of update callbacks, used to bound a failed close test.
	unsigned updates = 0;
};



//////
//
// Functions
//

/// Run the probe and require both Escape delivery and the expected close behavior.
auto main () -> int
{
	try {
		Results results;
		const auto status = fcg::run({}, std::make_unique<RuntimeProbe>(results));
		return status == 0 && results.escapes == 2 && results.continued && !results.timedOut ? 0 : 1;
	}
	catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
