
#ifndef __FCG_RENDER_LINE_RENDERER_H__
#define __FCG_RENDER_LINE_RENDERER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_lines Lines
/// \ingroup fcg_modules
/// \brief Draw independent 3D line segments with pixel widths and filtered edges.
///
/// <tt>\ref fcg::LineRenderer "LineRenderer"</tt> connects pairs of nodes from
/// <tt>\ref fcg::PrimitiveAttributes "PrimitiveAttributes"</tt> using
/// <tt>\ref fcg::PrimitiveConnectivity "PrimitiveConnectivity"</tt>. Node radii are half-widths in physical
/// framebuffer pixels, independent of camera distance. Node colors vary along each segment.
///
/// \section fcg_lines_usage Getting started
/// Include the line renderer and create it for the current scene target:
/// \snippet line_examples.cpp includes
/// \snippet line_examples.cpp factory
/// Upload node attributes and index pairs, then draw in an active pass:
/// \snippet line_examples.cpp nodes
/// \snippet line_examples.cpp connectivity
/// \snippet line_examples.cpp draw
///
/// \section fcg_lines_workflows Common tasks
/// Enable <tt>\ref fcg::RendererOptions::alphaBlending "alphaBlending"</tt> for smooth edges. The shared default
/// leaves blending disabled and draws hard edges. For blended lines, draw after opaque geometry, disable depth
/// writes when appropriate and arrange transparent segments from far to near. Fractional coverage with depth writes
/// uses one depth per fragment, including at stroke edges; it cannot represent visibility through uncovered parts
/// of a pixel. Segments are drawn in index order without sorting.
///
/// Set <tt>\ref fcg::LineStyle "LineStyle"</tt> outside the common draw interface:
/// \snippet line_examples.cpp style
/// Round caps are the default; butt caps terminate at the node. Separate segments sharing a node remain independent
/// strokes, and their filtered edges can overlap. Missing radii use the style's default width; missing colors use
/// opaque white. Widths below one pixel are allowed, with approximate filtering.
///
/// The runtime captures the viewport size in <tt>\ref fcg::RenderState "RenderState"</tt>. For a custom pass, supply
/// its physical viewport dimensions and keep them synchronized with the GPU viewport:
/// \snippet line_examples.cpp offscreen
/// The viewport depth interval must be [0,1]. See \ref fcg_connected_primitives for index ownership and ranges.



//////
//
// Includes
//

// C++ STL
#include <memory>

// FCG Framework
#include <FCG/Render/connected_primitive_renderer.h>



//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class Player;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_lines
/// @{



//////
//
// Structs and enums
//

/// \brief Shape of a line segment's endpoint boundary.
enum class LineCap {
	/// An outward semicircle with the endpoint's pixel radius.
	Round,

	/// A straight boundary perpendicular to the projected segment.
	Butt
};

/// \brief Method used to compute coverage at a stroke boundary.
enum class LineFilter {
	/// Hard geometric coverage, including when alpha blending is enabled.
	None,

	/// A derivative-scaled smooth transition, applied when alpha blending is enabled.
	Smoothstep
};

/// \brief Appearance of independent line segments, changed through \c LineRenderer::setStyle.
struct LineStyle
{
	/// Full stroke width in physical pixels when \c Attribute::Radius is absent; finite and nonnegative.
	float defaultWidth = 1.f;

	/// Endpoint shape; clipping intersections always use butt boundaries.
	LineCap cap = LineCap::Round;

	/// Coverage filter; blending-disabled pipelines always draw hard edges.
	LineFilter filter = LineFilter::Smoothstep;
};



//////
//
// Classes
//

/// \brief Draw independent segments between pairs of attribute-carrying nodes.
///
/// Each pair selects two homogeneous positions, pixel half-widths and RGBA colors. The projected body tapers linearly
/// between its radii, with semicircular or butt ends. Colors use perspective-correct interpolation. Model transforms
/// affect positions but not the pixel widths. Normals, extents and orientations are ignored.
///
/// Segments are clipped to the depth interval before projection; clipping intersections have butt ends. Both zero radii
/// produce no fragments. A projected length below 0.0001 pixels produces a disk for round ends, using the maximum
/// radius and the nearer endpoint's color/depth; equal depth selects the first endpoint. Such a segment with butt ends
/// produces no fragments.
///
/// Create the renderer in <tt>\ref Applet::recreatePipelines</tt> and draw in an active render pass. Input
/// preconditions are unchecked, and violations result in undefined behavior. A moved-from renderer permits only
/// assignment and /// destruction. The device must outlive the renderer. Calls never wait for GPU idle.
class FCG_RENDER_EXPORT LineRenderer final : public ConnectedPrimitiveRenderer
{
public:

	////
	// Object construction/destruction

