
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

/// Categories of errors reported by the Render library.
enum class RenderErrorCode
{
	/// \brief
	/// 	An argument is invalid.
	///
	/// Examples are invalid attribute values or buffer view layouts, a draw range beyond the instance count, invalid
	/// draw or renderer options, a target color or depth format or sample count the device does not support, and
	/// objects that belong to different devices.
	InvalidArgument,

	/// \brief
	/// 	An object is not in a state that permits the operation.
	///
	/// Examples are calling \c PrimitiveAttributes::setAttributes from within its own callback, drawing an attribute
	/// that is not usable (see <tt>PrimitiveAttributes::usable</tt>), a bound attribute buffer that was moved from or
	/// no longer covers the drawn range, a \c Player without a usable main-window render target, a moved-from device,
	/// and a moved-from renderer.
	InvalidState,

	/// An SDL call failed, for example when creating, uploading or submitting GPU resources.
	SDLFailure
};

/// Error returned by Render library operations: a category plus a human-readable message (including SDL's message
/// where relevant).
struct RenderError
{
	/// The error category.
	RenderErrorCode code;

	/// Human-readable description of the failure, including SDL's message where relevant.
	std::string message;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_ERROR_H__
