
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
#include <FCG/Render/ellipsoid_renderer.h>

// Local includes
#include "primitive_resources.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Attributes consumed by <tt>fcg::EllipsoidRenderer</tt>, in storage-slot order.
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
// EllipsoidRenderer

EllipsoidRenderer::EllipsoidRenderer(std::unique_ptr<detail::PrimitiveResources> resources) noexcept
	: resources(std::move(resources))
{}

EllipsoidRenderer::~EllipsoidRenderer() = default;

EllipsoidRenderer::EllipsoidRenderer(EllipsoidRenderer&&) noexcept = default;

auto EllipsoidRenderer::operator= (EllipsoidRenderer&&) noexcept -> EllipsoidRenderer& = default;

auto EllipsoidRenderer::create (Device &device, const RenderTargetInfo &target, const RendererOptions &options)
	-> std::expected<EllipsoidRenderer, RenderError> {
	auto resources = detail::PrimitiveResources::create(device, target, options, detail::PrimitiveKind::Ellipsoid);
	if (!resources)
		return std::unexpected(std::move(resources.error()));
	return EllipsoidRenderer(std::move(*resources));
}

auto EllipsoidRenderer::create (Player &player, const RendererOptions &options)
	-> std::expected<EllipsoidRenderer, RenderError>
{
	const auto target = player.mainRenderTargetInfo();
	if (!target)
		return std::unexpected(RenderError{
			RenderErrorCode::InvalidState, "Player has no valid main-window render target"
		});
	return create(player.device(), *target, options);
}

auto EllipsoidRenderer::supportedAttributes () const -> std::span<const Attribute> {
	return consumed;
}

auto EllipsoidRenderer::draw (
	const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
	SDL_GPURenderPass *renderPass, const DrawOptions &options, InstanceRange range
) const -> std::expected<void, RenderError> {
	if (!resources)
		return std::unexpected(RenderError{RenderErrorCode::InvalidState, "Renderer has been moved from"});
	return resources->draw(attributes, state, commandBuffer, renderPass, options, range);
}



//////
//
// Module namespace close
//

} // namespace fcg
