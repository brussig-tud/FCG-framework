
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <string>

// FCG Framework
// [includes]
#include <FCG/run.h>
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

	void init (fcg::Device &device, fcg::Player &player) override {}
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}
	void gui (fcg::Device &device, fcg::Player &player) override {}
	void update (fcg::Device &device, fcg::Player &player, float dt) override {}
	void render (
		fcg::Device &device, fcg::RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, fcg::Player &player
	) override {}
};
// [applet_declaration]



//////
//
// Function implementations
//

// [run_single]
int main ()
{
	return fcg::run<ExampleApplet>();
}
// [run_single]

// [run_settings]
int runWithSettings ()
{
	fcg::PlayerSettings settings{.mainWindowTitle = "My application"};
	return fcg::run(std::move(settings), std::make_unique<ExampleApplet>());
}
// [run_settings]
