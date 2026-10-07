
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Dear ImGui
#include <imgui.h>

// FCG Framework
#include <FCG/run.h>
#include <FCG/player.h>
#include <FCG/applet.h>
#include <FCG/applet/camera_2d.h>
#include <FCG/Image/image.h>
#include <FCG/Image/image_loader.h>
#include <FCG/Render/quad_renderer.h>



//////
//
// Classes
//

// Our demo applet.
class ImageViewerApplet : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Default constructor.
	ImageViewerApplet() {}

	/// The destructor. Releases the graphics pipeline created during <code>\ref init</code>.
	~ImageViewerApplet() override {}


	////
	// Interface: fcg::Applet

	[[nodiscard]] auto name () const -> const std::string& override {
		const static std::string name = "Image Viewer";
		return name;
	}

	void init (fcg::Device &device, fcg::Player &player) override
	{}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {
		// Nothing to do yet.
	}

	void gui (fcg::Device &device, fcg::Player &player) override
	{
		ImGui::SetNextWindowSize({ 0, 0 }, ImGuiCond_FirstUseEver);
		ImGui::Begin("Image Viewer");

		/* TODO: define our GUI */

		ImGui::End();
	}

	void update (fcg::Device &device, fcg::Player &player, float dt) override {
		// Nothing to do yet.
	}

	void render (
		fcg::Device &device, fcg::RenderState &rs, SDL_GPURenderPass *renderPass, SDL_GPUCommandBuffer *commandBuffer,
		fcg::Player &player
	) override
	{}


protected:

	////
	// Fields

	/// The renderer for the image quad.
	//fcg::QuadRenderer qr;
};



//////
//
// Functions
//

/// Program entry point.
int main () {
	// Run with our demo applets
	return fcg::run<fcg::applet::Camera2D, ImageViewerApplet>(
		fcg::PlayerSettings{.mainWindowTitle="Image Viewer"}
	);
}
