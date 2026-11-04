
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
	/// Examples are invalid attribute-update values or layouts, unsupported renderer features and missing connectivity.
	InvalidArgument,

	/// \brief
	/// 	An object is not in a state that permits the operation.
	///
	/// Examples are recursive attribute updates, unavailable renderer shader resources and unusable upload devices.
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
