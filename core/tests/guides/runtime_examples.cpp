
//////
//
// Includes
//

// [presentation_includes]
// C++ STL
#include <expected>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

// Dear ImGui
#include <imgui.h>

// FCG Framework
#include <FCG/applet.h>
#include <FCG/frame.h>
#include <FCG/fullscreen.h>
#include <FCG/player.h>
// [presentation_includes]

// [includes]
#include <FCG/run.h>
// [includes]



//////
//
// Classes
//

// [applet_declaration]
/// An applet that presents the scene with a custom gamma curve and offers a return to built-in presentation.
class ExampleApplet : public fcg::Applet
{
public:
	////
	// Interface: Applet

	/// Return the applet's stable name.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string instanceName = "Example";
		return instanceName;
	}

	/// Initialize resources independent of pipeline creation.
	void init (fcg::Device&, fcg::Player&) override {}

	/// \brief Install a move-only gamma correction presentation pass on the first pipeline-creation callback.
	///
	/// \param device The application's GPU device.
	/// \param player The player that owns the recorder.
	///
	/// \throws std::runtime_error If creating the presentation pass fails.
	/// \throws std::bad_alloc If allocating recorder ownership fails.
	// [presentation_install]
	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		if (presentationInitialized)
			return;
		auto pass = fcg::FullscreenPass::gammaCorrection (player.device(), player.swapchainFormat());
		if (!pass)
			throw std::runtime_error (pass.error().message);
		player.setPresentationRecorder (
			[this, pass = std::move (*pass)] (const fcg::PresentationContext &context)
				-> std::expected<void, fcg::FullscreenError>
			{
				const fcg::FullscreenTextureBinding sceneBindings[] = {
					{&context.scene, &context.sceneSampler}
				};
				const std::span<const std::byte> uniforms[] = {std::as_bytes(std::span(&gammaUniforms, 1))};
				const fcg::FullscreenBindings bindings{.samplers = sceneBindings, .uniforms = uniforms};
				return pass.record (context.cmdBuffer, context.target, bindings);
			}
		);
		presentationInitialized = true;
	}
	// [presentation_install]

	/// \brief Receive viewport changes; bindings are rebuilt during presentation.
	///
	/// \param device The application's GPU device.
	/// \param oldViewportSize The previous viewport extent in pixels.
	/// \param player The application's player.
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}

	/// \brief Adjust custom presentation gamma and offer a button that resumes built-in presentation.
	///
	/// \param device The application's GPU device.
	/// \param player The player whose presentation recorder is cleared.
	// [presentation_reset]
	void gui (fcg::Device &device, fcg::Player &player) override
	{
		// [presentation_gamma]
		ImGui::SliderFloat ("Red gamma", &gammaUniforms.gamma.r, .1f, 4.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderFloat ("Green gamma", &gammaUniforms.gamma.g, .1f, 4.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderFloat ("Blue gamma", &gammaUniforms.gamma.b, .1f, 4.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		// [presentation_gamma]
		if (ImGui::Button ("Resume built-in presentation"))
			player.clearPresentationRecorder ();
	}
	// [presentation_reset]

	/// \brief Receive an update without changing scene state.
	///
	/// \param device The application's GPU device.
	/// \param player The application's player.
	/// \param dt Elapsed time in seconds.
	void update (fcg::Device &device, fcg::Player &player, float dt) override {}

	/// \brief Leave the scene at its clear color for the presentation example.
	///
	/// \param device The application's GPU device.
	/// \param renderState The shared scene render state.
	/// \param renderPass The active scene pass.
	/// \param commandBuffer The command buffer containing the scene pass.
	/// \param player The application's player.
	void render (
		fcg::Device &device, fcg::RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, fcg::Player &player
	) override {}


private:

	////
	// Fields

	/// Whether presentation has been configured; preserve any later recorder selection across scene changes.
	bool presentationInitialized = false;

	/// The presentation parameters read by the recorder on each invocation, defaulting to gamma 2.2 per channel and
	/// RGB bounds [0, 1].
	// [presentation_parameters]
	fcg::GammaUniforms gammaUniforms{};
	// [presentation_parameters]
};
// [applet_declaration]



//////
//
// Function implementations
//

// [run_single]
/// \brief Run the presentation example.
///
/// \return The application's exit status.
auto main () -> int {
	return fcg::run<ExampleApplet>();
}
// [run_single]

// [run_settings]
/// \brief Run the presentation example with a custom window title.
///
/// \return The application's exit status.
[[nodiscard]] auto runWithSettings () -> int {
	fcg::PlayerSettings settings{.mainWindowTitle = "My application"};
	return fcg::run(std::move(settings), std::make_unique<ExampleApplet>());
}
// [run_settings]
