
#ifndef __FCG_RENDER_PRIMITIVE_RESOURCES_H__
#define __FCG_RENDER_PRIMITIVE_RESOURCES_H__


//////
//
// Includes
//

// C++ STL
#include <array>
#include <memory>
#include <optional>

// FCG Framework
#include <FCG/Render/primitive_renderer.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// The library's namespace for internal implementation details shared between translation units.
namespace detail {



//////
//
// Classes
//

/// \brief
/// 	GPU resources and draw implementation shared by \c BoxRenderer and <tt>QuadRenderer</tt>.
///
/// Boxes use an indexed triangle list of 24 vertices and 36 indices. Quads use an unindexed triangle strip of 4
/// vertices and shade both sides.
struct PrimitiveResources
{
	////
	// Object construction/destruction

	/// \brief
	/// 	Takes ownership of uploaded geometry and initialized placeholder storage. The pipelines are added by
	/// 	<tt>create</tt>.
	///
	/// \param device
	/// 	The device owning all resources. Must outlive this object.
	/// \param vertices
	/// 	The uploaded vertex buffer.
	/// \param indices
	/// 	The uploaded index buffer for boxes, or empty for quads.
	/// \param dummy
	/// 	Zero-initialized storage buffer bound to storage slots of attributes without a buffer source.
	/// \param quad
	/// 	Whether the geometry is a quad strip, which selects two-sided lighting.
	PrimitiveResources(Device &device, Buffer vertices, std::optional<Buffer> indices, Buffer dummy, bool quad)
		: device(device), vertices(std::move(vertices)), indices(std::move(indices)),
		dummy(std::move(dummy)), quad(quad)
	{}

	/// Releases the pipelines without waiting for the GPU.
	~PrimitiveResources();

	/// Not copyable; the resources are uniquely owned by one renderer.
	PrimitiveResources(const PrimitiveResources&) = delete;

	/// Not copy-assignable.
	auto operator= (const PrimitiveResources&) -> PrimitiveResources& = delete;


	////
	// Methods

	/// \brief
	/// 	Creates the geometry, the placeholder storage and the untextured and textured pipelines.
	///
	/// Either everything is created or an error is returned and nothing is retained.
	///
	/// \param device
	/// 	The device to create the resources on. Must outlive the result.
	/// \param target
	/// 	Attachment formats and sample count of compatible render passes.
	/// \param options
	/// 	Depth, culling and blending settings.
	/// \param quad
	/// 	Whether to create quad geometry instead of box geometry.
	///
	/// \return The created resources, or the error described for <tt>BoxRenderer::create</tt>.
	[[nodiscard]] static auto create (
		Device &device, const RenderTargetInfo &target, const RendererOptions &options, bool quad
	) -> std::expected<std::unique_ptr<PrimitiveResources>, RenderError>;

	/// \brief
	/// 	Validates the used attribute sources and the draw options, then records one instanced draw.
	///
	/// Implements \c PrimitiveRenderer::draw for both renderers; nothing is recorded on error.
	///
	/// \param attributes
	/// 	The instance attributes.
	/// \param state
	/// 	Supplies the viewing matrices.
	/// \param command
	/// 	The command buffer containing \p pass.
	/// \param pass
	/// 	The active render pass.
	/// \param options
	/// 	Texture and lighting settings.
	/// \param range
	/// 	The instances to draw.
	///
	/// \return Success, or the error described for <tt>PrimitiveRenderer::draw</tt>.
	[[nodiscard]] auto draw (
		const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *command,
		SDL_GPURenderPass *pass, const DrawOptions &options, InstanceRange range
	) const -> std::expected<void, RenderError>;


	////
	// Fields

	/// The device owning all resources; not owned and must outlive this object.
	Device &device;

	/// Immutable vertex geometry.
	Buffer vertices;

	/// Immutable 16-bit index buffer; present only for boxes.
	std::optional<Buffer> indices;

	/// Zero-initialized storage bound to the slots of attributes without a buffer source.
	Buffer dummy;

	/// The untextured pipeline at index 0 and the textured pipeline at index 1.
	std::array<SDL_GPUGraphicsPipeline*, 2> pipelines{};

	/// Whether the geometry is a quad strip with two-sided lighting.
	bool quad;
};



//////
//
// Namespaces close
//

} // namespace detail

} // namespace fcg


#endif // ifndef __FCG_RENDER_PRIMITIVE_RESOURCES_H__
