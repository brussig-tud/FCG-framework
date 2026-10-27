
#ifndef __FCG_RENDER_CONNECTED_PRIMITIVE_RENDERER_H__
#define __FCG_RENDER_CONNECTED_PRIMITIVE_RENDERER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_connected_primitives Connected primitives
/// \ingroup fcg_modules
/// \brief Draw primitives whose endpoints share attributes through explicit node indices.
///
/// Use <tt>\ref fcg::PrimitiveAttributes "PrimitiveAttributes"</tt> for node positions and optional attributes,
/// and <tt>\ref fcg::PrimitiveConnectivity "PrimitiveConnectivity"</tt> to describe which nodes form primitives.
/// <tt>\ref fcg::ConnectedPrimitiveRenderer "ConnectedPrimitiveRenderer"</tt> supplies the common draw interface.
/// For line segments, start with \ref fcg_lines.
///
/// \section fcg_connected_primitives_usage Getting started
/// Upload node indices after creating the node attributes:
/// \snippet line_examples.cpp connectivity
/// Each renderer specifies the number of consecutive indices per primitive. Lines use pairs; repeated indices
/// share node attributes without joining adjacent strokes.
///
/// \section fcg_connected_primitives_workflows Common tasks
/// Reuse one connectivity collection with multiple draws, or replace its indices with
/// <tt>\ref fcg::PrimitiveConnectivity::setIndices "setIndices"</tt>. To read caller-managed GPU indices, use
/// <tt>\ref fcg::PrimitiveConnectivity::bind "bind"</tt> with a
/// <tt>\ref fcg::ConnectivityBufferView "ConnectivityBufferView"</tt>.
/// <tt>\ref fcg::InstanceRange "InstanceRange"</tt> selects connected primitives rather than nodes.
///
/// All documented input preconditions must hold; violations result in undefined behavior. In particular, each
/// accessed index must fit every supplied attribute array that the renderer consumes. See \ref fcg_primitives for
/// attribute updates and \ref fcg_lines for pixel widths, caps and blending.



//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// FCG Framework
#include <FCG/Render/primitive_renderer.h>
#include <FCG/Render/primitive_connectivity.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_connected_primitives
/// @{



//////
//
// Classes
//

/// \brief Common interface for primitives assembled from explicitly indexed attribute-carrying nodes.
///
/// A connected draw emits one instance per group of \c nodesPerPrimitive indices. The device must outlive the
/// renderer and collections. Input preconditions are unchecked; violations result in undefined behavior.
class FCG_RENDER_EXPORT ConnectedPrimitiveRenderer : public PrimitiveRenderer
{
public:

	////
	// Object construction/destruction

	/// Destroy the connected renderer without waiting for GPU idle.
	~ConnectedPrimitiveRenderer() override;


	////
	// Interface: PrimitiveRenderer

	/// \brief Report that this renderer requires explicit connectivity.
	///
	/// \param attributes The node attributes.
	/// \param state The rendering state.
	/// \param commandBuffer The command buffer.
	/// \param renderPass The render pass.
	/// \param options The drawing options.
	/// \param range The primitive range.
	///
	/// \return Always \c RenderErrorCode::InvalidArgument; nothing is recorded.
	[[nodiscard]] auto draw (
		const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
		SDL_GPURenderPass *renderPass, const DrawOptions &options={}, InstanceRange range={}
	) const -> std::expected<void, RenderError> final;


	////
	// Accessors

	/// Number of consecutive node indices forming one primitive.
	[[nodiscard]] virtual auto nodesPerPrimitive () const -> std::size_t = 0;


	////
	// Methods

	/// \brief Draw the selected connected primitives into an active render pass.
	///
	/// A valid empty range succeeds without recording anything. Draws bind their own pipeline, buffers and uniforms.
	/// Calls never wait for GPU idle.
	///
	/// \pre
	/// 	The renderer, device and all used sources are live and usable. Both collections and all buffers belong to
	/// 	the renderer's device. Buffer layouts, sizes and addresses meet their source contracts and SDL's limits.
	/// \pre
	/// 	The connectivity count is divisible by \c nodesPerPrimitive. The range fits its primitive count and SDL's
	/// 	draw limits. Every accessed index fits the position source and each supplied consumed attribute array.
	/// \pre
	/// 	The nonnull command buffer contains the active, nonnull render pass on the same device. Its attachments
	/// 	match the renderer's target. Calls are confined to the command buffer's owning thread, without concurrent
	/// 	updates or draws involving these objects.
	///
	/// \param attributes The node attributes; unconsumed attributes are ignored.
	/// \param connectivity The explicit node indices grouped into primitives.
	/// \param state The viewing matrices and viewport used by this draw.
	/// \param commandBuffer The command buffer containing the render pass.
	/// \param renderPass The active render pass.
	/// \param options The texture and lighting request, subject to the concrete renderer's support.
	/// \param range The connected primitives to draw; the default selects all.
	///
	/// \return Success, or the feature errors documented by the concrete renderer; errors record nothing.
	[[nodiscard]] virtual auto draw (
		const PrimitiveAttributes &attributes, const PrimitiveConnectivity &connectivity, RenderState &state,
		SDL_GPUCommandBuffer *commandBuffer, SDL_GPURenderPass *renderPass, const DrawOptions &options={},
		InstanceRange range={}
	) const -> std::expected<void, RenderError> = 0;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_CONNECTED_PRIMITIVE_RENDERER_H__
