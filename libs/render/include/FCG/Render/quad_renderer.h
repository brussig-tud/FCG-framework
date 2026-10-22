
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

	namespace detail { // implementation details
		struct PrimitiveResources;
	}
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

/// \brief
/// 	Draws one filled quad per instance of a \c PrimitiveAttributes collection.
///
/// Each quad lies in its local XY plane, faces local +Z, is centered at the instance's \c Attribute::Position and is
/// rotated by its <tt>Attribute::Orientation</tt>. It extends by the X component of \c Attribute::Extent along local X
/// and by the Y component along local Y in both directions, so these components are the half size of the quad. The Z
/// component is ignored. Texture coordinate (0,0) lies at the local corner (-1,-1) and (1,1) at the
/// corner (+1,+1). Both sides are drawn unless \c RendererOptions::cullMode specifies otherwise. With lighting
/// enabled, the front side uses the normal +Z and the back side the flipped normal.
///
/// Create or replace the renderer in <tt>Applet::recreatePipelines</tt>, and call \c draw inside an active render pass. A
/// moved-from renderer is empty: only assignment and destruction are valid, and \c draw returns
/// <tt>RenderErrorCode::InvalidState</tt>.
class FCG_RENDER_EXPORT QuadRenderer final : public PrimitiveRenderer
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Creates a quad renderer for render passes with the given attachment formats and sample count.
	///
	/// The renderer can draw into any render pass whose attachments match \p target, regardless of the pass size. To
	/// draw into passes with other formats or another sample count, create another renderer.
	///
	/// \param device
	/// 	The device to create the renderer on. It must outlive the renderer.
	/// \param target
	/// 	Color format, depth format and sample count of the render passes to draw into. A depth format of
	/// 	\c SDL_GPU_TEXTUREFORMAT_INVALID means the passes have no depth attachment, which disables depth testing and
	/// 	depth writes.
	/// \param options
	/// 	Depth, culling and blending settings.
	///
	/// \return
	/// 	The renderer, or an error. The error is <tt>RenderErrorCode::InvalidArgument</tt> if the device does not
	/// 	support a format or the sample count of \p target, or if \p options holds an invalid culling mode or depth
	/// 	comparison. It is <tt>RenderErrorCode::InvalidState</tt> if \p device has been moved from, and
	/// 	<tt>RenderErrorCode::SDLFailure</tt> if creating the GPU resources failed.
	[[nodiscard]] static auto create (
		Device &device, const RenderTargetInfo &target, const RendererOptions &options={}
	) -> std::expected<QuadRenderer, RenderError>;

	/// \brief
	/// 	Creates a quad renderer for the main render pass of \p player.
	///
	/// Uses the device of \p player and the target described by <tt>Player::mainRenderTargetInfo</tt>. Call it from
	/// \c Applet::recreatePipelines; it also succeeds while the main window is minimized. The renderer keeps no reference
	/// to \p player. If the formats of the main render target change, create a new renderer.
	///
	/// \param player
	/// 	The player whose device and main render target are used. Its device must outlive the renderer.
	/// \param options
	/// 	Depth, culling and blending settings.
	///
	/// \return
	/// 	The renderer, or an error. The error is <tt>RenderErrorCode::InvalidState</tt> if \p player has no main
	/// 	window or the main window is not claimed by its device. Otherwise the errors are those of the overload
	/// 	taking a \c Device and a <tt>RenderTargetInfo</tt>.
	[[nodiscard]] static auto create (Player &player, const RendererOptions &options={})
		-> std::expected<QuadRenderer, RenderError>;

	/// Destroys the renderer without waiting for the GPU. Draws that were already submitted still complete.
	~QuadRenderer() override;

	/// Not copyable.
	QuadRenderer(const QuadRenderer&) = delete;

	/// Not copy-assignable.
	auto operator= (const QuadRenderer&) -> QuadRenderer& = delete;

	/// \brief
	/// 	Moves the renderer, leaving \p other empty.
	///
	/// \param other
	/// 	The renderer to move from.
	QuadRenderer(QuadRenderer &&other) noexcept;

	/// \brief
	/// 	Releases the resources of this renderer and moves \p other into this renderer, leaving \p other empty.
	///
	/// \param other
	/// 	The renderer to move from.
	///
	/// \return This renderer.
	auto operator= (QuadRenderer &&other) noexcept -> QuadRenderer&;


	////
	// Interface: PrimitiveRenderer

	/// Returns the attributes used by quads: <tt>Attribute::Position</tt>, <tt>Attribute::Extent</tt>,
	/// \c Attribute::Orientation and <tt>Attribute::Color</tt>.
	[[nodiscard]] auto supportedAttributes () const -> std::span<const Attribute> override;

	/// \brief
	/// 	Draws the quads of the selected instances; see \c PrimitiveRenderer::draw for the full contract.
	///
	/// \param attributes
	/// 	The instance attributes. Must belong to the renderer's device.
	/// \param state
	/// 	Supplies the current viewing matrices.
	/// \param commandBuffer
	/// 	The command buffer that contains \p renderPass.
	/// \param renderPass
	/// 	The active render pass to draw into.
	/// \param options
	/// 	Texture and lighting settings for this draw.
	/// \param range
	/// 	The instances to draw. The default draws all instances.
	///
	/// \return Success, or an error as described for <tt>PrimitiveRenderer::draw</tt>.
	[[nodiscard]] auto draw (
		const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
		SDL_GPURenderPass *renderPass, const DrawOptions &options={}, InstanceRange range={}
	) const -> std::expected<void, RenderError> override;


private:

	////
	// Object construction/destruction

	/// \brief
	/// 	Takes ownership of completely created resources; used by <tt>create</tt>.
	///
	/// \param resources
	/// 	The created geometry and pipelines.
	explicit QuadRenderer(std::unique_ptr<detail::PrimitiveResources> resources) noexcept;


	////
	// Fields

	/// Geometry and pipelines, using the implementation shared with <tt>BoxRenderer</tt>. Null when moved from.
	std::unique_ptr<detail::PrimitiveResources> resources;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_QUAD_RENDERER_H__
