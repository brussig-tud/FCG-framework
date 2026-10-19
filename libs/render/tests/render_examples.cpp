
//////
//
// Includes
//

// C++ STL
#include <span>

// FCG Framework
/// [includes]
#include <FCG/Render/quad_renderer.h>
#include <FCG/Render/box_renderer.h>
#include <FCG/player.h>
/// [includes]



//////
//
// Functions
//

/// Upload positions and a shared color before drawing boxes into the caller's active pass.
///
/// \param device The device shared by the renderer and command buffer.
/// \param boxes The renderer used for the draw.
/// \param state The current viewing matrices.
/// \param commandBuffer The command buffer owning the active pass.
/// \param renderPass The active pass compatible with the renderer.
/// \param positions The instance centers, borrowed for this call.
///
/// \return Success, or the upload or draw error.
// [update_and_draw]
[[nodiscard]] auto updateAndDraw (
	fcg::Device &device, const fcg::BoxRenderer &boxes, fcg::RenderState &state,
	SDL_GPUCommandBuffer *commandBuffer, SDL_GPURenderPass *renderPass, std::span<const glm::vec4> positions
) -> std::expected<void, fcg::RenderError>
{
	fcg::PrimitiveAttributes attributes(device);
	auto result = attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.set<fcg::Attribute::Position>(positions);
		update.set<fcg::Attribute::Color>(glm::vec4(1.f, 0.f, 0.f, 1.f));
	});
	if (result)
		result = boxes.draw(attributes, state, commandBuffer, renderPass);
	return result;
}
// [update_and_draw]

/// Update shared GPU attributes from caller-owned arrays.
/// [attributes]
[[nodiscard]] auto replaceInstances (
	fcg::PrimitiveAttributes &attributes, std::span<const glm::vec4> positions,
	std::span<const glm::quat> orientations, std::span<const glm::vec4> colors
) -> std::expected<void, fcg::RenderError>
{
	return attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.set<fcg::Attribute::Position>(positions);
		update.set<fcg::Attribute::Extent>(glm::vec3(1.f));
		update.set<fcg::Attribute::Orientation>(orientations);
		update.set<fcg::Attribute::Color>(colors);
	});
}
/// [attributes]

/// Create a renderer during applet initialization, before the first frame.
/// [player]
[[nodiscard]] auto createQuads (fcg::Player &player) -> std::expected<fcg::QuadRenderer, fcg::RenderError> {
	return fcg::QuadRenderer::create(player, {.alphaBlending = true});
}
/// [player]

/// Draw a shared collection using an already initialized renderer and active main pass.
/// [draw]
[[nodiscard]] auto drawBoxes (
	const fcg::BoxRenderer &boxes, const fcg::PrimitiveAttributes &attributes,
	fcg::RenderState &state, SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass
) -> std::expected<void, fcg::RenderError>
{
	fcg::DrawOptions options;
	options.lighting.enabled = true;
	return boxes.draw(attributes, state, commands, pass, options);
}
/// [draw]
