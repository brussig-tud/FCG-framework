
#ifndef __FCG_RENDER_PRIMITIVE_RENDERER_H__
#define __FCG_RENDER_PRIMITIVE_RENDERER_H__


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

/// \page fcg_render_guide Render library guide
///
/// Link against <tt>FCG-Framework\::Render</tt>, which publicly links Core. Include the concrete renderer headers
/// below.
/// \snippet render_examples.cpp includes
///
/// A <tt>\ref fcg::PrimitiveAttributes</tt> collection shares GPU instance data between <tt>\ref fcg::QuadRenderer</tt>
/// and <tt>\ref fcg::BoxRenderer</tt>. Position determines the instance count; extent is a local half size, orientation
/// is a quaternion, and color is RGBA. Missing optional sources default to unit extent, identity orientation, and
/// white. The initial renderers ignore normal and radius.
/// \snippet render_examples.cpp attributes
///
/// <tt>\ref fcg::PrimitiveAttributes::setAttributes</tt> invokes a scoped
/// <tt>\ref fcg::PrimitiveAttributes::Update</tt> proxy, then validates final assignments and uploads changed arrays
/// together. The last assignment wins; omitted sources remain. Input arrays must stay valid and unchanged until the
/// call returns. No CPU array mirror is retained. GPU capacity and staging are reused across updates. Typed object
/// bytes are copied directly; <tt>\ref fcg::AttributeTraits</tt> describes component offsets, including padded vector
/// types and quaternion ordering. <tt>\ref fcg::AttributeBufferView</tt> supports interleaved borrowed buffers with
/// word-aligned offsets and strides; owners must remain alive and unmoved.
///
/// Create a renderer from a <tt>\ref fcg::Player</tt> during initialization, before the first frame.
/// <tt>\ref fcg::Player::mainRenderTargetInfo</tt> returns the main-pass attachments even while minimized. Missing or
/// unclaimed windows produce <tt>\ref fcg::RenderErrorCode::InvalidState</tt>.
/// \snippet render_examples.cpp player
///
/// Alternatively use the explicit factory with a borrowed <tt>\ref fcg::Device</tt> and
/// <tt>\ref fcg::RenderTargetInfo</tt>. An invalid depth format means no depth attachment and disables depth
/// tests and writes. Defaults are depth testing and writing with LESS, back-face culling for boxes, no culling for
/// quads, and disabled blending. <tt>\ref fcg::RendererOptions</tt> can override culling or enable straight-alpha
/// blending. Renderer resources retain no player. Resize needs no recreation; formats or sample-count changes do.
///
/// Draw in an active compatible render pass with <tt>\ref fcg::RenderState</tt> viewing matrices.
/// The nonempty draw issues one instanced draw; <tt>\ref fcg::InstanceRange</tt> selects a contiguous subrange.
/// Draws allocate no resources or array uploads. Arrays for consumed sources must cover the requested range.
/// \snippet render_examples.cpp draw
///
/// <tt>\ref fcg::DrawOptions</tt> optionally borrows one caller-created 2D texture and sampler. Texture RGBA multiplies
/// instance RGBA. Quads use UV (0,0) at local (-1,-1) and (1,1) at (+1,+1). Each box face has independent UVs: +X uses
/// -Z/+Y, -X +Z/+Y, +Y +X/-Z, -Y +X/+Z, +Z +X/+Y, -Z -X/+Y. Lighting uses oriented flat normals transformed by Core's
/// normal matrix; quad back faces flip their normal. The light direction points toward the light in eye space. RGB is
/// multiplied by ambient plus diffuse times the clamped normal/light dot product; base alpha is preserved. Defaults are
/// +Z, ambient 0.2, and diffuse 0.8.
///
/// Transforms require finite homogeneous positions with nonzero W, finite nonnegative extents, and finite nonzero
/// orientations. CPU replacements are validated; borrowed GPU arrays have the same caller-owned preconditions.
/// Degenerate zero extents are allowed. Geometry is scaled by extent, rotated by normalized orientation, and translated
/// to the homogeneous center.
///
/// Devices outlive resources. Access is externally serialized; recursive updates fail. Callback exceptions propagate
/// after cleanup and record nothing. Validation, allocation, and staging failure preserve previous metadata and clear
/// pending staging. Already recorded writes cannot be rolled back. Failed automatic submission makes changed arrays
/// unusable until successfully replaced. The supplied-copy-pass overload reports recording success only: end the pass,
/// submit the command buffer, and resupply changed arrays after cancellation or failed submission. Standard C++
/// allocation exceptions may propagate after cleanup. <tt>\ref fcg::RenderError</tt> owns its diagnostic. Updates and
/// draws never wait for GPU idle.
///
/// This version supports filled primitives, one color target, and one texture per draw. Custom shaders, per-face
/// materials, transparent-instance sorting, and interpretation of radius or user normals are future capabilities.

