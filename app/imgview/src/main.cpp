
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
#include <chrono>
#include <memory>
#include <utility>

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
#include <FCG/Image/image.h>
#include <FCG/Image/image_loader.h>
#include <FCG/Render/quad_renderer.h>
#include <FCG/applet/camera_2d.h>
#include <FCG/Extras/file_dialog.h>
#include <FCG/Extras/file_filters.h>



//////
//
// Classes
//

/// Displays a textured image with planar camera navigation.
class ImageViewerApplet : public fcg::Applet
{
	////
	// Types

	/// Complete image state, replaced as one owner only after all preparation succeeds.
	struct LoadedImage
	{
		/// Decoded CPU image and dimensions.
		fcg::Image image;

		/// Uploaded GPU texture.
		fcg::Texture texture;

		/// Separate ownership for nonmovable quad attributes.
		std::unique_ptr<fcg::PrimitiveAttributes> attributes;

		/// Actual native path of this image.
		std::filesystem::path path;

		/// UTF-8 path for ImGui display.
		std::string displayPath;
	};


public:

	////
	// Object construction/destruction

	/// The default constructor.
	ImageViewerApplet() {}

	/// Drain a pending native dialog before its parent and SDL are destroyed, then balance redraw requests.
	~ImageViewerApplet() override
	{
		if (pending.valid()) {
			while (pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
				SDL_PumpEvents();
				pending.wait_for(std::chrono::milliseconds(10));
			}
			(void)pending.get();
			owner->popContinuousRedraw();
		}
	}


	////
	// Interface: fcg::Applet

	/// Human-readable applet name.
	[[nodiscard]] auto name () const -> const std::string& override {
		const static std::string name = "Image Viewer";
		return name;
	}

	/// Create the renderer and sampler, then load the initial logo.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		owner = &player;

		// Init our quad renderer
		if (auto maybeQr
		    = fcg::QuadRenderer::create(player, {.alphaBlending=true}); maybeQr)
			qr = std::move(*maybeQr);
		else
			throw std::runtime_error(std::format(
				"Image Viewer: failed to create quad renderer: {}", maybeQr.error().message
			));

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
		const auto base = std::filesystem::path((const char8_t*)SDL_GetBasePath());
		if (auto loaded = loadImage(device, base/"assets/cgvlogo.png"); !loaded)
			throw std::runtime_error(loaded.error());
	}

	/// Image geometry stays independent of the viewport size.
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {
		// Nothing to do yet.
	}

	/// Show the actual image path, dimensions, loading status, and recoverable failures.
	void gui (fcg::Device &device, fcg::Player &player) override
	{
		pollImage(device, player);
		ImGui::SetNextWindowSize({ 0, 0 }, ImGuiCond_FirstUseEver);
		ImGui::Begin("Image Viewer");

		ImGui::BeginDisabled(pending.valid());
		if (ImGui::Button("Open image…"))
			openImage(player);
		ImGui::EndDisabled();
		if (pending.valid())
			ImGui::TextUnformatted("Waiting for file selection…");
		ImGui::TextUnformatted(current->displayPath.c_str());
		ImGui::Text("%d × %d pixels", current->image.width(), current->image.height());
		if (!error.empty())
			ImGui::TextWrapped("%s", error.c_str());

		ImGui::End();
	}

	/// Native completion is polled before GUI generation, so status and dimensions update before the loop idles.
	void update (fcg::Device &device, fcg::Player &player, float dt) override {}

	/// Draw the committed image state.
	void render (
		fcg::Device &device, fcg::RenderState &rs, SDL_GPURenderPass *renderPass, SDL_GPUCommandBuffer *commandBuffer,
		fcg::Player &player
	) override
	{
		fcg::DrawOptions options;
		options.texture = fcg::PrimitiveTexture{current->texture.handle(), sampler->handle()};
		if (auto drawn = qr->draw(*current->attributes, rs, commandBuffer, renderPass, options); !drawn)
			throw std::runtime_error(drawn.error().message);
	}


protected:

	////
	// Methods

	/// Poll native completion on the applet thread before displaying status, then commit a prepared replacement.
	void pollImage (fcg::Device &device, fcg::Player &player)
	{
		if (!pending.valid() || pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
			return;
		auto selected = pending.get();
		player.popContinuousRedraw();
		if (!selected) {
			error = selected.error().message;
			return;
		}
		if (selected->paths.empty())
			return;
		try {
			if (auto loaded = loadImage(device, selected->paths.front()); !loaded)
				error = std::move(loaded.error());
		} catch (const std::exception &failure) {
			error = failure.what();
		}
	}

	/// Open one file with a fresh snapshot of the singleton registry's format metadata.
	void openImage (fcg::Player &player)
	{
		if (pending.valid())
			return;
		try
		{
			fcg::extra::FileDialogOptions options;
			options.parent = player.mainWindow();
			options.defaultLocation = current->path;
			options.filters = fcg::extra::imageFileFilters(fcg::ImageLoader::global().fileFormats());
			options.title = "Open image";
			pending = fcg::extra::showOpenFileDialog(std::move(options));
			player.pushContinuousRedraw();
			error.clear();
		} catch (const std::exception &failure) {
			error = failure.what();
		}
	}

	/// Decode, upload, and prepare new quad storage before replacing any displayed state.
	[[nodiscard]] auto loadImage (fcg::Device &device, const std::filesystem::path &filepath)
		-> std::expected<void, std::string>
	{
		auto newPath = filepath;
		const auto utf8 = newPath.u8string();
		std::string displayPath((const char*)utf8.data(), utf8.size());
		auto image = fcg::ImageLoader::global().load(filepath);
		if (!image)
			return std::unexpected(std::move(image.error().message));
		auto texture = image->upload(device);
		if (!texture)
			return std::unexpected(std::move(texture.error().message));
		auto attributes = std::make_unique<fcg::PrimitiveAttributes>(device);
		const std::array position{glm::vec4(0.f, 0.f, 0.f, 1.f)};
		auto updated = attributes->setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
			update.set<fcg::Attribute::Position>(std::span(position));
			update.set<fcg::Attribute::Extent>(glm::vec3((float)image->width()/image->height(), 1, 1));
			update.set<fcg::Attribute::Orientation>(glm::angleAxis(glm::radians(180.f), glm::vec3(1, 0, 0)));
		});
		if (!updated)
			return std::unexpected(std::move(updated.error().message));
		auto replacement = std::make_unique<LoadedImage>(LoadedImage{
			std::move(*image), std::move(*texture), std::move(attributes), std::move(newPath), std::move(displayPath)
		});
		current = std::move(replacement);
		return {};
	}


	////
	// Fields

	/// Current image, texture, geometry, and path, committed as a single owner.
	std::unique_ptr<LoadedImage> current;

	/// Dialog result polled on the applet thread; destruction alone would not wait for native completion.
	std::future<fcg::extra::FileDialogResult> pending;

	/// Borrowed player, alive during applet destruction, for balancing pending redraw requests.
	fcg::Player *owner = nullptr;

	/// Inline recoverable error for the most recent opening attempt.
	std::string error;

	/// The renderer for the image quad.
	std::optional<fcg::QuadRenderer> qr;

	/// Linear filtered, clamped image sampling.
	std::optional<fcg::Sampler> sampler;
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
