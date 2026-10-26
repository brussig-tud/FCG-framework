
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
#include <memory>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/gtc/quaternion.hpp>

// Dear ImGui
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

// FCG Framework
#include <FCG/run.h>
#include <FCG/player.h>
#include <FCG/applet.h>
#include <FCG/Image/image.h>
#include <FCG/Image/image_loader.h>
#include <FCG/Render/quad_renderer.h>
#include <FCG/applet/camera_2d.h>
#include <FCG/Extras/gui.h>
#include <FCG/Extras/file_dialog.h>
#include <FCG/Extras/file_filters.h>



//////
//
// Classes
//

/// \brief Applet that displays an image file as a textured quad and lets the user open other images via a file dialog.
///
/// The image is drawn as a quad of height 2 centered at the origin, with its width matching the image's aspect ratio.
/// Combine it with a planar camera applet such as \c fcg::applet::Camera2D for navigation.
class ImageViewerApplet : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Constructs the applet without any resources. They are created by \c init.
	ImageViewerApplet() = default;

	/// Releases the image, its texture and all rendering resources.
	~ImageViewerApplet() override = default;


	////
	// Interface: fcg::Applet

	/// \brief Returns the display name of this applet.
	///
	/// \return The string <tt>"Image Viewer"</tt>.
	[[nodiscard]] auto name () const -> const std::string& override {
		const static std::string name = "Image Viewer";
		return name;
	}

	/// \brief Creates the quad attributes and sampler, then loads the placeholder image <tt>assets/cgvlogo.png</tt>.
	///
	/// \param device The device to create the GPU resources on.
	/// \param player The player running the applet.
	///
	/// \throws std::runtime_error if creating the sampler, the texture or the quad attributes fails.
	void init (fcg::Device &device, fcg::Player &player) override
	{
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

		// Load initial placeholder image
		loadImage(device, "assets/cgvlogo.png", player);
	}

	/// \brief Replace the quad renderer for the effective scene format.
	///
	/// \param device The application's GPU device.
	/// \param player The player providing the effective scene render target.
	///
	/// \throws std::runtime_error If creating the renderer fails.
	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		qr.reset();
		// Init our quad renderer
		if (auto maybeQr
		    = fcg::QuadRenderer::create(player, {.alphaBlending=true}); maybeQr)
			qr = std::move(*maybeQr);
		else
			throw std::runtime_error(std::format(
				"Image Viewer: failed to create quad renderer: {}", maybeQr.error().message
			));

	}

	/// Does nothing, as the applet holds no viewport-dependent resources.
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {
		// Nothing to do.
	}

	/// Shows a window with the current file name, a button that opens a file dialog, and the image dimensions.
	void gui (fcg::Device &device, fcg::Player &player) override
	{
		// Start our widget
		const auto &workPos = ImGui::GetMainViewport()->WorkPos,
				   &padding = ImGui::GetStyle().WindowPadding;
		const auto workSizeX = ImGui::GetMainViewport()->WorkSize.x;
		const auto initialWidth = player.toGuiUnits(264.f);
		ImGui::SetNextWindowSize({initialWidth, 432}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(
			ImVec2{workSizeX-initialWidth - padding.x, workPos.y + padding.y}, ImGuiCond_FirstUseEver
		);
		ImGui::Begin("Image Loader");

		// File selection
		ImGui::BeginDisabled(); {
			auto filename = imageFilepath.filename().string();
			ImGui::InputText("##imageFilepath", &filename);
		} ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("..."))
			openImage(player);

		// Image metadata
		if (image)
			fcg::extra::displayImageMetadata(*image);
		else
			ImGui::TextUnformatted("No image loaded");

		// Finalize our widget
		ImGui::End();
	}

	/// Does nothing, as the applet has no per-frame state to advance.
	void update (fcg::Device &device, fcg::Player &player, float dt) override {
		// Nothing to do.
	}

	/// \brief
	/// 	Draws the image quad with the current viewing transformation and alpha blending. Draws nothing while no
	/// 	image is loaded.
	///
	/// \throws std::runtime_error if drawing the quad fails.
	void render (
		fcg::Device &device, fcg::RenderState &rs, SDL_GPURenderPass *renderPass, SDL_GPUCommandBuffer *commandBuffer,
		fcg::Player &player
	) override
	{
		if (!texture || !qr)
			return;
		fcg::DrawOptions options;
		options.texture = fcg::PrimitiveTexture{texture->handle(), sampler->handle()};
		if (auto drawn = qr->draw(*attributes, rs, commandBuffer, renderPass, options); !drawn)
			throw std::runtime_error(drawn.error().message);
	}