	/// \brief Create a renderer for the given attachment formats and sample count.
	///
	/// \pre
	/// 	The device is live and supports the target's color/depth formats and sample count for their attachment
	/// 	usages. Renderer enum values are valid. The device outlives the renderer.
	///
	/// \param device The owning device.
	/// \param target Compatible render-pass formats and sample count; an invalid depth format means no depth.
	/// \param options Fixed depth, culling and blending options; absent culling selects no culling.
	///
	/// \return
	/// 	The renderer, \c RenderErrorCode::SDLFailure if resource creation fails, or
	/// 	\c RenderErrorCode::InvalidState if required renderer resources are unavailable.
	[[nodiscard]] static auto create (
		Device &device, const RenderTargetInfo &target, const RendererOptions &options={}
	) -> std::expected<LineRenderer, RenderError>;

	/// \brief Create a renderer for the player's main scene target.
	///
	/// \pre
	/// 	The player has a usable main-window render target and meets the explicit factory's device and option
	/// 	preconditions. The player's device outlives the renderer.
	///
	/// \param player The player supplying the device and scene target.
	/// \param options Fixed depth, culling and blending options.
	///
	/// \return The renderer, or the resource error described by the explicit factory.
	[[nodiscard]] static auto create (Player &player, const RendererOptions &options={})
		-> std::expected<LineRenderer, RenderError>;

	/// Release resources without waiting for GPU idle; submitted draws still complete.
	~LineRenderer() override;

	/// Not copyable.
	LineRenderer(const LineRenderer&) = delete;

	/// Not copy-assignable.
	auto operator= (const LineRenderer&) -> LineRenderer& = delete;

	/// \brief Move the renderer, leaving the source empty.
	///
	/// \param other The renderer to move.
	LineRenderer(LineRenderer &&other) noexcept;

	/// \brief Release current resources and move the source renderer into this object.
	///
	/// \param other The renderer to move, leaving it empty.
	///
	/// \return This renderer.
	auto operator= (LineRenderer &&other) noexcept -> LineRenderer&;


	////
	// Interface: PrimitiveRenderer

	/// Attributes consumed by lines: \c Attribute::Position, \c Attribute::Radius and \c Attribute::Color.
	[[nodiscard]] auto supportedAttributes () const -> std::span<const Attribute> override;

	/// Expose the inherited overload reporting missing connectivity.
	using ConnectedPrimitiveRenderer::draw;


	////
	// Interface: ConnectedPrimitiveRenderer

	/// Two nodes form each independent segment.
	[[nodiscard]] auto nodesPerPrimitive () const -> std::size_t override;

	/// \brief Draw the selected index pairs; see \c ConnectedPrimitiveRenderer::draw for shared contracts.
	///
	/// \pre
	/// 	For nonempty draws, the viewport dimensions are positive and match the GPU viewport, whose depth interval
	/// 	is [0,1]. Viewing matrices and node values meet their finite-value contracts. Radii are nonnegative.
	///
	/// \param attributes The node positions, optional pixel radii and optional colors.
	/// \param connectivity Consecutive node-index pairs.
	/// \param state Viewing matrices and physical viewport dimensions.
	/// \param commandBuffer The command buffer containing the active pass.
	/// \param renderPass The compatible active pass.
	/// \param options Texture and lighting requests; lines support neither.
	/// \param range The segment range, rather than a node range; the default selects all.
	///
	/// \return Success, or \c RenderErrorCode::InvalidArgument for texture or lighting requests; errors record nothing.
	[[nodiscard]] auto draw (
		const PrimitiveAttributes &attributes, const PrimitiveConnectivity &connectivity, RenderState &state,
		SDL_GPUCommandBuffer *commandBuffer, SDL_GPURenderPass *renderPass, const DrawOptions &options={},
		InstanceRange range={}
	) const -> std::expected<void, RenderError> override;


	////
	// Accessors

	/// The style used by subsequent draws, borrowed until the next style change or destruction.
	[[nodiscard]] auto style () const -> const LineStyle&;


	////
	// Methods

	/// \brief Set the style for subsequent draws without recreating the renderer.
	///
	/// \pre The renderer is live; width is finite and nonnegative, and cap/filter enum values are valid.
	///
	/// \param style The style to copy.
	void setStyle (const LineStyle &style);


private:

	////
	// Types

	/// The line pipeline, placeholder storage and mutable appearance.
	struct State;


	////
	// Object construction/destruction

	/// \brief Take ownership of fully initialized resources.
	///
	/// \param state The created state.
	explicit LineRenderer(std::unique_ptr<State> state) noexcept;


	////
	// Fields

	/// Owned line resources; null after moving the renderer.
	std::unique_ptr<State> state;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_LINE_RENDERER_H__
