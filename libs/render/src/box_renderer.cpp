
//////
//
// Includes
//

// C++ STL
#include <array>
#include <expected>
#include <memory>
#include <span>
#include <utility>

// FCG Framework
#include <FCG/player.h>
#include <FCG/Render/box_renderer.h>

// Local includes
#include "primitive_resources.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Attributes consumed by <tt>fcg::BoxRenderer</tt>, in storage-slot order.
constexpr std::array consumed{
	fcg::Attribute::Position, fcg::Attribute::Extent, fcg::Attribute::Orientation, fcg::Attribute::Color
};

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// BoxRenderer

BoxRenderer::BoxRenderer(std::unique_ptr<detail::PrimitiveResources> resources) noexcept
	: resources(std::move(resources))
{}

BoxRenderer::~BoxRenderer() = default;

BoxRenderer::BoxRenderer(BoxRenderer&&) noexcept = default;

auto BoxRenderer::operator= (BoxRenderer&&) noexcept -> BoxRenderer& = default;

auto BoxRenderer::create (Device &device, const RenderTargetInfo &target, const RendererOptions &options)
	-> std::expected<BoxRenderer, RenderError>
{
	auto resources = detail::PrimitiveResources::create(device, target, options, detail::PrimitiveKind::Box);
	if (!resources)
		return std::unexpected(std::move(resources.error()));
	return BoxRenderer(std::move(*resources));
}

auto BoxRenderer::create (Player &player, const RendererOptions &options) -> std::expected<BoxRenderer, RenderError> {
	return create(player.device(), *player.mainRenderTargetInfo(), options);
}

auto BoxRenderer::supportedAttributes () const -> std::span<const Attribute> {
	return consumed;
}

auto BoxRenderer::draw (
	const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
	SDL_GPURenderPass *renderPass, const DrawOptions &options, InstanceRange range
) const -> std::expected<void, RenderError> {
	return resources->draw(attributes, state, commandBuffer, renderPass, options, range);
}



//////
//
// Module namespace close
//

} // namespace fcg
