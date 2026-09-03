
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
		: m_shapes{ {
			std::make_unique<ConvexPolygon>()
		} }
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
		if (ImGui::BeginCombo("Shape", m_shapes[m_selected]->name())) {
			for (unsigned i=0; i<(unsigned)SS::NUM; ++i) {
				const bool isSelected = (i == static_cast<std::size_t>(m_selected));
				if (ImGui::Selectable(m_shapes[i]->name(), isSelected)) {
					m_selected = static_cast<int>(i);
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();

		// GUI for the currently selected shape's parameters.
		m_shapes[m_selected]->gui();

		ImGui::End();
	}

	void update (fcg::Device &device, fcg::Player &player) override {
		auto &shape = *m_shapes[m_selected];
		if (shape.dirty()) {
			shape.rebuild(device);
		}
	}

	void render (fcg::Device &device, SDL_GPURenderPass *renderPass, fcg::Player &player) override {
		// Nothing to draw yet. GPU buffers for the selected shape are ready in
		// shape.vertexBuffer() / shape.indexBuffer() and will be rendered once we have shader handling.
	}


protected:

	////
	// Fields

	/// All available simple shapes, instantiated once.
	std::unique_ptr<SimpleShape> m_shapes[(size_t)SS::NUM];

	/// Index of the currently selected shape in \ref m_shapes.
	int m_selected = 0;
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
