
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Dear ImGui
#include <imgui.h>

// FCG Framework
// [includes]
#include <FCG/applet.h>
#include <FCG/player.h>
// [includes]



//////
//
// Classes
//

// [applet_declaration]
class ExampleApplet : public fcg::Applet
{
public:
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string instanceName = "Example";
		return instanceName;
	}

	void gui (fcg::Device &device, fcg::Player &player) override;

	/// No scene pipelines are needed by this applet.
	void recreatePipelines (fcg::Device&, fcg::Player&) override {}

	void init (fcg::Device &device, fcg::Player &player) override {}
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}
	void update (fcg::Device &device, fcg::Player &player, float dt) override {}
	void render (
		fcg::Device &device, fcg::RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, fcg::Player &player
	) override {}
};
// [applet_declaration]



//////
//
// Class implementations
//

////
// ExampleApplet

// [gui_widgets]
void ExampleApplet::gui (fcg::Device &device, fcg::Player &player)
{
	// [gui_authored_size]
	ImGui::SetNextWindowPos(player.toGuiUnits(ImVec2(20.f, 20.f)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(player.toGuiUnits(ImVec2(400.f, 300.f)), ImGuiCond_FirstUseEver);
	ImGui::Begin("Fixed-size panel");
	ImGui::TextUnformatted("Dimensions follow display scaling.");
	ImGui::End();
	// [gui_authored_size]

	// [gui_relative_size]
	ImGui::SetNextWindowSize(ImVec2(30.f * ImGui::GetFontSize(), 0.f), ImGuiCond_FirstUseEver);
	// [gui_relative_size]
	ImGui::Begin("Example");
	static float intensity = 0.5f;
	ImGui::SliderFloat("Intensity", &intensity, 0.f, 1.f);
	ImGui::End();
}
// [gui_widgets]
