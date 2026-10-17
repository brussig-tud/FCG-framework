
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

// Framework implementation
#include "window_scaling.h"

// Framework public API
#include <FCG/window.h>

// SDL3 library
#include <SDL3/SDL.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Report the location of a failed behavior check.
void check (bool condition, const std::source_location &where = std::source_location::current()) {
	if (!condition)
		throw std::runtime_error("Window scaling check failed at line " + std::to_string(where.line()));
}

/// Exercise native-size requests without a window manager or GPU.
class Window
{
public:

	////
	// Object construction

	/// Initialize the requested and actual client dimensions.
	explicit Window(glm::ivec2 size = {800, 600}) : policy(size), actual(size) {}


	////
	// Methods

	/// Apply a simulated display transition with optional native size constraints.
	void move (float display, float density = 1.f, bool suspended = false)
	{
		policy.refresh(display, density, actual, suspended, [this] (glm::ivec2 size)
		{
			++requests;
			requested = size;
			if (!reject)
				actual = limit.x > 0 ? glm::min(size, limit) : size;
			return actual;
		});
	}


	////
	// Fields

	/// Production policy under test.
	fcg::detail::WindowScaling policy;

	/// Actual dimensions returned by the simulated window manager.
	glm::ivec2 actual;

	/// Most recent resize request.
	glm::ivec2 requested = {0, 0};

	/// Optional client-size limit imposed by the window manager.
	glm::ivec2 limit = {0, 0};

	/// Number of native resize requests.
	unsigned requests = 0;

	/// Whether to reject resize requests or defer their completion.
	bool reject = false;
};

/// Check startup, scale transitions, density-only changes and fractional round trips.
void transitions ()
{
	Window window;
	window.move(1.5f);
	check(window.actual == glm::ivec2(1200, 900));
	window.move(1.5f);
	check(window.requests == 1);
	window.move(2.f);
	check(window.actual == glm::ivec2(1600, 1200));
	window.move(1.f);
	check(window.actual == glm::ivec2(800, 600));
	const auto requests = window.requests;
	window.move(2.f, 2.f);
	check(window.requests == requests);
	Window fractional({803, 607});
	for (int i = 0; i < 50; ++i) {
		fractional.move(1.25f);
		check(fractional.actual == glm::ivec2(1004, 759));
		fractional.move(1.75f);
		fractional.move(1.f);
		check(fractional.actual == glm::ivec2(803, 607));
	}
	Window retina;
	retina.move(2.f, 2.f);
	check(retina.requests == 0);
	retina.move(1.f, 1.f);
	check(retina.requests == 0);
}

/// Check user resizing and restoration without adopting maximized or fullscreen dimensions.
void resizingAndRestoration ()
{
	Window window;
	window.move(1.5f);
	window.actual = {1350, 975};
	window.move(1.5f);
	check(window.requests == 1);
	window.move(2.f);
	check(window.actual == glm::ivec2(1800, 1300));
	window.actual = {3840, 2160};
	window.move(1.f, 1.f, true);
	check(window.requests == 2);
	window.actual = {1800, 1300};
	window.move(1.f);
	check(window.actual == glm::ivec2(900, 650));
	window.actual = {0, 0};
	window.move(2.f, 1.f, true);
	window.actual = {900, 650};
	window.move(2.f);
	check(window.actual == glm::ivec2(1800, 1300));
	// A user resize and a scale transition can be observed in the same update.
	window.actual = {1400, 1000};
	window.move(1.f);
	check(window.actual == glm::ivec2(700, 500));
	window.move(2.f);
	window.actual = {1000, 800};
	window.move(2.f);
	// Returning manually to an older automatic size must establish a new reference too.
	window.actual = {1400, 1000};
	window.move(2.f);
	window.move(1.f);
	check(window.actual == glm::ivec2(700, 500));
}

/// Check constrained, rejected and delayed requests without retries or lost reference dimensions.
void nativeRestrictions ()
{
	Window window;
	window.limit = {1000, 750};
	window.move(2.f);
	check(window.requested == glm::ivec2(1600, 1200));
	check(window.actual == glm::ivec2(1000, 750));
	window.move(2.f);
	check(window.requests == 1);
	window.move(1.f);
	check(window.actual == glm::ivec2(800, 600));
	window.reject = true;
	window.move(2.f);
	window.move(2.f);
	check(window.requests == 3);
	// A timeout can be followed by a late acknowledgement of the requested size.
	window.actual = window.requested;
	window.move(2.f);
	window.reject = false;
	window.limit = {0, 0};
	window.move(1.f);
	check(window.actual == glm::ivec2(800, 600));
	window.move(2.f);
	window.actual = {1000, 800};
	window.move(2.f);
	window.move(1.f);
	check(window.actual == glm::ivec2(500, 400));
}

/// Check invalid queries, recovery, tolerance and safe integer conversion.
void invalidQueries ()
{
	Window window;
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float infinity = std::numeric_limits<float>::infinity();
	window.move(0.f);
	window.move(nan);
	window.move(infinity);
	window.move(2.f, 0.f);
	window.move(2.f, -1.f);
	window.move(2.f, nan);
	window.move(2.f, infinity);
	check(window.requests == 0);
	window.move(1.5f);
	check(window.actual == glm::ivec2(1200, 900));
	window.move(nan);
	window.move(1.5001f);
	check(window.requests == 1);
	window.move(1.f);
	check(window.actual == glm::ivec2(800, 600));
	window.reject = true;
	window.move(std::numeric_limits<float>::max());
	check(window.requested == glm::ivec2(std::numeric_limits<int>::max()));
}

/// Exercise window creation, opt-out and manual resizing through SDL's headless video driver.
void nativeWindows ()
{
	check(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));
	check(SDL_Init(SDL_INIT_VIDEO));
	for (bool enabled : {false, true})
	{
		auto window = fcg::Window::create({.width = 803, .height = 607, .resizable = false,
			.scaleToDisplay = enabled});
		check(bool(window));
		check(!(SDL_GetWindowFlags(window->handle()) & SDL_WINDOW_HIDDEN));
		check(!(SDL_GetWindowFlags(window->handle()) & SDL_WINDOW_RESIZABLE));
		glm::ivec2 size;
		check(SDL_GetWindowSize(window->handle(), &size.x, &size.y));
		check(size == glm::ivec2(803, 607));
		check(SDL_SetWindowSize(window->handle(), 700, 500));
		window->updateDisplayScale();
		std::optional<glm::uvec2> previous;
		window->pollViewportSize(previous);
		check(window->viewportSize() == glm::uvec2(700, 500));
	}
	check(!fcg::Window::create({.width = 0}));
	check(!fcg::Window::create({.width = std::numeric_limits<unsigned>::max()}));
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run CPU-only window sizing checks and report failures through the process exit status.
auto main () -> int
{
	try {
		transitions();
		resizingAndRestoration();
		nativeRestrictions();
		invalidQueries();
		nativeWindows();
	}
	catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		SDL_Quit();
		return 1;
	}
	SDL_Quit();
	return 0;
}
