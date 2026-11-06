
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui library
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

// FCG Framework
#include <FCG/run.h>
#include <FCG/player.h>
#include <FCG/applet/orbit_camera.h>
#include <FCG/Extras/file_dialog.h>
#include <FCG/Extras/file_filters.h>
#include <FCG/Extras/gui.h>
#include <FCG/Render/gpu_mesh.h>
#include <FCG/Render/mesh_renderer.h>



//////
//
// Classes
//

/// Displays a mesh in source coordinates, with file selection, inspection and surface rendering controls.
class MeshViewerApplet : public fcg::Applet
{
public:

	////
	// Interface: fcg::Applet

	/// Return the applet's display name.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string value = "Mesh Viewer";
		return value;
	}

	/// \brief Load the bundled startup mesh, reporting failures in the viewer.
	///
	/// \param device The device receiving GPU geometry.
	/// \param player The player supplying the camera and scene refreshes.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		// Resolve the bundled asset independently of the process working directory
		const char *base = SDL_GetBasePath();
		if (!base) {
			reportError(std::format("Cannot locate startup mesh: {}", SDL_GetError()));
			return;
		}
		loadMesh(device, std::filesystem::path(reinterpret_cast<const char8_t*>(base)) / "assets/mesh/max_low.obj", player);
	}

	/// \brief Create the renderer for the effective scene target and current surface settings.
	///
	/// \param device Unused; the player supplies the device.
	/// \param player The player supplying the effective scene target.
	///
	/// \throws std::runtime_error If pipeline creation fails.
	void recreatePipelines (fcg::Device &device, fcg::Player &player) override {
		auto next = fcg::MeshRenderer::create(player, layout(shading), {.cullMode=cullMode(cullBackFaces)});
		if (!next)
			throw std::runtime_error(next.error().message);
		renderer = std::move(*next);
	}

	/// \brief Keep geometry and the user's camera navigation when the viewport changes.
	///
	/// \param device Unused.
	/// \param oldViewportSize Unused; this applet has no viewport-dependent resources.
	/// \param player Unused; the camera handles projection changes.
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}

	/// \brief Keep the static mesh ready for rendering without per-frame simulation.
	///
	/// \param device Unused.
	/// \param player Unused.
	/// \param dt Unused.
	void update (fcg::Device &device, fcg::Player &player, float dt) override {}

	/// \brief Display file selection, surface controls and cached source information.
	///
	/// \param device Unused; loading uses the player's device.
	/// \param player The player supplying the window, device and camera.
	void gui (fcg::Device &device, fcg::Player &player) override
	{
		// Seed a scaled inspector near the right edge, retaining saved settings
		const auto *viewport = ImGui::GetMainViewport();
		const auto size = player.toGuiUnits(ImVec2(340.f, 540.f));
		const auto padding = ImGui::GetStyle().WindowPadding;
		ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - size.x - padding.x,
			viewport->WorkPos.y + padding.y}, ImGuiCond_FirstUseEver);
		const bool visible = ImGui::Begin("Mesh Loader");
		if (visible)
		{
			// Select another file through the same native-dialog workflow as the image viewer
			auto filename = meshFilepath.filename().string();
			const float buttonWidth = ImGui::CalcTextSize("...").x + 2.f * ImGui::GetStyle().FramePadding.x;
			ImGui::SetNextItemWidth(-buttonWidth - ImGui::GetStyle().ItemSpacing.x);
			ImGui::BeginDisabled();
			ImGui::InputText("##meshFilepath", &filename);
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("...", {buttonWidth, 0.f}))
				openMesh(player);

			// Stage surface settings until their replacement resources are complete
			int nextShading = shading;
			bool nextCull = cullBackFaces;
			ImGui::Combo("Shading", &nextShading, "File normals\0Flat\0Smooth\0");
			ImGui::Checkbox("Cull back faces", &nextCull);
			if (nextShading != shading || nextCull != cullBackFaces)
				changeRendering(player, nextShading, nextCull);
			if (ImGui::Checkbox("Lighting", &lighting))
				player.requireSceneRedraw();
			ImGui::BeginDisabled(!mesh);
			if (ImGui::Button("Fit view to mesh"))
				fitMesh(player);
			ImGui::EndDisabled();

			// Present source properties independently of generated rendering attributes
			ImGui::Separator();
			if (mesh) {
				fcg::extra::displayMeshMetadata(metadata);
				ImGui::Text("Rendered triangles: %zu", gpuMesh->triangleCount());
			} else
				ImGui::TextUnformatted("No mesh loaded");
			if (!error.empty()) {
				ImGui::Separator();
				ImGui::TextWrapped("%s", error.c_str());
			}
		}
		ImGui::End();
	}

	/// \brief Draw the loaded mesh using the camera's viewing state.
	///
	/// \param device Unused; resources already belong to the player's device.
	/// \param state Current viewing transformations supplied by the active camera.
	/// \param pass Active scene render pass.
	/// \param command Unsubmitted command buffer containing the pass.
	/// \param player Unused.
	void render (
		fcg::Device &device, fcg::RenderState &state, SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *command,
		fcg::Player &player
	) override
	{
		if (!gpuMesh || !renderer)
			return;
		fcg::MeshDrawOptions options;
		if (!metadata.vertices.colors && !metadata.faces.colors && !metadata.corners.colors)
			options.color = {1.f / 32.f, .25f, 2.f / 3.f, 1.f};
		options.lighting.enabled = lighting;
		renderer->draw(*gpuMesh, state, command, pass, options);
	}


