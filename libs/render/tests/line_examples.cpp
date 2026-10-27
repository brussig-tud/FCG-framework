
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstdint>
#include <span>

// FCG Framework
/// [includes]
#include <FCG/Render/line_renderer.h>
#include <FCG/player.h>
/// [includes]



//////
//
// Functions
//

/// \brief Create a renderer for smooth lines drawn after opaque scene geometry.
///
/// \param player The player whose main target will receive the lines.
///
/// \return The created renderer or an operational creation error.
/// [factory]
[[nodiscard]] auto createLines (fcg::Player &player) -> std::expected<fcg::LineRenderer, fcg::RenderError> {
	return fcg::LineRenderer::create(player, {.depthWrite = false, .alphaBlending = true});
}
/// [factory]

/// \brief Upload nodes for two independently capped segments sharing their middle node.
///
/// \param attributes The collection receiving positions, radii and colors.
///
/// \return Success or an upload error.
/// [nodes]
[[nodiscard]] auto lineNodes (fcg::PrimitiveAttributes &attributes) -> std::expected<void, fcg::RenderError>
{
	const std::array positions{glm::vec4(-1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(1, 0, 0, 1)};
	const std::array colors{glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1), glm::vec4(0, 0, 1, 1)};
	return attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.set<fcg::Attribute::Position>(std::span(positions));
		update.set<fcg::Attribute::Radius>(2.f);
		update.set<fcg::Attribute::Color>(std::span(colors));
	});
}
/// [nodes]

/// \brief Upload pairs selecting two independent segments through three nodes.
///
/// \param connectivity The collection receiving the index pairs.
///
/// \return Success or an upload error.
/// [connectivity]
[[nodiscard]] auto linePairs (fcg::PrimitiveConnectivity &connectivity) -> std::expected<void, fcg::RenderError> {
	const std::array<std::uint32_t, 4> indices{0, 1, 1, 2};
	return connectivity.setIndices(indices);
}
/// [connectivity]

/// \brief Draw segments through the common connected interface.
///
/// \param lines The renderer compatible with the pass.
/// \param nodes The shared node attributes.
/// \param pairs The segment index pairs.
/// \param state The current matrices and physical viewport dimensions.
/// \param commands The command buffer containing the active pass.
/// \param pass The active compatible pass.
///
/// \return Success or a feature error.
/// [draw]
[[nodiscard]] auto drawLines (
	const fcg::ConnectedPrimitiveRenderer &lines, const fcg::PrimitiveAttributes &nodes,
	const fcg::PrimitiveConnectivity &pairs, fcg::RenderState &state,
	SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass
) -> std::expected<void, fcg::RenderError> {
	return lines.draw(nodes, pairs, state, commands, pass);
}
/// [draw]

/// \brief Choose butt caps and the fallback width for nodes without radius attributes.
///
/// \param lines The live renderer to configure.
/// [style]
void styleLines (fcg::LineRenderer &lines) {
	auto style = lines.style();
	style.cap = fcg::LineCap::Butt;
	style.defaultWidth = 3.f;
	lines.setStyle(style);
}
/// [style]

/// \brief Draw lines into a caller-created pass covering a custom target.
///
/// \param device The device shared by renderer, nodes, connectivity and pass.
/// \param lines The renderer created for the custom target's formats.
/// \param nodes The node attributes.
/// \param pairs The segment index pairs.
/// \param size The physical dimensions of the active GPU viewport.
/// \param commands The command buffer containing the pass.
/// \param pass The active compatible pass.
///
/// \return Success or a feature error.
/// [offscreen]
[[nodiscard]] auto offscreenLines (
	fcg::Device &device, const fcg::LineRenderer &lines, const fcg::PrimitiveAttributes &nodes,
	const fcg::PrimitiveConnectivity &pairs, glm::uvec2 size,
	SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass
) -> std::expected<void, fcg::RenderError> {
	fcg::RenderState state(device, size);
	return lines.draw(nodes, pairs, state, commands, pass);
}
/// [offscreen]
