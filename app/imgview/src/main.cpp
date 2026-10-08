
//////
//
// Includes
//

// C++ STL
#include <format>
#include <array>
#include <stdexcept>
#include <filesystem>
#include <optional>

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/gtc/quaternion.hpp>

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

/// Displays a textured image with planar camera navigation.
class ImageViewerApplet : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// The default constructor.
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
	{
		// Init our quad renderer
		if (auto maybeQr
		    = fcg::QuadRenderer::create(player, {.alphaBlending=true}); maybeQr)
			qr = std::move(*maybeQr);
		else
			throw std::runtime_error(std::format(
				"Image Viewer: failed to create quad renderer: {}", maybeQr.error().message
			));

		// Init attribute storage for use with quad renderer
		attributes.emplace(/* fcg::PrimitiveAttributes::<ctor>: */device);

		// Create our texture sampler
		if (auto maybeSampler =fcg::Sampler::create(device, SDL_GPUSamplerCreateInfo {
		    	.min_filter = SDL_GPU_FILTER_LINEAR, .mag_filter = SDL_GPU_FILTER_LINEAR,
		    	.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		    	.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		    	.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
		    }); maybeSampler)
			sampler = std::move(*maybeSampler);
		else
			throw std::runtime_error(maybeSampler.error().message);

		// Load image initial image
		loadImage(device, std::filesystem::path(SDL_GetBasePath())/"assets/cgvlogo.png");
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {
		// Nothing to do yet.
	}

	void gui (fcg::Device &device, fcg::Player &player) override
	{
		ImGui::SetNextWindowSize({ 0, 0 }, ImGuiCond_FirstUseEver);
		ImGui::Begin("Image Viewer");

		ImGui::TextUnformatted("assets/cgvlogo.png");
		ImGui::Text("%d × %d pixels", image->width(), image->height());

		ImGui::End();
	}

	void update (fcg::Device &device, fcg::Player &player, float dt) override {
		// Nothing to do yet.
	}

	void render (
		fcg::Device &device, fcg::RenderState &rs, SDL_GPURenderPass *renderPass, SDL_GPUCommandBuffer *commandBuffer,
		fcg::Player &player
	) override
	{
		fcg::DrawOptions options;
		options.texture = fcg::PrimitiveTexture{texture->handle(), sampler->handle()};
		if (auto drawn = qr->draw(*attributes, rs, commandBuffer, renderPass, options); !drawn)
			throw std::runtime_error(drawn.error().message);
	}


protected:

	////
	// Methods

	/// Load image from given file.
	void loadImage (fcg::Device &device, const std::filesystem::path &filepath)
	{
		// Load from file
		if (auto maybeImage = fcg::ImageLoader::global().load(filepath); maybeImage)
			image = std::move(*maybeImage);
		else
			throw std::runtime_error(maybeImage.error().message);

		// Upload to texture
		if (auto maybeTex = image->upload(device); maybeTex)
			texture = std::move(*maybeTex);
		else
			throw std::runtime_error(maybeTex.error().message);

		/* create the quad for displaying our image */ {
			const std::array position{glm::vec4(0.f, 0.f, 0.f, 1.f)};
			auto updated = attributes->setAttributes(
				[&] (fcg::PrimitiveAttributes::Update &update) {
					update.set<fcg::Attribute::Position>(std::span(position));
					update.set<fcg::Attribute::Extent>(
						glm::vec3((float)image->width()/image->height(), 1, 1)
					);
					update.set<fcg::Attribute::Orientation>(
						glm::angleAxis(glm::radians(180.f), glm::vec3(1, 0, 0))
					);
				}
			);
			if (!updated)
				throw std::runtime_error(updated.error().message);
		}
	}


	////
	// Fields

	/// The CPU-side image which we keep around to have access to meta information
	std::optional<fcg::Image> image;

	/// The renderer for the image quad.
	std::optional<fcg::QuadRenderer> qr;

	/// Uploaded linear-sampled image.
	std::optional<fcg::Texture> texture;

	/// Linear filtered, clamped image sampling.
	std::optional<fcg::Sampler> sampler;

	/// One image quad's transform.
	std::optional<fcg::PrimitiveAttributes> attributes;
};



//////
//
// Functions
//

/// Program entry point.
auto main () -> int {
	// Run with 2D camera and our image viewer applet
	return fcg::run<fcg::applet::Camera2D, ImageViewerApplet>(fcg::PlayerSettings{.mainWindowTitle="Image Viewer"});
}