private:

	////
	// Methods

	/// Select the GPU channels for file normals, flat shading or regenerated smooth normals.
	[[nodiscard]] static auto layout (int shading) -> fcg::GPUMeshLayout {
		return {.normals=shading != 1, .colors=true, .texCoords=false};
	}

	/// Select two-sided rendering or back-face culling.
	[[nodiscard]] static auto cullMode (bool enabled) -> SDL_GPUCullMode {
		return enabled ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
	}

	/// Upload source geometry without changing its properties or polygon boundaries.
	[[nodiscard]] static auto uploadMesh (fcg::Device &device, const fcg::Mesh &source, int shading)
		-> std::expected<fcg::GPUMesh, fcg::RenderError>
	{
		// Triangulate polygons and generate missing smooth normals only when requested
		fcg::MeshPreprocessingPolicy<> policy;
		policy.triangulate = fcg::triangulateEarCut;
		if (shading != 1)
			policy.vertexNormals = fcg::vertexNormalAngleWeights;
		const fcg::GPUMeshOptions options{.layout=layout(shading)};
		if (shading != 2)
			return fcg::GPUMesh::create(device, source, policy, options);

		// Regenerate every normal on a temporary copy, preserving authored attributes in the source
		auto smooth = source;
		for (auto vertex : smooth.vertices())
			smooth.data(vertex).hasNormal = false;
		for (auto face : smooth.faces())
			smooth.data(face).hasNormal = false;
		for (auto corner : smooth.halfedges())
			smooth.data(corner).hasNormal = false;
		return fcg::GPUMesh::create(device, smooth, policy, options);
	}

	/// Retain a diagnostic for the panel and forward it to the application's log.
	void reportError (std::string message) {
		error = std::move(message);
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mesh Viewer: %s", error.c_str());
	}

	/// Replace source geometry and its cached summary only after a complete successful upload.
	void loadMesh (fcg::Device &device, const std::filesystem::path &path, fcg::Player &player)
	{
		// Load using the registry's default topology policy and retain any previous mesh on failure
		auto loaded = fcg::MeshLoader::global().load(path);
		if (!loaded) {
			reportError(std::format("Cannot load '{}': {}", path.string(), loaded.error().message));
			return;
		}
		auto nextMesh = std::make_unique<fcg::Mesh>(std::move(*loaded));
		auto nextMetadata = fcg::extra::collectMeshMetadata(*nextMesh);
		if (!nextMetadata.faceCount) {
			reportError("The selected mesh has no drawable faces.");
			return;
		}
		auto uploaded = uploadMesh(device, *nextMesh, shading);
		if (!uploaded) {
			reportError(std::format("Cannot prepare '{}': {}", path.string(), uploaded.error().message));
			return;
		}
		if (!uploaded->triangleCount()) {
			reportError("The selected mesh has no drawable triangles.");
			return;
		}

		// Commit complete geometry before framing it through the shared camera API
		auto nextPath = path;
		meshFilepath.swap(nextPath);
		mesh = std::move(nextMesh);
		metadata = std::move(nextMetadata);
		gpuMesh = std::move(*uploaded);
		fitMesh(player);
		player.requireSceneRedraw();
	}

	/// Show the registered mesh formats and load the selected file; cancellation preserves the current scene.
	void openMesh (fcg::Player &player)
	{
		// Use the native blocking picker, keeping its result on the application thread
		auto selected = fcg::extra::showOpenFileDialog({
			.parent=player.mainWindow(), .title="Open mesh",
			.filters=fcg::extra::meshFileFilters(fcg::MeshLoader::global().fileFormats())
		});
		if (!selected) {
			reportError(std::format("File dialog failed: {}", selected.error().message));
			return;
		}
		if (!selected->paths.empty() && !selected->paths.front().empty())
			loadMesh(player.device(), selected->paths.front(), player);
	}

	/// Frame cached source bounds through the camera's sphere helper and choose matching clipping distances.
	void fitMesh (fcg::Player &player)
	{
		// Derive only the center and a padded radius; the camera performs the framing calculation
		const auto camera = player.activeCamera();
		if (!metadata.bounds || !camera)
			return;
		const auto &bounds = *metadata.bounds;
		const auto center = glm::vec3((bounds.minimum + bounds.maximum) * .5);
		const float radius = float(std::max(.001, 1.1 * glm::length((bounds.maximum - bounds.minimum) * .5)));
		try
		{
			if (!std::isfinite(radius))
				throw std::invalid_argument("Mesh bounds exceed the camera's coordinate range");
			(*camera)->translateToFocalPoint(center);
			(*camera)->adjustForSphereAtFocus(radius);
			const double distance = (*camera)->params().intrinsics.f;
			const float farPlane = float(distance + 3*radius);
			if (!std::isfinite(farPlane))
				throw std::invalid_argument("Mesh bounds exceed the camera's clipping range");
			(*camera)->setNearPlane(std::clamp(.001f, 1.f, .01f*radius));
			(*camera)->setFarPlane(farPlane);
			error.clear();
		}
		catch (const std::invalid_argument &exception) {
			reportError(std::format("Cannot fit to mesh: {}", exception.what()));
		}
	}

	/// Stage GPU geometry and pipelines before committing a surface-control selection.
	void changeRendering (fcg::Player &player, int nextShading, bool nextCull)
	{
		// A shading change requires geometry with the matching normal channel
		std::optional<fcg::GPUMesh> nextMesh;
		if (mesh && nextShading != shading) {
			auto uploaded = uploadMesh(player.device(), *mesh, nextShading);
			if (!uploaded) {
				reportError(uploaded.error().message);
				return;
			}
			nextMesh = std::move(*uploaded);
		}

		// Pipeline options and geometry always change together
		auto nextRenderer = fcg::MeshRenderer::create(player, layout(nextShading), {.cullMode=cullMode(nextCull)});
		if (!nextRenderer) {
			reportError(nextRenderer.error().message);
			return;
		}
		if (nextMesh)
			gpuMesh = std::move(*nextMesh);
		renderer = std::move(*nextRenderer);
		shading = nextShading;
		cullBackFaces = nextCull;
		error.clear();
		player.requireSceneRedraw();
	}


	////
	// Fields

	/// The path of the successfully loaded source mesh.
	std::filesystem::path meshFilepath;

	/// Independently owned source geometry, retained for inspection and shading changes.
	std::unique_ptr<fcg::Mesh> mesh;

	/// Source information collected once after loading.
	fcg::extra::MeshMetadata metadata;

	/// Immutable geometry uploaded with the current shading layout.
	std::optional<fcg::GPUMesh> gpuMesh;

	/// Pipeline owner matching the current geometry layout, target and culling setting.
	std::optional<fcg::MeshRenderer> renderer;

	/// Surface mode: file normals with fallback, flat, or regenerated smooth normals.
	int shading = 0;

	/// Whether to cull back-facing triangles; false enables two-sided lighting.
	bool cullBackFaces = false;

	/// Whether to apply the renderer's eye-space directional lighting.
	bool lighting = true;

	/// Most recent operation failure, displayed until a subsequent operation succeeds.
	std::string error;
};



//////
//
// Function implementations
//

/// Run the orbit camera and mesh viewer in a window titled <tt>"Mesh Viewer"</tt>.
auto main () -> int {
	return fcg::run<fcg::applet::OrbitCamera, MeshViewerApplet>({.mainWindowTitle="Mesh Viewer"});
}
