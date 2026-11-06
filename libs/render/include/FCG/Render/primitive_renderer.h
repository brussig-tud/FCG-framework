
#ifndef __FCG_RENDER_PRIMITIVE_RENDERER_H__
#define __FCG_RENDER_PRIMITIVE_RENDERER_H__


//////
//
// Library documentation
//

/// \page fcg_render_overview Render library overview
///
/// The Render library draws instanced quads, boxes, smooth ellipsoids, wire boxes and (optionally anti-aliased) lines
/// from user-definable GPU attributes; the quickest way to put many simple objects on the screen without writing your own
/// pipelines and shaders.
///
/// Link your application against \verbatim FCG-Framework::Render \endverbatim and include the renderer headers under
/// \c FCG/Render/:
/// \snippet render_examples.cpp includes
///
/// \section fcg_render_start Where to start
/// Start with the \ref fcg_primitives guide: it explains the shared attribute collection, the supplied renderers, and
/// how to create, update, and draw them. For spheres and ellipsoids, see \ref fcg_ellipsoids. For pixel-width line
/// segments between shared nodes, see \ref fcg_lines and \ref fcg_connected_primitives. A convenient, highly abstracted
/// way to use the line renderer is provided as well, see \ref fcg_line_sets for details.
/// For oriented box outlines, see \ref fcg_wire_boxes. For loaded or procedural meshes, see \ref fcg_gpu_meshes
/// and \ref fcg_mesh_rendering.



//////
//
// Module documentation
//

/// \defgroup fcg_primitives Instanced primitives
/// \ingroup fcg_modules
/// \brief
/// 	Drawing many geometric primitives of the same type at once according to provided per-instance attribute values.
///
/// A <tt>\ref fcg::PrimitiveAttributes "PrimitiveAttributes"</tt> collection holds the per-instance data — position,
/// extent, orientation, and color — on the GPU, and also offers optional normal and radius sources.
/// <tt>\ref fcg::LineRenderer "LineRenderer"</tt> uses node radii for pixel widths;
/// <tt>\ref fcg::WireBoxRenderer "WireBoxRenderer"</tt> uses per-box radii.
/// The renderers <tt>\ref fcg::QuadRenderer "QuadRenderer"</tt> and
/// <tt>\ref fcg::BoxRenderer "BoxRenderer"</tt> draw rectangles and cuboids.
/// <tt>\ref fcg::EllipsoidRenderer "EllipsoidRenderer"</tt> draws smooth ellipsoids. Each renderer uses one instanced
/// draw call. Missing optional attributes fall back to sensible defaults (unit extent, identity orientation, opaque
/// white); positions are always mandatory and thus determine actual the instance count.
/// \snippet render_examples.cpp attributes
///
/// \section fcg_primitives_usage Getting started
/// Create the attribute collection and a renderer bound to the main render pass, then draw into an active render
/// pass. Create the attribute collection in <tt>\ref fcg::Applet::init "Applet::init"</tt>, then create or replace
/// the renderer in <tt>\ref fcg::Applet::recreatePipelines "Applet::recreatePipelines"</tt> for the effective scene
/// format. Keep the attribute collection across these calls:
/// \snippet render_examples.cpp player
/// \snippet render_examples.cpp draw
///
/// \section fcg_primitives_workflows Common tasks
/// \subsection fcg_primitives_workflows_attributes Updating attributes
/// Change attributes with <tt>\ref fcg::PrimitiveAttributes::setAttributes "setAttributes"</tt>: inside its update
/// scope, assign whole arrays per <tt>\ref fcg::Attribute "attribute"</tt>. Changes are uploaded together when the
/// scope ends; a failed update leaves the previously uploaded data in place.
/// Typed attribute bytes are copied directly; <tt>\ref fcg::AttributeTraits</tt> describes the component layout,
/// including padded vector types and the quaternion ordering of <tt>\ref fcg::Attribute::Orientation
/// "Orientation"</tt>. Interleaved, borrowed GPU buffers are supported through
/// <tt>\ref fcg::AttributeBufferView "AttributeBufferView"</tt>.
///
/// \subsection fcg_primitives_workflows_renderers Creating renderers
/// Besides creating from a <tt>\ref fcg::Player "Player"</tt> as shown above, the explicit factory variant takes a
/// borrowed <tt>\ref fcg::Device "Device"</tt> and <tt>\ref fcg::RenderTargetInfo "RenderTargetInfo"</tt>, e.g. for
/// off-screen targets. A renderer keeps no reference to the player, so it keeps working while the window is
/// minimized. Pipeline settings such as culling, blending, and depth behavior are fixed at creation via
/// <tt>\ref fcg::RendererOptions "RendererOptions"</tt>; recreate the renderer to change them. Resizing the
/// attribute data needs no recreation.
///
/// \subsection fcg_primitives_workflows_drawing Drawing and appearance
/// Filled renderers support textures and lighting. Lines and wire boxes reject requests for those features.
/// <tt>\ref fcg::DrawOptions "DrawOptions"</tt> optionally binds one caller-created 2D texture and sampler; its RGBA
/// values multiply the instance colors. Filled renderers apply one directional light in eye space; boxes shade with
/// their flat face normals, quads shade with their plane normal and flip it on the back side, and ellipsoids use
/// their outward surface normals. Configure light direction, ambient and diffuse strengths, specular color and
/// shininess through <tt>\ref fcg::DirectionalLight "DirectionalLight"</tt>. Specular reflection is added
/// independently of instance and texture colors, preserving alpha.
/// <tt>\ref fcg::InstanceRange "InstanceRange"</tt> selects a contiguous subrange of the instances;
/// an empty range draws nothing.
///
/// \section fcg_primitives_lifetime Ownership and lifetime
/// The device must outlive the attribute collection and the renderers. The collection is not thread-safe; use it from
/// one thread at a time. Borrowed buffers passed to attribute updates must stay alive and unmoved while they are used.
/// Updates and draws never wait for GPU idle.
///
/// \section fcg_primitives_errors Handling errors
/// Factories and operations return <tt>std::expected&lt;T, \ref fcg::RenderError "RenderError"&gt;</tt>. Failed
/// updates keep the previous attribute data; a failed automatic submission makes the changed arrays unusable until
/// they are replaced successfully. Callback exceptions propagate after cleanup and record nothing. Errors carry a
/// human-readable message and never log. Renderer factories and draws rely on caller-guaranteed input validity;
/// invalid inputs violate their preconditions. Upload operations report the input errors described in their APIs.
///
/// \see \ref fcg_buffers, \ref fcg_textures, \ref fcg_applets, \ref fcg_render_state



