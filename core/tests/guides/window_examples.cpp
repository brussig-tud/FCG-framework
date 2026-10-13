
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <stdexcept>
#include <utility>

// FCG Framework
// [includes]
#include <FCG/device.h>
#include <FCG/frame.h>
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

/// \brief Create a reusable scene presentation command recorder for a claimed window.
///
/// \param device The device claiming the window; it must outlive the recorder.
/// \param window The claimed window whose output format the recorder uses.
///
/// \return A recorder owning the sRGB conversion pass.
///
/// \throws std::runtime_error If creating the pass fails.
/// \throws std::bad_alloc If recorder allocation fails.
// [presentation_setup]
[[nodiscard]] auto createPresentationRecorder (fcg::Device &device, const fcg::Window &window) -> fcg::PresentationRecorder
{
	auto pass = fcg::FullscreenPass::linearToSRGB(device, window.swapchainFormat());
	if (!pass)
		throw std::runtime_error(pass.error().message);
	return [pass = std::move(*pass)] (const fcg::PresentationContext &context)
		-> std::expected<void, fcg::FullscreenError> {
		const fcg::FullscreenTextureBinding binding{&context.scene, &context.sceneSampler};
		return pass.record(context.cmdBuffer, context.target, {.samplers = {&binding, 1}});
	};
}
// [presentation_setup]

/// \brief Record and submit a frame using a recorder created before entering the frame loop.
///
/// \param device The device claiming the window.
/// \param window The claimed window, with no frame in progress.
/// \param recorder The recorder to borrow for this frame's presentation.
///
/// \throws std::runtime_error If frame acquisition or presentation fails.
/// \throws ... Recorder exceptions propagate after the acquired frame is submitted.
// [frame_loop]
void renderFrame (fcg::Device &device, fcg::Window &window, fcg::PresentationRecorder &recorder)
{
	auto *frame = window.beginFrame(device);
	if (!frame)
		return; // No window image is available, for example while minimized.
	try
	{
		if (frame->beginRenderPass({0.f, 0.f, 0.f, 1.f})) {
			// ... record draw calls here ...
			frame->endRenderPass();
		}
		if (auto result = frame->present(recorder); !result)
			throw std::runtime_error(result.error().message);
		// Optional overlays can be recorded here, after successful presentation.
	}
	catch (...) {
		window.endFrame();
		throw;
	}
	window.endFrame();
}
// [frame_loop]
