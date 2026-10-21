
//////
//
// Includes
//

// C++ STL
#include <string>

// Dear ImGui
#include <imgui.h>

// FCG Framework
#include <FCG/applet.h>
#include <FCG/player.h>
#include <FCG/run.h>



//////
//
// Classes
//

/// An applet that adjusts the player's built-in scene presentation.
class GammaCorrectionApplet : public fcg::Applet
{
public:
	////
	// Interface: fcg::Applet

	/// Return the applet's stable name.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string label = "Gamma correction";
		return label;
	}

	/// \brief Use built-in scene presentation without allocating applet resources.
	///
	/// \param device The application's GPU device.
	/// \param player The application's player.
	void init (fcg::Device &device, fcg::Player &player) override {}

	/// \brief Keep the controls independent of the viewport dimensions.
	///
	/// \param device The application's GPU device.
	/// \param oldViewportSize The previous viewport extent in pixels.
	/// \param player The application's player.
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}

	/// \brief Adjust scene gamma and offer a reset to standard sRGB presentation.
	///
	/// \param device The application's GPU device.
	/// \param player The player whose built-in presentation is adjusted.
	// [builtin_gamma]
	void gui (fcg::Device &device, fcg::Player &player) override
	{
		ImGui::Begin("Scene presentation");
		auto &correction = player.gammaCorrection();
		ImGui::SliderFloat("Gamma", &correction.gamma, .1f, 4.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		if (ImGui::Button("Reset gamma and RGB bounds"))
			correction = {.gamma = 1.f};
		ImGui::End();
	}
	// [builtin_gamma]

	/// \brief Leave presentation settings under the control of the GUI.
	///
	/// \param device The application's GPU device.
	/// \param player The application's player.
	/// \param dt Elapsed time in seconds.
	void update (fcg::Device &device, fcg::Player &player, float dt) override {}

	/// \brief Present the player's clear color without additional scene draws.
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
};



//////
//
// Function implementations
//

/// \brief Run the built-in gamma correction example.
///
/// \return The application's exit status.
auto main () -> int {
	return fcg::run<GammaCorrectionApplet>();
}
