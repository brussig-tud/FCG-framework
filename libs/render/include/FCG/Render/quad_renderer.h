
#ifndef __FCG_RENDER_QUAD_RENDERER_H__
#define __FCG_RENDER_QUAD_RENDERER_H__


//////
//
// Includes
//

// C++ STL
#include <memory>

// FCG Framework
#include <FCG/Render/primitive_renderer.h>



//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class Player;
}

// Implementation details
namespace fcg::detail {
	struct PrimitiveResources;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_primitives
/// @{



//////
//
// Classes
//

/// Unindexed four-vertex triangle strip with local +Z normals and XY half extents.
class FCG_RENDER_EXPORT QuadRenderer final : public PrimitiveRenderer
{
public:

	////
	// Object construction/destruction

	/// Create both texture pipelines and immutable geometry for an explicitly described target.
	///
	/// \param device Borrowed device that outlives this renderer.
	/// \param target Pipeline attachment formats and sample count.
	/// \param options Depth, culling, and blending settings.
	///
	/// \return A complete renderer or an owned diagnostic.
	[[nodiscard]] static auto create (
		Device &device, const RenderTargetInfo &target, const RendererOptions &options={}
	) -> std::expected<QuadRenderer, RenderError>;

	/// Snapshot the player's main-pass target before the first frame or while minimized.
	///
	/// \param player Source of the borrowed device and target description; never retained.
	/// \param options Depth, culling, and blending settings.
	///
	/// \return A renderer, or \c RenderErrorCode::InvalidState without a valid main-window target.
	[[nodiscard]] static auto create (Player &player, const RendererOptions &options={})
		-> std::expected<QuadRenderer, RenderError>;

	/// Release geometry and pipelines without waiting for GPU idle.
	~QuadRenderer() override;

	/// Renderers cannot be copied.
	QuadRenderer(const QuadRenderer&) = delete;

	/// Renderers cannot be copy-assigned.
	auto operator= (const QuadRenderer&) -> QuadRenderer& = delete;

	/// Transfer ownership, leaving an inert source.
	QuadRenderer(QuadRenderer&&) noexcept;

	/// Release previous resources and transfer ownership.
	auto operator= (QuadRenderer&&) noexcept -> QuadRenderer&;


	////
	// Interface: PrimitiveRenderer

	/// Position, extent, orientation, and color; normal and radius are ignored.
	[[nodiscard]] auto supportedAttributes () const -> std::span<const Attribute> override;

	/// Encode an instanced draw; see \c PrimitiveRenderer::draw for parameter and lifetime contracts.
	[[nodiscard]] auto draw (
		const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
		SDL_GPURenderPass *renderPass, const DrawOptions &options={}, InstanceRange range={}
	) const -> std::expected<void, RenderError> override;


private:

	////
	// Object construction/destruction

	/// Adopt fully initialized private resources.
	explicit QuadRenderer(std::unique_ptr<detail::PrimitiveResources> resources) noexcept;


	////
	// Fields

	/// Shared implementation of indexed and unindexed geometry and pipeline resources.
	std::unique_ptr<detail::PrimitiveResources> resources;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_QUAD_RENDERER_H__
