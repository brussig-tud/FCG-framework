
//////
//
// Includes
//

// C++ STL
#include <memory>

// SDL3 library
#include <SDL3/SDL.h>

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

// Our demo applet.
class SimpleShapeApplet : public fcg::Applet
{
protected:

	////
	// Object construction

	/// Protected default constructor.
	SimpleShapeApplet() = default;


public:

	////
	// Object construction/destruction

	/// Default-construct as required by the \ref fcg::AppletConcept.
	static std::unique_ptr<SimpleShapeApplet> create () {
		return std::unique_ptr<SimpleShapeApplet>(new SimpleShapeApplet());
	}

	/// The destructor.
	~SimpleShapeApplet() override = default;


	////
	// Interface: fcg::Applet

	auto name () -> std::string& override {
		static std::string name = "Simple Shapes";
		return name;
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {
		// Nothing to do yet.
	}

	void init (fcg::Device &device, fcg::Player &player) override {
		// Nothing to initialize yet – this is where GPU resources would be created on the provided device.
	}

	void gui (fcg::Player &player) override
	{
		// Show a small window displaying the current dimensions of the main viewport
		ImGui::SetNextWindowSize({ 0, 0 }, ImGuiCond_FirstUseEver);
		ImGui::Begin("Simple Shapes");
		const auto viewportSize = player.mainViewportSize();
		ImGui::Text("Viewport: %u x %u", viewportSize.x, viewportSize.y);
		ImGui::End();
	}

	void update (fcg::Player &player) override {
		// Nothing to update yet. If you start animating something here, keep the main loop running via
		// player.pushContinuousRedraw(), and balance it with player.popContinuousRedraw() once the animation is done.
	}

	void render (fcg::Device &device, SDL_GPURenderPass *renderPass, fcg::Player &player) override {
		// Nothing to draw yet.
	}
};



//////
//
// Functions
//

/// Program entry point.
int main () {
	// Run with our demo applets
	return fcg::run<SimpleShapeApplet>(fcg::PlayerSettings{.mainWindowTitle="Hello FCG!"});
}
