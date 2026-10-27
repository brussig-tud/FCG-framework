
#ifndef __FCG_RENDER_ELLIPSOID_RENDERER_H__
#define __FCG_RENDER_ELLIPSOID_RENDERER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_ellipsoids Ellipsoids
/// \ingroup fcg_modules
/// \brief Draw smooth ellipsoids with surface lighting, textures, and depth.
///
/// Use <tt>\ref fcg::EllipsoidRenderer "EllipsoidRenderer"</tt> for spheres and rotated ellipsoids, for example
/// particle or tensor glyphs. Supply their centers and semi-axis lengths through
/// <tt>\ref fcg::PrimitiveAttributes "PrimitiveAttributes"</tt>. See \ref fcg_primitives for attribute updates,
/// buffer ownership, instance ranges, and shared drawing options.
///
/// \section fcg_ellipsoids_usage Getting started
/// Include \c FCG/Render/ellipsoid_renderer.h and link against \verbatim FCG-Framework::Render \endverbatim.
/// Create the renderer in <tt>\ref fcg::Applet::recreatePipelines "Applet::recreatePipelines"</tt>:
/// \snippet render_examples.cpp ellipsoid_factory
/// Upload centers and draw into a compatible active pass with the current camera matrices:
/// \snippet render_examples.cpp ellipsoids
///
/// \section fcg_ellipsoids_workflows Common tasks
/// Set all three components of <tt>\ref fcg::Attribute::Extent "Extent"</tt> to the same positive value for
/// spheres, or use different values for ellipsoids. Their values are radii, in the same units as the centers.
/// Set <tt>\ref fcg::Attribute::Orientation "Orientation"</tt> to rotate the local axes and the texture.
///
/// Enable <tt>\ref fcg::DrawOptions::lighting "DrawOptions::lighting"</tt> for directional surface lighting.
/// Bind a texture through <tt>\ref fcg::DrawOptions::texture "DrawOptions::texture"</tt>; longitude and latitude
/// mapping is described by <tt>\ref fcg::EllipsoidRenderer "EllipsoidRenderer"</tt>.
///
/// Perspective and orthographic cameras are supported. Supply finite, invertible affine modelview matrices and
/// use viewport depth bounds of 0 and 1. Nonuniform scale and shear are supported. To see the surface from inside
/// an ellipsoid, create the renderer with <tt>\ref fcg::RendererOptions::cullMode "cullMode"</tt> set to
/// <tt>SDL_GPU_CULLMODE_NONE</tt>. Alpha blending draws one visible surface per ellipsoid in instance order;
/// arrange transparent instances from far to near when needed.



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

/// \addtogroup fcg_ellipsoids
/// @{



//////
//
// Classes
//

/// \brief Draws smooth, filled ellipsoids from a \c PrimitiveAttributes collection.
///
/// Each ellipsoid is centered at \c Attribute::Position and has local semi-axis lengths given by
/// <tt>Attribute::Extent</tt>, rotated by <tt>Attribute::Orientation</tt>. A missing extent produces a unit sphere.
/// An extent with any zero component produces no fragments. Lighting uses the outward surface normal.
///
/// Texture coordinates use longitude around local +Y and latitude from +Y to -Y: U is 0.5 on +X and 0.75 on +Z;
/// V is 0 at +Y and 1 at -Y. The seam lies on -X. At the poles U is 0.5. Repeat addressing in U and clamp addressing
/// in V are suitable sampler settings.
///
/// Back faces are culled by default. With culling disabled, a viewer inside an ellipsoid sees its exit surface.
/// Each ellipsoid contributes its nearest surface surviving clipping and culling, including when alpha blending
/// is enabled. Clipping leaves open surfaces; no cap is drawn. Surface depth is used for depth testing and writing.
///
/// Create or replace the renderer in <tt>\ref Applet::recreatePipelines</tt> and call \c draw inside an active render
/// pass. A moved-from renderer is empty; \c draw returns <tt>RenderErrorCode::InvalidState</tt>.
class FCG_RENDER_EXPORT EllipsoidRenderer final : public PrimitiveRenderer
{
public:

	////
	// Object construction/destruction

