
//////
//
// Includes
//

// C++ STL
#include <span>

// FCG Framework
/// [includes]
#include <FCG/Render/wire_box_renderer.h>
/// [includes]
#include <FCG/player.h>



//////
//
// Functions
//

/// \brief Upload bounding-box centers and a shared half-size and color.
///
/// \param attributes The collection to update.
/// \param centers Homogeneous box centers borrowed until this call returns.
///
/// \return Success, or an upload error.
/// [attributes]
[[nodiscard]] auto uploadWireBoxes (fcg::PrimitiveAttributes &attributes, std::span<const glm::vec4> centers)
	-> std::expected<void, fcg::RenderError>
{
	return attributes.setAttributes([&] (auto &update) {
		update.template set<fcg::Attribute::Position>(centers);
		update.template set<fcg::Attribute::Extent>(glm::vec3(.5f, .25f, .4f));
		update.template set<fcg::Attribute::Color>(glm::vec4(1.f, .8f, .1f, 1.f));
	});
}
/// [attributes]

/// \brief Create filtered outlines for the player's effective scene target.
///
/// \param player Supplies the device and usable target.
///
/// \return The renderer, or a creation error.
/// [factory]
[[nodiscard]] auto createWireBoxes (fcg::Player &player) -> std::expected<fcg::WireBoxRenderer, fcg::RenderError> {
	return fcg::WireBoxRenderer::create(player, {.alphaBlending=true});
}
/// [factory]

/// \brief Draw uploaded boxes into a compatible active pass.
///
/// \param boxes The renderer to use.
/// \param attributes The uploaded box attributes.
/// \param state Viewing matrices and physical viewport dimensions.
/// \param commands The command buffer containing the active pass.
/// \param pass The active render pass.
///
/// \return Success, or an unsupported-feature error.
/// [draw]
[[nodiscard]] auto drawWireBoxes (
	const fcg::WireBoxRenderer &boxes, const fcg::PrimitiveAttributes &attributes, fcg::RenderState &state,
	SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass
) -> std::expected<void, fcg::RenderError> {
	return boxes.draw(attributes, state, commands, pass);
}
/// [draw]

/// \brief Set the fallback width and cap shape for subsequent draws.
///
/// \param boxes The renderer whose appearance is changed.
/// [style]
void styleWireBoxes (fcg::WireBoxRenderer &boxes) {
	boxes.setStyle({.defaultWidth=3.f, .cap=fcg::LineCap::Round, .filter=fcg::LineFilter::Smoothstep});
}
/// [style]

/// \brief Synchronize viewing state after configuring a custom GPU viewport.
///
/// \param state The state used by the draw.
/// \param physicalSize The positive physical pixel dimensions of that viewport.
/// [viewport]
void wireBoxViewport (fcg::RenderState &state, glm::uvec2 physicalSize) {
	state.setViewportSize(physicalSize);
}
/// [viewport]
