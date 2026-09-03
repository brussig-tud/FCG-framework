
//////
//
// Includes
//

// C++ STL
#include <array>
#include <memory>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui
#include <imgui.h>

// FCG Framework
#include <FCG/applet.h>
#include <FCG/player.h>
#include <FCG/run.h>

// Local includes
#include <shapes.h>



//////
//
// Classes
//

// Our demo applet.
class SimpleShapesApplet : public fcg::Applet
{
public:

	////
	// Types

	/// The applet factory
	struct Factory : public fcg::AppletFactory
	{
		//////
		// Object construction/destruction

		/// The destructor.
		virtual ~Factory () = default;


		////
		// Interface: fcg::AppletFactory

		auto create () -> std::unique_ptr<fcg::Applet> override {
			return std::make_unique<SimpleShapesApplet>();
		}
	};


	////
	// Object construction/destruction

	/// Default constructor.
	SimpleShapesApplet()
		: shapes{
			std::make_unique<ConvexPolygon>()
		}
	{}

	/// The destructor.
	~SimpleShapesApplet() override = default;


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
		// Nothing to initialize here. Shapes are lazy-rebuilt in update() when their dirty flag
		// (set by default) is set.
	}

	void gui (fcg::Device &device, fcg::Player &player) override
	{
		ImGui::SetNextWindowSize({ 0, 0 }, ImGuiCond_FirstUseEver);
		ImGui::Begin("Simple Shapes");

		// Shape selection combo box.
		if (ImGui::BeginCombo("Shape", shapes[selectedShape]->name())) {
			for (unsigned i=0; i<(unsigned)SS::NUM; ++i) {
				const bool isSelected = (i == static_cast<std::size_t>(selectedShape));
				if (ImGui::Selectable(shapes[i]->name(), isSelected)) {
					selectedShape = static_cast<int>(i);
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();

		// GUI for the currently selected shape's parameters.
		shapes[selectedShape]->gui();

		ImGui::End();
	}

	void update (fcg::Device &device, fcg::Player &player) override {
		// Make sure our shape is up-to-date and ready to render
		shapes[selectedShape]->update(device);
	}

	void render (
		fcg::Device &device, fcg::RenderState &renderState, SDL_GPURenderPass *renderPass, fcg::Player &player
	) override {
		// Nothing to draw yet. GPU buffers for the selected shape are ready in
		// shape.vertexBuffer() / shape.indexBuffer() and will be rendered once we have shader handling.
	}


protected:

	////
	// Fields

	/// All available simple shapes, instantiated once.
	std::unique_ptr<SimpleShape> shapes[(size_t)SS::NUM];

	/// Index of the currently selected shape in \ref shapes.
	int selectedShape = 0;
};



//////
//
// Functions
//

/// Program entry point.
int main () {
	// Run with our demo applets
	return fcg::run<SimpleShapesApplet::Factory>(fcg::PlayerSettings{.mainWindowTitle="Hello FCG!"});
}