//////
//
// Includes
//

// C++ STL
#include <expected>
#include <optional>
#include <span>

// FCG Framework
#include <FCG/Render/renderer_options.h>
#include <FCG/render_state.h>
#include <FCG/render_target.h>
#include <FCG/Render/primitive_attributes.h>



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
// Structs and enums
//

/// \brief A contiguous range of instances to draw.
struct InstanceRange {
	/// Index of the first instance to draw.
	std::size_t first = 0;

	/// Number of instances to draw. \c std::dynamic_extent draws all instances from \c first to the end.
	std::size_t count = std::dynamic_extent;
};



//////
//
// Classes
//

/// \brief Common interface of instanced primitive renderers and the connected renderer specialization.
///
/// A renderer draws one primitive per instance of a <tt>\ref PrimitiveAttributes</tt> collection, using the attributes
/// listed by <tt>\ref PrimitiveRenderer::supportedAttributes "supportedAttributes"</tt>. It draws into render passes
/// whose attachments match the target it was created for, at any size. When the formats or the sample count of the
/// target change, you must create a new renderer. <tt>\ref ConnectedPrimitiveRenderer</tt> adds explicit node
/// connectivity and requires its connected draw overload be used instead.
///
/// The device must outlive the renderer. Every buffer bound to a drawn collection with
/// <tt>\ref PrimitiveAttributes::Update::bind</tt> must stay alive while it is bound. Renderers are not thread-safe:
/// calls must not run concurrently with each other or with updates of the drawn collection. Drawing never waits for the
/// GPU to process the draw commands. Input preconditions are unchecked; violations result in undefined behavior.
/// A moved-from renderer permits only assignment and destruction.
class FCG_RENDER_EXPORT PrimitiveRenderer
{
public:

	////
	// Object construction/destruction

	/// Destroys the renderer without waiting for the GPU. Draws that were already submitted still complete.
	virtual ~PrimitiveRenderer();


	////
	// Accessors

	/// \brief Returns the attributes this renderer uses when drawing.
	///
	/// The sources of all other attributes in a drawn collection are ignored and need not cover the drawn range.
	///
	/// \return The used attributes.
	[[nodiscard]] virtual auto supportedAttributes () const -> std::span<const Attribute> = 0;

	/// \brief Returns whether this renderer uses an attribute when drawing.
	///
	/// \param attribute The attribute to query.
	///
	/// \return Whether \p attribute is one of the attributes returned by <tt>supportedAttributes</tt>.
	[[nodiscard]] auto supports (Attribute attribute) const -> bool;


	////
	// Methods

	/// \brief Draws one primitive for each instance in \p range into an active render pass.
	///
	/// Records a single instanced draw into \p renderPass. The renderer binds its pipeline and required buffers and
	/// samplers, and pushes vertex and fragment uniforms to \p commandBuffer, replacing the corresponding bindings
	/// and uniform data. A valid empty range succeeds without recording commands, subject to feature support.
	/// On error, nothing is recorded.
	///
	/// \pre
	/// 	The renderer, device, collections and used sources are live and usable. Collections, buffers and the
	/// 	nonnull command/pass belong to the renderer's device; the pass is active and has compatible attachments.
	/// \pre
	/// 	The range fits the position count and SDL's 32-bit draw/address limits. Every supplied consumed array
	/// 	covers the accessed elements; buffer layouts and values meet their attribute contracts.
	/// \pre
	/// 	Supported texture/sampler resources and enabled lighting values meet their documented contracts. Viewing
	/// 	matrices meet the concrete renderer's requirements. Calls follow command-buffer threading and collection
	/// 	lifetime rules.
	///
	/// \param attributes The instance attributes. Must belong to the renderer's device.
	/// \param state Supplies the current viewing matrices.
	/// \param commandBuffer The command buffer that contains \p renderPass.
	/// \param renderPass The active render pass to draw into.
	/// \param options Texture and lighting settings for this draw.
	/// \param range The instances to draw. The default draws all instances.
	///
	/// \return Success, or the unsupported-feature error documented by the concrete renderer; errors record nothing.
	[[nodiscard]] virtual auto draw (
		const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
		SDL_GPURenderPass *renderPass, const DrawOptions &options={}, InstanceRange range={}
	) const -> std::expected<void, RenderError> = 0;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_PRIMITIVE_RENDERER_H__