protected:

	////
	// Methods

	/// \brief
	/// 	Uploads the current \c image into a new texture and sets up the quad with height 2 and the image's aspect
	/// 	ratio.
	///
	/// The quad is rotated by 180 degrees around the \e x-axis, flipping it vertically so that the image's first row
	/// appears at the top.
	///
	/// \pre \c image holds an image.
	///
	/// \param device The device to create the texture on.
	///
	/// \throws std::runtime_error if the upload or the quad attribute update fails.
	void uploadImage (fcg::Device &device)
	{
		// Upload to texture
		if (auto maybeTex = image->upload(device); maybeTex)
			texture = std::move(*maybeTex);
		else
			throw std::runtime_error(maybeTex.error().message);

		// Configure the quad for displaying our image
		const std::array position{glm::vec4(0.f, 0.f, 0.f, 1.f)};
		auto updated = attributes->setAttributes(
			[&] (fcg::PrimitiveAttributes::Update &update)
			{
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

	/// \brief Loads \p filepath through \c fcg::ImageLoader::load and performs color-managed upload.
	///
	/// Missing RGB metadata is labelled with the loader's default assumptions before uploading.
	/// If loading fails, the error is logged and the previously loaded image, if any, stays on display.
	///
	/// \param device The device to create the texture on.
	/// \param filepath The path of the image file to load.
	/// \param player The player whose scene is refreshed after uploading the image.
	///
	/// \throws std::runtime_error if uploading the image fails.
	void loadImage (fcg::Device &device, const std::filesystem::path &filepath, fcg::Player &player)
	{
		// Load from file
		auto maybeImage = fcg::ImageLoader::global().load(filepath);
		if (!maybeImage) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION, "ImageViewerApplet: failed to load image '%s'\n%s",
				filepath.string().c_str(), maybeImage.error().message.c_str()
			);
			return;
		}
		image = std::move(*maybeImage);
		imageFilepath = filepath;

		// Upload
		uploadImage(device);
		player.requireSceneRedraw();
	}

	/// \brief
	/// 	Shows a native open-file dialog filtered to the image formats currently registered with the global
	/// 	\c fcg::ImageLoader, and loads the selected file.
	///
	/// Blocks until the user closes the dialog. Does nothing if the user cancels. If the dialog cannot be shown, the
	/// error is logged and nothing else happens.
	///
	/// \param player The player whose main window is the dialog's parent and whose device receives the texture.
	///
	/// \throws std::runtime_error if uploading the selected image fails.
	void openImage (fcg::Player &player)
	{
		// Open the file dialog
		auto selected = fcg::extra::showOpenFileDialog(fcg::extra::FileDialogOptions {
			.parent=player.mainWindow(), .title="Open image",
			.filters=fcg::extra::imageFileFilters(fcg::ImageLoader::global().fileFormats())
		});
		if (!selected) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION, "ImageViewerApplet: file dialog failed\n%s",
				selected.error().message.c_str()
			);
			return;
		}

		// Try to load the selected image if any
		if (!selected->paths.empty() && !selected->paths.front().empty())
			loadImage(player.device(), selected->paths.front(), player);
	}


	////
	// Fields

	/// The path of the currently loaded image file.
	std::filesystem::path imageFilepath;

	/// The currently loaded image in CPU memory.
	std::optional<fcg::Image> image;

	/// The GPU texture created from <tt>\ref image "image"</tt> for display.
	std::optional<fcg::Texture> texture;

	/// The renderer for the image quad.
	std::optional<fcg::QuadRenderer> qr;

	/// The sampler used for the image texture, with linear filtering and clamp-to-edge addressing.
	std::optional<fcg::Sampler> sampler;

	/// The attributes of the single image quad: position, extent and orientation.
	std::optional<fcg::PrimitiveAttributes> attributes;
};



//////
//
// Function implementations
//

/// The program entry point. Runs a 2D camera followed by the image viewer applet in a window titled
/// <tt>"Image Viewer"</tt>.
auto main () -> int {
	// Run with 2D camera and our image viewer applet
	return fcg::run<fcg::applet::Camera2D, ImageViewerApplet>(fcg::PlayerSettings{.mainWindowTitle="Image Viewer"});
}
