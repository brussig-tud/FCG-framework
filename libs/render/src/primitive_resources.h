
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
namespace fcg::detail {



//////
//
// Classes
//

/// Shared resources for indexed boxes and unindexed quad strips.
struct PrimitiveResources
{
	////
	// Object construction/destruction

	/// Adopt uploaded geometry and initialized dummy storage.
	PrimitiveResources(Device &device, Buffer vertices, std::optional<Buffer> indices, Buffer dummy, bool quad)
		: device(device), vertices(std::move(vertices)), indices(std::move(indices)),
		dummy(std::move(dummy)), quad(quad)
	{}

	/// Release pipelines without waiting for GPU idle.
	~PrimitiveResources();

	/// Resources remain uniquely owned by one renderer.
	PrimitiveResources(const PrimitiveResources&) = delete;

	/// Resources cannot be copied or reassigned.
	auto operator= (const PrimitiveResources&) -> PrimitiveResources& = delete;


	////
	// Methods

	/// Create geometry, dummy storage, and both pipelines transactionally.
	[[nodiscard]] static auto create (
		Device &device, const RenderTargetInfo &target, const RendererOptions &options, bool quad
	) -> std::expected<std::unique_ptr<PrimitiveResources>, RenderError>;

	/// Validate consumed sources and encode one instanced draw.
	[[nodiscard]] auto draw (
		const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *command,
		SDL_GPURenderPass *pass, const DrawOptions &options, InstanceRange range
	) const -> std::expected<void, RenderError>;


	////
	// Fields

	/// Borrowed device; outlives this object.
	Device &device;

	/// Immutable vertex geometry.
	Buffer vertices;

	/// Index allocation present only for boxes.
	std::optional<Buffer> indices;

	/// Initialized storage bound to unused attribute slots.
	Buffer dummy;

	/// Untextured and textured pipelines.
	std::array<SDL_GPUGraphicsPipeline*, 2> pipelines{};

	/// Whether this geometry is a quad strip with two-sided shading.
	bool quad;
};



//////
//
// Namespaces close
//

} // namespace fcg::detail


#endif // ifndef __FCG_RENDER_PRIMITIVE_RESOURCES_H__