	/// \brief Creates an ellipsoid renderer for render passes with the given attachment formats and sample count.
	///
	/// The renderer can draw into any render pass whose attachments match \p target, regardless of the pass size. To
	/// draw into passes with other formats or another sample count, create another renderer.
	///
	/// \param device The device to create the renderer on. It must outlive the renderer.
	/// \param target
	/// 	Color format, depth format and sample count of the render passes to draw into. A depth format of
	/// 	\c SDL_GPU_TEXTUREFORMAT_INVALID means the passes have no depth attachment, which disables depth testing and
	/// 	depth writes.
	/// \param options Depth, culling and blending settings.
	///
	/// \return
	/// 	The renderer, or an error. The error is <tt>RenderErrorCode::InvalidArgument</tt> if the device does not
	/// 	support a format or the sample count of \p target, or if \p options holds an invalid culling mode or depth
	/// 	comparison. It is <tt>RenderErrorCode::InvalidState</tt> if \p device has been moved from, and
	/// 	<tt>RenderErrorCode::SDLFailure</tt> if creating the GPU resources failed.
	[[nodiscard]] static auto create (
		Device &device, const RenderTargetInfo &target, const RendererOptions &options={}
	) -> std::expected<EllipsoidRenderer, RenderError>;

	/// \brief Creates an ellipsoid renderer for the main render pass of \p player.
	///
	/// Uses the device of \p player and the target described by <tt>Player::mainRenderTargetInfo</tt>. Call it from
	/// \c Applet::recreatePipelines; it also succeeds while the main window is minimized. The renderer keeps no
	/// reference to \p player. If the formats of the main render target change, create a new renderer.
	///
	/// \param player The player whose device and main render target are used. Its device must outlive the renderer.
	/// \param options Depth, culling and blending settings.
	///
	/// \return
	/// 	The renderer, or an error. The error is <tt>RenderErrorCode::InvalidState</tt> if \p player has no main
	/// 	window or the main window is not claimed by its device. Otherwise the errors are those of the overload
	/// 	taking a \c Device and a <tt>RenderTargetInfo</tt>.
	[[nodiscard]] static auto create (Player &player, const RendererOptions &options={})
		-> std::expected<EllipsoidRenderer, RenderError>;

	/// Destroys the renderer without waiting for the GPU. Draws that were already submitted still complete.
	~EllipsoidRenderer() override;

	/// Not copyable.
	EllipsoidRenderer(const EllipsoidRenderer&) = delete;

	/// Not copy-assignable.
	auto operator= (const EllipsoidRenderer&) -> EllipsoidRenderer& = delete;

	/// \brief Moves the renderer, leaving \p other empty.
	///
	/// \param other The renderer to move from.
	EllipsoidRenderer(EllipsoidRenderer &&other) noexcept;

	/// \brief Releases the resources of this renderer and moves \p other into this renderer, leaving \p other empty.
	///
	/// \param other The renderer to move from.
	///
	/// \return This renderer.
	auto operator= (EllipsoidRenderer &&other) noexcept -> EllipsoidRenderer&;


	////
	// Interface: PrimitiveRenderer

	/// Returns the attributes used by ellipsoids: <tt>Attribute::Position</tt>, <tt>Attribute::Extent</tt>,
	/// \c Attribute::Orientation and <tt>Attribute::Color</tt>.
	[[nodiscard]] auto supportedAttributes () const -> std::span<const Attribute> override;

	/// \brief Draws the ellipsoids of the selected instances; see \c PrimitiveRenderer::draw for the full contract.
	///
	/// \param attributes The instance attributes. Must belong to the renderer's device.
	/// \param state
	/// 	Supplies finite, invertible viewing matrices: an affine modelview and a conventional perspective or
	/// 	orthographic projection with zero-to-one depth. The viewport depth bounds must be 0 and 1.
	/// \param commandBuffer The command buffer that contains \p renderPass.
	/// \param renderPass The active render pass to draw into.
	/// \param options Texture and lighting settings for this draw.
	/// \param range The instances to draw. The default draws all instances.
	///
	/// \return
	/// 	Success, or an error as described for <tt>PrimitiveRenderer::draw</tt>. Additionally returns
	/// 	<tt>RenderErrorCode::InvalidArgument</tt> for nonfinite or singular viewing matrices or a non-affine modelview.
	[[nodiscard]] auto draw (
		const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
		SDL_GPURenderPass *renderPass, const DrawOptions &options={}, InstanceRange range={}
	) const -> std::expected<void, RenderError> override;


private:

	////
	// Object construction/destruction

	/// \brief Takes ownership of completely created resources; used by <tt>create</tt>.
	///
	/// \param resources The created geometry and pipelines.
	explicit EllipsoidRenderer(std::unique_ptr<detail::PrimitiveResources> resources) noexcept;


	////
	// Fields

	/// Owned geometry and pipelines. Null when moved from.
	std::unique_ptr<detail::PrimitiveResources> resources;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_ELLIPSOID_RENDERER_H__
