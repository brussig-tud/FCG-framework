
#ifndef __FCG_RENDER_ERROR_H__
#define __FCG_RENDER_ERROR_H__


//////
//
// Includes
//

// C++ STL
#include <string>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_primitives
/// @{



//////
//
// Structs and enums
//

/// Recoverable Render failure categories.
enum class RenderErrorCode
{
	/// Invalid attribute layout, range, or option.
	InvalidArgument,

	/// Recursive update, unusable array, missing target, or moved-from renderer.
	InvalidState,

	/// SDL allocation, mapping, or submission failure.
	SDLFailure
};

/// Failure with diagnostic text owned independently of SDL's error state.
struct RenderError {
	/// Machine-readable category.
	RenderErrorCode code;

	/// Context and captured diagnostic.
	std::string message;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_ERROR_H__
