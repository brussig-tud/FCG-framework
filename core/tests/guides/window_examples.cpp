
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <span>
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

/// \brief Create a window with native screen-coordinate dimensions and automatic resizing disabled.
///
/// \return The new window, or \c nullptr if creation failed.
[[nodiscard]] auto createNativeSizeWindow () -> std::unique_ptr<fcg::Window>
{
	// [native_size]
	auto window = fcg::Window::create({.width = 800, .height = 600, .scaleToDisplay = false});
	// [native_size]
	return window;
}

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
		const fcg::GammaUniforms parameters{.gamma = glm::vec3(1)};
		const std::span<const std::byte> uniforms[]{std::as_bytes(std::span(&parameters, 1))};
		return pass.record(context.cmdBuffer, context.target, {.samplers = {&binding, 1}, .uniforms = uniforms});
	};
}
// [presentation_setup]

/// \brief Record and submit a frame using a recorder created before entering the frame loop.
///
/// \param device The device claiming the window.
/// \param window The claimed window, with no frame in progress.
/// \param recorder The recorder to borrow for this frame's presentation.
///
/// \throws std::runtime_error If frame acquisition, scene-pass creation, presentation or submission fails.
/// \throws ...
/// 	Exceptions from drawing or the recorder propagate unchanged after frame submission is attempted, even if
/// 	submission also fails.
// [frame_loop]
void renderFrame (fcg::Device &device, fcg::Window &window, fcg::PresentationRecorder &recorder)
{
	auto *frame = window.beginFrame(device);
	if (!frame)
		return; // <- no window image is available, for example while minimized.
	try
	{
		if (frame->beginRenderPass({0.f, 0.f, 0.f, 1.f}))
		{
			/// Ends the scene pass on every exit from this scope (including exceptions).
			struct EndRenderPass {
				fcg::Frame &frame;
				~EndRenderPass() { frame.endRenderPass(); }
			} renderPassGuard{*frame};

			// ... record draw calls here ...
		}
		else
			throw std::runtime_error("Beginning the scene render pass failed");
		if (auto result = frame->present(recorder); !result)
			throw std::runtime_error(result.error().message);
	}
	catch (...)
	{
		try { window.endFrame(); }
		catch (...) {
			// Submission failures are already logged; preserve the original exception.
		}
		throw;
	}
	window.endFrame();
}
// [frame_loop]
