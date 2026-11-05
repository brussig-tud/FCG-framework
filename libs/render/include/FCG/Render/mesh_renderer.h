
#ifndef __FCG_RENDER_MESH_RENDERER_H__
#define __FCG_RENDER_MESH_RENDERER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_mesh_rendering Mesh rendering
/// \ingroup fcg_modules
/// \brief Drawing GPU meshes with colors, textures and optional directional lighting.
///
/// Create a <tt>\ref fcg::MeshRenderer "MeshRenderer"</tt> for the layout reported by
/// <tt>\ref fcg::GPUMesh "GPUMesh"</tt>, or select a layout before loading any meshes.
///
/// \section fcg_mesh_rendering_usage Getting started
/// Create pipelines for your scene target and draw within its active pass:
/// \snippet mesh_render_examples.cpp renderer
///
/// \section fcg_mesh_rendering_workflows Common tasks
/// Upload several meshes with one explicit layout to share a renderer. Change the modelview matrix for each object.
/// Recreate pipelines when target formats or sample count change. Depth testing and writing are enabled when the
/// target has depth; alpha blending is optional. Counterclockwise faces are front-facing; back faces are culled by
/// default. Disabling culling illuminates visible back faces with reversed normals.
/// Colors are unlit by default. Optional lighting uses the eye-space direction toward a directional light, with
/// ambient and diffuse intensities. A texture multiplies the mesh and draw colors and requires a UV layout.
/// Draw preconditions are unchecked; a mismatch results in undefined behavior.



//////
//
// Includes
//

// C++ STL
#include <array>
#include <expected>
#include <optional>

// Framework
#include <FCG/render_state.h>
#include <FCG/render_target.h>
#include <FCG/Render/error.h>
#include <FCG/Render/export.h>
#include <FCG/Render/gpu_mesh_layout.h>
#include <FCG/Render/renderer_options.h>



//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class GPUMesh;
	class Player;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_mesh_rendering
/// @{



//////
//
// Structs and enums
//

/// Per-draw mesh color, texturing, and optional diffuse illumination.
struct MeshDrawOptions
{
	/// Linear RGBA multiplier applied to mesh colors and the optional texture.
	glm::vec4 color{1.f};

	/// Optional borrowed 2D texture and sampler; requires a layout containing UVs.
	std::optional<PrimitiveTexture> texture;

	/// Eye-space ambient/directional diffuse lighting, disabled by default.
	DirectionalLight lighting;
};



//////
//
// Classes
//

/// Draws immutable GPU meshes sharing one canonical vertex layout and device.
class FCG_RENDER_EXPORT MeshRenderer
{
public:

	////
	// Object construction/destruction

	/// Create pipelines for a layout before any mesh has been uploaded.
	///
	/// \pre The device supports the target and outlives the renderer. Option enum values are valid.
	///
	/// \param device Borrowed creating device.
	/// \param target Attachment formats and sample count of compatible passes.
	/// \param layout Exact layout accepted by subsequent draws.
	/// \param options Pipeline settings; unspecified culling means back-face culling.
	///
	/// \return Complete renderer, or an SDL resource or missing-shader error.
	[[nodiscard]] static auto create (
		Device &device, const RenderTargetInfo &target, GPUMeshLayout layout, const RendererOptions &options={}
	) -> std::expected<MeshRenderer, RenderError>;

	/// Create pipelines for an uploaded mesh without retaining a reference to it.
	///
	/// \pre The mesh's device supports the target and outlives the renderer.
	///
	/// \param mesh Mesh supplying the device and layout.
	/// \param target Attachment formats and sample count of compatible passes.
	/// \param options Pipeline settings.
	///
	/// \return Renderer or an operational creation error.
	[[nodiscard]] static auto create (
		const GPUMesh &mesh, const RenderTargetInfo &target, const RendererOptions &options={}
	) -> std::expected<MeshRenderer, RenderError>;

	/// Create pipelines for a player's main target, including while minimized.
	///
	/// \pre The player has a usable target and its device outlives the renderer.
	///
	/// \param player Player supplying the device and target.
	/// \param layout Exact accepted GPU vertex format.
	/// \param options Pipeline settings.
	///
	/// \return Renderer or an operational creation error.
	[[nodiscard]] static auto create (Player &player, GPUMeshLayout layout, const RendererOptions &options={})
		-> std::expected<MeshRenderer, RenderError>;

	/// Release pipelines without waiting for submitted GPU work.
	~MeshRenderer();

	/// Not copyable.
	MeshRenderer(const MeshRenderer&) = delete;

	/// Not copy-assignable.
	auto operator= (const MeshRenderer&) -> MeshRenderer& = delete;

	/// Transfer pipeline ownership, leaving the source suitable for assignment or destruction.
	///
	/// \param other Renderer to move.
	MeshRenderer(MeshRenderer &&other) noexcept;

	/// Replace pipelines by transferring ownership.
	///
	/// \param other Renderer to move; self-assignment is supported.
	///
	/// \return This renderer.
	auto operator= (MeshRenderer &&other) noexcept -> MeshRenderer&;


	////
	// Accessors

	/// Return the exact vertex format required by draws.
	[[nodiscard]] auto layout () const -> GPUMeshLayout { return m_layout; }


	////
	// Methods

	/// Record one indexed mesh draw without waiting or allocating resources.
	///
	/// \pre Mesh layout and device match the renderer; all resources and objects are live.
	/// \pre The render pass belongs to the command buffer and matches the renderer's target formats and samples.
	/// \pre Texturing requires UVs and a live sampler/texture on this device through command submission.
	/// \pre Viewing matrices and lighting satisfy their public contracts. All preconditions are unchecked.
	///
	/// \param mesh Borrowed mesh whose buffers remain alive through command submission.
	/// \param state Current modelview and projection transformations.
	/// \param commandBuffer Unsubmitted command buffer used on its acquiring thread.
	/// \param renderPass Active compatible render pass.
	/// \param options Color multiplier, optional texture and optional lighting.
	void draw (
		const GPUMesh &mesh, RenderState &state, SDL_GPUCommandBuffer *commandBuffer, SDL_GPURenderPass *renderPass,
		const MeshDrawOptions &options={}
	) const;


private:

	////
	// Object construction

	/// Construct an empty pipeline owner during creation.
	///
	/// \param device Borrowed creating device.
	/// \param layout Accepted mesh format.
	MeshRenderer(Device &device, GPUMeshLayout layout) : m_device(&device), m_layout(layout) {}


	////
	// Fields

	/// Borrowed creating device; null after a move.
	Device *m_device;

	/// Accepted canonical vertex layout.
	GPUMeshLayout m_layout;

	/// Untextured pipeline followed by the optional textured pipeline.
	std::array<SDL_GPUGraphicsPipeline*, 2> m_pipelines{};

	/// Whether visible back faces receive reversed lighting normals.
	bool m_twoSided = false;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // __FCG_RENDER_MESH_RENDERER_H__
