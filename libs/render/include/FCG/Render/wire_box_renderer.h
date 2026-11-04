
#ifndef __FCG_RENDER_WIRE_BOX_RENDERER_H__
#define __FCG_RENDER_WIRE_BOX_RENDERER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_wire_boxes Wire boxes
/// \ingroup fcg_modules
/// \brief Draw oriented boxes as twelve independent pixel-width edge strokes.
///
/// Use <tt>\ref fcg::WireBoxRenderer "WireBoxRenderer"</tt> to display bounding volumes and box outlines. Supply
/// centers, half-extents, orientations and colors through <tt>\ref fcg::PrimitiveAttributes "PrimitiveAttributes"</tt>.
/// The same collection can also be drawn with <tt>\ref fcg::BoxRenderer "BoxRenderer"</tt>.
///
/// \section fcg_wire_boxes_usage Getting started
/// Link against \verbatim FCG-Framework::Render \endverbatim and include the renderer:
/// \snippet wire_box_examples.cpp includes
/// Upload attributes during application initialization, create the renderer in
/// <tt>\ref fcg::Applet::recreatePipelines "Applet::recreatePipelines"</tt>, and draw in an active pass:
/// \snippet wire_box_examples.cpp attributes
/// \snippet wire_box_examples.cpp factory
/// \snippet wire_box_examples.cpp draw
///
/// \section fcg_wire_boxes_workflows Common tasks
/// Set <tt>\ref fcg::Attribute::Radius "Radius"</tt> to a pixel half-width per box, or change the fallback full
/// width and caps through <tt>\ref fcg::LineStyle "LineStyle"</tt>:
/// \snippet wire_box_examples.cpp style
/// Enable <tt>\ref fcg::RendererOptions::alphaBlending "alphaBlending"</tt> for filtered stroke boundaries.
/// Each edge has independent caps; overlapping edges and corners can contribute multiple times when blending.
/// Zero half-extents retain all twelve strokes, including coincident edges and round-capped points.
///
/// All edges participate in ordinary depth testing against the scene. To hide rear edges behind a box surface,
/// first draw that surface into depth. For overlays, configure the depth comparison appropriate to the scene;
/// coincident surfaces can exhibit depth artifacts. See \ref fcg_lines for filtering and depth-write guidance.
/// For custom passes, synchronize the physical viewport dimensions with the GPU viewport:
/// \snippet wire_box_examples.cpp viewport



//////
//
// Includes
//

// C++ STL
#include <memory>

// FCG Framework
#include <FCG/Render/primitive_renderer.h>
#include <FCG/Render/line_style.h>



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

/// \addtogroup fcg_wire_boxes
/// @{



//////
//
// Classes
//

/// \brief Draw one twelve-edge wire box per instance of a \c PrimitiveAttributes collection.
///
/// Each box is centered at the homogeneous \c Attribute::Position, scaled by the local half-extents in
/// \c Attribute::Extent and rotated by <tt>Attribute::Orientation</tt>. Missing extents are (1,1,1), missing
/// orientations are identity, and missing colors are opaque white. \c Attribute::Radius supplies one physical
/// pixel half-width for every edge of a box; absent radii use half of <tt>LineStyle::defaultWidth</tt>.
/// Model transforms affect positions but not pixel widths. Normals are ignored.
///
/// Edges use the clipping, cap and collapsed-stroke behavior of <tt>LineRenderer</tt>. Coincident edges remain
/// independent. All twelve edges are drawn; box faces do not occlude them. Default culling is disabled; explicit
/// culling applies to the stroke triangles. Textures and lighting are unsupported.
///
/// Create in \c Applet::recreatePipelines and draw in an active compatible pass. Input preconditions are unchecked;
/// violations result in undefined behavior. A moved-from renderer permits only assignment and destruction.
/// The device must outlive the renderer. Calls never wait for GPU idle.
class FCG_RENDER_EXPORT WireBoxRenderer final : public PrimitiveRenderer
{
public:

	////
	// Object construction/destruction