/// \addtogroup fcg_primitives
/// @{



//////
//
// Structs and enums
//

/// Pipeline settings resolved when a renderer is created.
struct RendererOptions
{
	/// Enable depth comparisons when the target has depth.
	bool depthTest = true;

	/// Enable depth writes when the target has depth.
	bool depthWrite = true;

	/// Depth comparison function.
	SDL_GPUCompareOp depthCompare = SDL_GPU_COMPAREOP_LESS;

	/// Explicit culling; absence chooses back-face culling for boxes and no culling for quads.
	std::optional<SDL_GPUCullMode> cullMode;

	/// Enable straight-alpha blending; instances are drawn in array order.
	bool alphaBlending = false;
};

/// Optional borrowed texture and sampler; caller creates, uploads, and retains both resources.
struct PrimitiveTexture {
	/// Sampled two-dimensional color texture on the renderer's device.
	SDL_GPUTexture *texture = nullptr;

	/// Sampler on the renderer's device.
	SDL_GPUSampler *sampler = nullptr;
};

/// Eye-space directional lighting, preserving the base alpha.
struct DirectionalLight
{
	/// Enable generated geometry normals and directional lighting.
	bool enabled = false;

	/// Finite, nonzero direction toward the light, normalized when drawing.
	glm::vec3 direction{0.f, 0.f, 1.f};

	/// Finite RGB ambient intensity.
	glm::vec3 ambient{.2f};

	/// Finite RGB diffuse intensity.
	glm::vec3 diffuse{.8f};
};

/// Per-draw texture and lighting choices; no uploads or pipeline creation occur during drawing.
struct DrawOptions {
	/// Optional borrowed texture/sampler pair; multiplies RGBA instance color.
	std::optional<PrimitiveTexture> texture;

	/// Disabled by default; quads flip back-face normals for two-sided lighting.
	DirectionalLight lighting;
};

/// Contiguous instance range; counts are validated only for attributes consumed by the renderer.
struct InstanceRange {
	/// First instance in the shared collection.
	std::size_t first = 0;

	/// Number of instances; \c std::dynamic_extent means all remaining instances.
	std::size_t count = std::dynamic_extent;
};



//////
//
// Classes
//

/// Common interface for instanced primitive renderers.
///
/// Renderers borrow a device and retain pipelines. Resize alone requires no recreation; changed target formats or
/// sample count require recreating the renderer. Devices outlive all resources. Attribute buffers remain stable and
/// alive during use. Access must be externally serialized.
class FCG_RENDER_EXPORT PrimitiveRenderer
{
public:

	////
	// Object construction/destruction

	/// Release concrete resources through the interface without waiting for idle.
	virtual ~PrimitiveRenderer();


	////
	// Accessors

	/// Logical attributes consumed by this renderer; other collection sources are ignored.
	[[nodiscard]] virtual auto supportedAttributes () const -> std::span<const Attribute> = 0;

	/// Test whether a logical attribute is consumed.
	///
	/// \param attribute The logical attribute to query.
	///
	/// \return Whether the supported list contains the attribute.
	[[nodiscard]] auto supports (Attribute attribute) const -> bool;


	////
	// Methods

	/// Encode one instanced draw for a nonempty range into an active compatible render pass.
	///
	/// \pre
	/// 	The command buffer and render pass belong to the renderer's device. The pass matches its target.
	/// 	GPU-supplied transforms satisfy the \c Attribute value contracts. The state describes valid viewing matrices.
	///
	/// \param attributes Shared collection on the renderer's device.
	/// \param state Core viewing state; lazy viewing matrices may be evaluated.
	/// \param commandBuffer Command buffer owning the render pass.
	/// \param renderPass Active render pass with matching attachments.
	/// \param options Borrowed texture and lighting settings.
	/// \param range Requested contiguous instance range; the default draws all instances.
	///
	/// \return Encoding success or a recoverable validation/state error.
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
