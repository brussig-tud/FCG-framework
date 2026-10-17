
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
	// [gui_relative_size]
	ImGui::SetNextWindowSize(ImVec2(30.f * ImGui::GetFontSize(), 0.f), ImGuiCond_FirstUseEver);
	// [gui_relative_size]
	ImGui::Begin("Example");
	static float intensity = 0.5f;
	ImGui::SliderFloat("Intensity", &intensity, 0.f, 1.f);
	ImGui::End();
}
// [gui_widgets]
