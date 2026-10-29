
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

/// [includes]
// FCG Framework
#include <FCG/Render/line_set_builder.h>
#include <FCG/Render/line_renderer.h>
/// [includes]



//////
//
// Functions
//

/// \brief Build two colored polylines with a shared pixel width.
///
/// \param device The device that will own the uploaded set.
///
/// \return The uploaded set or an upload error.
/// [build]
[[nodiscard]] auto buildLineSet (fcg::Device &device) -> std::expected<fcg::LineSet, fcg::RenderError>
{
	auto lines = fcg::LineSetBuilder::create();
	lines.setConstantWidth(2.f);
	lines.recordPolyline([] (auto &line) {
		line.addNode({0, 0, 0}, {1, 1, 0, 1});
		line.addNode({0, 0, 1}, {1, 0, 1, 1});
		line.addNode({0, 1, 0}, {0, 1, 1, 1});
	});
	lines.addPolyline({
		{{1, 0, 0}, {0, 1, 0, 1}},
		{{1, 0, 1}, {1, 0, 0, 1}},
		{{1, 1, 0}, {0, 0, 1, 1}},
	});
	return lines.upload(device);
}
/// [build]

/// \brief Upload and draw a line set in an active pass.
///
/// \param device The device shared by renderer and pass.
/// \param renderer The compatible line renderer.
/// \param state Viewing matrices and viewport dimensions.
/// \param commands The command buffer containing the active pass.
/// \param pass The active render pass.
///
/// \return Success or the upload or drawing error.
/// [draw]
[[nodiscard]] auto drawLineSet (
	fcg::Device &device, const fcg::LineRenderer &renderer, fcg::RenderState &state,
	SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass
) -> std::expected<void, fcg::RenderError>
{
	auto gpuLines = buildLineSet(device);
	if (!gpuLines)
		return std::unexpected(gpuLines.error());
	return renderer.draw(*gpuLines, state, commands, pass);
}
/// [draw]

/// \brief Append a closed triangle using renderer-default width and color.
///
/// \param lines A builder with no constants or explicit node attributes.
/// [closed]
void appendTriangle (fcg::LineSetBuilder &lines) {
	lines.addPolyline(
	{ {{0, 0, 0}}, {{1, 0, 0}}, {{0, 1, 0}} }, /*closed:*/true
	);
}
/// [closed]

/// \brief Append a segment with a varying width and a set-wide color.
///
/// \param lines An empty builder.
/// [widths]
void appendTaperedSegment (fcg::LineSetBuilder &lines) {
	lines.setConstantColor({1, .5f, 0, 1});
	lines.recordPolyline([] (auto &line) {
		line.addNode({0, 0, 0}, 2.f);
		line.addNode({1, 0, 0}, 6.f);
	});
}
/// [widths]

/// \brief Rebuild a GPU set while retaining its constant width and color settings.
///
/// \param lines The builder with no explicit node attributes.
/// \param gpuLines The live destination set.
///
/// \return Success or the upload error.
/// [rebuild]
[[nodiscard]] auto rebuildLineSet (fcg::LineSetBuilder &lines, fcg::LineSet &gpuLines)
	-> std::expected<void, fcg::RenderError> {
	lines.clear();
	lines.addPolyline({{{0, 0, 0}}, {{0, 1, 0}}});
	return lines.upload(gpuLines);
}
/// [rebuild]