	/// \brief Create a renderer for compatible attachment formats and sample count.
	///
	/// \pre The device is live, supports the target attachment usages/sample count, and outlives the renderer.
	/// \pre Renderer enum values are valid.
	///
	/// \param device The owning device.
	/// \param target Attachment formats and sample count; an invalid depth format disables depth testing/writes.
	/// \param options Fixed depth, blending and stroke-triangle culling settings; absent culling selects none.
	///
	/// \return
	/// 	The renderer, \c RenderErrorCode::SDLFailure on resource creation failure, or
	/// 	\c RenderErrorCode::InvalidState for missing shaders.
	[[nodiscard]] static auto create (
		Device &device, const RenderTargetInfo &target, const RendererOptions &options={}
	) -> std::expected<WireBoxRenderer, RenderError>;

	/// \brief Create a renderer for the player's main scene target.
	///
	/// \pre The player has a usable main target and meets the explicit factory's device and option preconditions.
	///
	/// \param player Supplies the target and device, which must outlive the renderer.
	/// \param options Fixed depth, blending and stroke-triangle culling settings.
	///
	/// \return The renderer, or the operational error described by the explicit factory.
	[[nodiscard]] static auto create (Player &player, const RendererOptions &options={})
		-> std::expected<WireBoxRenderer, RenderError>;

	/// Release resources without waiting for GPU idle; submitted draws still complete.
	~WireBoxRenderer() override;

	/// Not copyable.
	WireBoxRenderer(const WireBoxRenderer&) = delete;

	/// Not copy-assignable.
	auto operator= (const WireBoxRenderer&) -> WireBoxRenderer& = delete;

	/// \brief Move the renderer, leaving the source empty.
	///
	/// \param other The renderer to move.
	WireBoxRenderer(WireBoxRenderer &&other) noexcept;

	/// \brief Release current resources and move the source renderer into this object.
	///
	/// \param other The renderer to move, leaving it empty.
	///
	/// \return This renderer.
	auto operator= (WireBoxRenderer &&other) noexcept -> WireBoxRenderer&;


	////
	// Interface: PrimitiveRenderer

	/// Attributes consumed by boxes: position, extent, orientation, color and pixel radius.
	[[nodiscard]] auto supportedAttributes () const -> std::span<const Attribute> override;

	/// \brief Draw selected boxes; see \c PrimitiveRenderer::draw for the shared preconditions.
	///
	/// \pre The physical viewport dimensions are positive and match the GPU viewport with depth interval [0,1].
	/// \pre Viewing matrices are finite; supplied radii are finite and nonnegative.
	///
	/// \param attributes Per-box positions, optional half-extents, orientations, colors and pixel half-widths.
	/// \param state Viewing matrices and physical viewport dimensions.
	/// \param commandBuffer The command buffer containing the active pass.
	/// \param renderPass The compatible active render pass.
	/// \param options Texture and lighting requests; wire boxes support neither.
	/// \param range The box range; the default selects all boxes.
	///
	/// \return Success, or \c RenderErrorCode::InvalidArgument for unsupported features; errors record nothing.
	[[nodiscard]] auto draw (
		const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
		SDL_GPURenderPass *renderPass, const DrawOptions &options={}, InstanceRange range={}
	) const -> std::expected<void, RenderError> override;


	////
	// Accessors

	/// The style used by subsequent draws, borrowed until the next style change or destruction.
	[[nodiscard]] auto style () const -> const LineStyle&;


	////
	// Methods

	/// \brief Set the style used by subsequent draws.
	///
	/// \pre The renderer is live, width is finite and nonnegative, and cap/filter enum values are valid.
	///
	/// \param style The style to copy.
	void setStyle (const LineStyle &style);


private:

	////
	// Types

	/// Independently owned pipeline, fixed indices, placeholder storage and mutable style.
	struct State;


	////
	// Object construction/destruction

	/// \brief Take ownership of fully initialized resources.
	///
	/// \param state The created state.
	explicit WireBoxRenderer(std::unique_ptr<State> state) noexcept;


	////
	// Fields

	/// Owned rendering resources; null after moving.
	std::unique_ptr<State> state;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_WIRE_BOX_RENDERER_H__
