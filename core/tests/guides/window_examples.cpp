
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <stdexcept>

// FCG Framework
// [includes]
#include <FCG/device.h>
#include <FCG/window.h>
// [includes]



//////
//
// Function implementations
//

// [window_setup]
[[nodiscard]] auto createWindow (fcg::Device &device) -> std::unique_ptr<fcg::Window>
{
	auto window = fcg::Window::create(
		fcg::WindowSettings{.title = "My application", .width = 1280, .height = 720}
	);
	if (!window)
		throw std::runtime_error("Creating the window failed");
	if (!device.claimWindow(window))
		throw std::runtime_error("Claiming the window failed");
	return window;
}
// [window_setup]

// [frame_loop]
void renderFrame (fcg::Device &device, fcg::Window &window)
{
	if (auto *frame = window.beginFrame(device)) {
		// Record the scene into an active render pass.
		if (frame->beginRenderPass({0.f, 0.f, 0.f, 1.f})) {
			// ... record draw calls here ...
			frame->endRenderPass();
		}
		// Presenting the scene before overlays happens automatically; just end the frame.
		window.endFrame();
	}
	else {
		// No frame could be acquired (e.g. the window is minimized); skip rendering.
	}
}
// [frame_loop]
