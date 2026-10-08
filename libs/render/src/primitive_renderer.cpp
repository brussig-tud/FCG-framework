
//////
//
// Includes
//

// C++ STL
#include <algorithm>

// FCG Framework
#include <FCG/Render/primitive_renderer.h>



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
// PrimitiveRenderer

PrimitiveRenderer::~PrimitiveRenderer() = default;

auto PrimitiveRenderer::supports (Attribute attribute) const -> bool {
	const auto supported = supportedAttributes();
	return std::find(supported.begin(), supported.end(), attribute) != supported.end();
}



//////
//
// Module namespace close
//

} // namespace fcg
