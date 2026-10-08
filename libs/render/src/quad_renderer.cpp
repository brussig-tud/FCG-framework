
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
#include <FCG/Render/quad_renderer.h>

// Local includes
#include "primitive_resources.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Attributes consumed by <tt>fcg::QuadRenderer</tt>, in storage-slot order.
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
// QuadRenderer

QuadRenderer::QuadRenderer(std::unique_ptr<detail::PrimitiveResources> resources) noexcept
	: resources(std::move(resources))
{}

QuadRenderer::~QuadRenderer() = default;

QuadRenderer::QuadRenderer(QuadRenderer&&) noexcept = default;

auto QuadRenderer::operator= (QuadRenderer&&) noexcept -> QuadRenderer& = default;

auto QuadRenderer::create (Device &device, const RenderTargetInfo &target, const RendererOptions &options)
	-> std::expected<QuadRenderer, RenderError>
{
	auto resources = detail::PrimitiveResources::create(device, target, options, true);
	if (!resources)
		return std::unexpected(std::move(resources.error()));
	return QuadRenderer(std::move(*resources));
}

auto QuadRenderer::create (Player &player, const RendererOptions &options) -> std::expected<QuadRenderer, RenderError>
{
	const auto target = player.mainRenderTargetInfo();
	if (!target)
		return std::unexpected(RenderError{
			RenderErrorCode::InvalidState, "The provided player has no valid main-window render target"
		});
	return create(player.device(), *target, options);
}

auto QuadRenderer::supportedAttributes () const -> std::span<const Attribute> {
	return consumed;
}

auto QuadRenderer::draw (
	const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
	SDL_GPURenderPass *renderPass, const DrawOptions &options, InstanceRange range
) const -> std::expected<void, RenderError>
{
	if (!resources)
		return std::unexpected(RenderError{
			RenderErrorCode::InvalidState, "Renderer has been moved from"
		});
	return resources->draw(attributes, state, commandBuffer, renderPass, options, range);
}



//////
//
// Module namespace close
//

} // namespace fcg
