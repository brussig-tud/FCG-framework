
#ifndef __FCG_RENDER_PRIMITIVE_RENDERER_H__
#define __FCG_RENDER_PRIMITIVE_RENDERER_H__


//////
//
// Library documentation
//

/// \page fcg_render_overview Render library overview
///
/// The Render library draws instanced quads, boxes, smooth ellipsoids and (optionally anti-aliased) lines from
/// user-definable GPU attributes; the quickest way to put many simple objects on the screen without writing your own
/// pipelines and shaders.
///
/// Link your application against \verbatim FCG-Framework::Render \endverbatim and include the renderer headers under
/// \c FCG/Render/:
/// \snippet render_examples.cpp includes
///
/// \section fcg_render_start Where to start
/// Start with the \ref fcg_primitives guide: it explains the shared attribute collection, the supplied renderers, and
/// how to create, update, and draw them. For spheres and ellipsoids, see \ref fcg_ellipsoids. For pixel-width line
/// segments between shared nodes, see \ref fcg_lines and \ref fcg_connected_primitives.



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
/// <tt>\ref fcg::LineRenderer "LineRenderer"</tt> uses node radii for pixel widths.
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
/// <tt>\ref fcg::RendererOptions "RendererOptions"</tt>; recreate the renderer to change them. Resizing the attribute data needs no recreation.
///
/// \subsection fcg_primitives_workflows_drawing Drawing and appearance
/// <tt>\ref fcg::DrawOptions "DrawOptions"</tt> optionally binds one caller-created 2D texture and sampler; its RGBA
/// values multiply the instance colors. The renderers apply one directional light in eye space; boxes shade with
/// their flat face normals, quads shade with their plane normal and flip it on the back side, and ellipsoids use
/// their outward surface normals. The light direction, ambient, and diffuse strengths are configurable.
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
/// human-readable message and never log.
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

/// \brief Pipeline settings of a primitive renderer.
///
/// The settings are fixed when the renderer is created. To change them, create a new renderer.
struct RendererOptions
{
	/// Whether depth testing is enabled. Has no effect if the target has no depth attachment.
	bool depthTest = true;

	/// Whether depth writing is enabled. Has no effect if the target has no depth attachment.
	bool depthWrite = true;

	/// The comparison used for depth testing.
	SDL_GPUCompareOp depthCompare = SDL_GPU_COMPAREOP_LESS;

	/// \brief The face culling mode.
	///
	/// If empty, \c BoxRenderer and \c EllipsoidRenderer cull back faces; \c QuadRenderer and \c LineRenderer cull
	/// nothing. The outward sides of boxes and ellipsoids and the +Z sides of quads are their front faces.
	std::optional<SDL_GPUCullMode> cullMode;

	/// \brief Whether colors are blended using straight (not premultiplied) alpha.
	///
	/// When enabled, the output RGB is <tt>src.rgb * src.a + dst.rgb * (1 - src.a)</tt> and the output alpha is
	/// <tt>src.a + dst.a * (1 - src.a)</tt>. Instances are drawn in the order of the collection and are not sorted by
	/// depth. Ellipsoids contribute at most one surface point per instance at each sample.
	bool alphaBlending = false;
};

/// \brief A texture and a sampler for drawing primitives textured, set via <tt>DrawOptions::texture</tt>.
///
/// You create both resources, and they must stay alive at least until the command buffer containing the draw has
/// been submitted.
struct PrimitiveTexture
{
	/// The texture, sampled at the texture coordinates of the primitive. Must be a 2D texture with sampler usage,
	/// created on the renderer's device. Must not be null.
	SDL_GPUTexture *texture = nullptr;

	/// The sampler used to sample <tt>texture</tt>. Must be created on the renderer's device. Must not be null.
	SDL_GPUSampler *sampler = nullptr;
};

/// \brief A directional light for simple diffuse lighting of primitives, set via <tt>DrawOptions::lighting</tt>.
///
/// With lighting enabled, the RGB part of each fragment color is multiplied by
/// <tt>ambient + diffuse * max(dot(n, l), 0)</tt>, where \c n is the unit normal of the surface in eye space and
/// \c l is the normalized <tt>direction</tt>. The alpha is unchanged. Boxes use the flat outward normal of each face.
/// Quads use their +Z normal on the front side and the flipped normal on the back side. Ellipsoids use their
/// outward surface normal.
struct DirectionalLight
{
	/// Whether lighting is applied. If <tt>false</tt>, the other fields are ignored and colors are drawn unlit.
	bool enabled = false;

	/// Direction from the surface toward the light, in eye space. Must be finite and nonzero when lighting is enabled;
	/// its length does not matter. The default (0, 0, 1) points toward the viewer.
	glm::vec3 direction{0.f, 0.f, 1.f};

	/// RGB intensity of the ambient term. Must be finite when lighting is enabled.
	glm::vec3 ambient{.2f};

	/// RGB intensity of the diffuse term. Must be finite when lighting is enabled.
	glm::vec3 diffuse{.8f};
};

/// \brief Texture and lighting settings for a single <tt>PrimitiveRenderer::draw</tt> call.
struct DrawOptions {
	/// Optional texture. If set, the instance color is multiplied by the texture color.
	std::optional<PrimitiveTexture> texture;

	/// Lighting settings. Lighting is disabled by default.
	DirectionalLight lighting;
};

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
/// GPU to process the draw commands.
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
	/// Records a single instanced draw into \p renderPass. The renderer binds its own pipeline, vertex buffer, storage
	/// buffers and, when texturing, sampler on \p renderPass, and pushes its own vertex and fragment uniform data to
	/// \p commandBuffer, replacing what was bound or pushed before. If \p range is valid but selects no instances, the
	/// call succeeds and records nothing. On error, nothing is recorded.
	///
	/// \pre
	/// 	\p commandBuffer and \p renderPass belong to the renderer's device, \p renderPass is active, and its
	/// 	attachments match the target the renderer was created for. Values in buffers bound to used attributes with
	/// 	<tt>PrimitiveAttributes::Update::bind</tt> meet the requirements of the respective <tt>Attribute</tt>.
	///
	/// \param attributes The instance attributes. Must belong to the renderer's device.
	/// \param state Supplies the current viewing matrices.
	/// \param commandBuffer The command buffer that contains \p renderPass.
	/// \param renderPass The active render pass to draw into.
	/// \param options Texture and lighting settings for this draw.
	/// \param range The instances to draw. The default draws all instances.
	///
	/// \return
	/// 	Success, or an error. The error is <tt>RenderErrorCode::InvalidArgument</tt> if \p attributes belongs to
	/// 	another device, if \p commandBuffer or \p renderPass is null, if \p range extends beyond the instance count
	/// 	of \p attributes or selects more than 2^32 - 1 instances, if an array of a used attribute has fewer
	/// 	elements than the end of \p range, if \p options holds a texture or sampler that is null, or if enabled
	/// 	lighting has a nonfinite or zero direction or nonfinite intensities. The error is
	/// 	<tt>RenderErrorCode::InvalidState</tt> if the renderer has been moved from, if a used attribute is not
	/// 	usable (see <tt>PrimitiveAttributes::usable</tt>), or if a buffer bound to a used attribute has been moved
	/// 	from or no longer meets the requirements of <tt>AttributeBufferView</tt>.
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
