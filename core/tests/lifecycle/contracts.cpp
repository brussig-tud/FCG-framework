
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

	/// Escape delivery order across the three observers.
	std::vector<unsigned> escapeOrder;

	/// Unhandled Space delivery order across the three observers.
	std::vector<unsigned> spaceOrder;

	/// Number of wheel events consumed before the camera.
	unsigned wheels = 0;

	/// Camera borrowed until the runtime destroys its applets.
	fcg::applet::Camera2D *camera = nullptr;

	/// Camera span before the synthetic wheel event.
	float initialSpan = 0.f;

	/// Whether consumed wheel input left the camera unchanged.
	bool cameraUnchanged = false;
};



//////
//
// Classes
//

/// Camera with its normal input behavior and no widgets that could capture the probe's synthetic keyboard input.
class InputCamera final : public fcg::applet::Camera2D
{
public:

	////
	// Interface: fcg::applet::Camera2D

	/// Keep keyboard focus available to the event observers.
	void gui (fcg::Device&, fcg::Player&) override {}
};


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

	/// No scene pipelines are needed by this applet.
	void recreatePipelines (fcg::Device&, fcg::Player&) override {}

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
			if (const auto *key = event.data<fcg::KeyEvent>())
			{
				if (key->key == SDLK_ESCAPE) {
					results.escapeOrder.push_back(1);
					if (++results.escapes == 1)
						context.markHandled();
				}
				if (key->key == SDLK_SPACE)
					results.spaceOrder.push_back(1);
			}
	}

	/// This probe does not allocate viewport-dependent resources.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {}

	/// This probe does not capture keyboard input through a GUI.
	void gui (fcg::Device&, fcg::Player&) override {}

	/// Send Escape events on separate frames and bound failure by a frame count.
	void update (fcg::Device&, fcg::Player &player, float) override
	{
		if (++updates == 1)
		{
			SDL_Event wheel{};
			wheel.type = SDL_EVENT_MOUSE_WHEEL;
			wheel.wheel.windowID = window;
			wheel.wheel.y = 1.f;
			if (!SDL_PushEvent(&wheel))
				throw std::runtime_error(SDL_GetError());
			SDL_Event space{};
			space.type = SDL_EVENT_KEY_DOWN;
			space.key.windowID = window;
			space.key.key = SDLK_SPACE;
			if (!SDL_PushEvent(&space))
				throw std::runtime_error(SDL_GetError());
			pushEscape();
		}
		if (results.escapes == 1 && !results.continued) {
			results.cameraUnchanged = results.camera->frustumDiameterAtFocus() == results.initialSpan;
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


/// Observe input before or after the consumer; the first observer consumes wheel input before it reaches a camera.
class EventObserver final : public fcg::Applet
{
public:

	////
	// Object construction

	/// Borrow the retained counters and select the observer position.
	EventObserver(Results &results, unsigned index) : results(results), index(index) {}


	////
	// Interface: fcg::Applet

	/// Return a stable observer name.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string label = "Event observer";
		return label;
	}

	/// Observe delivery order and consume wheel events at the first recipient.
	void onEvent (const fcg::Event &event, fcg::EventContext &context, fcg::Player&) override
	{
		if (!index && event.type() == fcg::EventType::MouseWheel) {
			++results.wheels;
			context.markHandled();
		}
		if (event.type() == fcg::EventType::KeyDown)
			if (const auto *key = event.data<fcg::KeyEvent>()) {
				if (key->key == SDLK_ESCAPE)
					results.escapeOrder.push_back(index);
				if (key->key == SDLK_SPACE)
					results.spaceOrder.push_back(index);
			}
	}

	/// No scene pipelines are needed by this applet.
	void recreatePipelines (fcg::Device&, fcg::Player&) override {}

	/// No initialization resources are needed.
	void init (fcg::Device&, fcg::Player&) override {}

	/// No viewport-dependent resources are needed.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {}

	/// No widgets are needed.
	void gui (fcg::Device&, fcg::Player&) override {}

	/// The central probe controls the test lifetime.
	void update (fcg::Device&, fcg::Player&, float) override {}

	/// The observer does not draw scene geometry.
	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, fcg::Player&
	) override {}


private:

	////
	// Fields

	/// Counters owned by the main function.
	Results &results;

	/// Observer position, zero before and two after the consuming probe.
	unsigned index;
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
		auto camera = std::make_unique<InputCamera>();
		results.camera = camera.get();
		results.initialSpan = camera->frustumDiameterAtFocus();
		const auto status = fcg::run({}, std::make_unique<EventObserver>(results, 0),
			std::make_unique<RuntimeProbe>(results), std::move(camera), std::make_unique<EventObserver>(results, 2));
		std::cout << "status=" << status << " escapes=" << results.escapes << " continued=" << results.continued
			<< " wheels=" << results.wheels << " camera=" << results.cameraUnchanged << " escape order:";
		for (auto index : results.escapeOrder)
			std::cout << ' ' << index;
		std::cout << "; space order:";
		for (auto index : results.spaceOrder)
			std::cout << ' ' << index;
		std::cout << '\n';
		return status == 0 && results.escapes == 2 && results.continued && !results.timedOut
			&& results.escapeOrder == std::vector<unsigned>{0, 1, 0, 1, 2}
			&& results.spaceOrder == std::vector<unsigned>{0, 1, 2}
			&& results.wheels == 1 && results.cameraUnchanged ? 0 : 1;
	}
	catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
