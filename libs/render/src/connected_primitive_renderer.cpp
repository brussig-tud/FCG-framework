
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Implemented header
#include "FCG/Render/connected_primitive_renderer.h"



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
// ConnectedPrimitiveRenderer

ConnectedPrimitiveRenderer::~ConnectedPrimitiveRenderer() = default;

auto ConnectedPrimitiveRenderer::draw (
	const PrimitiveAttributes&, RenderState&, SDL_GPUCommandBuffer*, SDL_GPURenderPass*,
	const DrawOptions&, InstanceRange
) const -> std::expected<void, RenderError> {
	return std::unexpected(RenderError{
		RenderErrorCode::InvalidArgument, "Connected primitive drawing requires explicit connectivity"
	});
}



//////
//
// Module namespace close
//

} // namespace fcg
