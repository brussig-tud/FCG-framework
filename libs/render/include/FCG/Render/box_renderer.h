
#ifndef __FCG_RENDER_BOX_RENDERER_H__
#define __FCG_RENDER_BOX_RENDERER_H__


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

/// Indexed box with 24 face vertices, 36 indices, flat outward normals, and independent UV seams.
class FCG_RENDER_EXPORT BoxRenderer final : public PrimitiveRenderer
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
	) -> std::expected<BoxRenderer, RenderError>;

	/// Snapshot the player's main-pass target before the first frame or while minimized.
	///
	/// \param player Source of the borrowed device and target description; never retained.
	/// \param options Depth, culling, and blending settings.
	///
	/// \return A renderer, or \c RenderErrorCode::InvalidState without a valid main-window target.
	[[nodiscard]] static auto create (Player &player, const RendererOptions &options={})
		-> std::expected<BoxRenderer, RenderError>;

	/// Release geometry and pipelines without waiting for GPU idle.
	~BoxRenderer() override;

	/// Renderers cannot be copied.
	BoxRenderer(const BoxRenderer&) = delete;

	/// Renderers cannot be copy-assigned.
	auto operator= (const BoxRenderer&) -> BoxRenderer& = delete;

	/// Transfer ownership, leaving an inert source.
	BoxRenderer(BoxRenderer&&) noexcept;

	/// Release previous resources and transfer ownership.
	auto operator= (BoxRenderer&&) noexcept -> BoxRenderer&;


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
	explicit BoxRenderer(std::unique_ptr<detail::PrimitiveResources> resources) noexcept;


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


#endif // ifndef __FCG_RENDER_BOX_RENDERER_H__
