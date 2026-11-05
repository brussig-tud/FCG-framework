
#ifndef __FCG_MESH_ERROR_H__
#define __FCG_MESH_ERROR_H__


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

/// \addtogroup fcg_meshes
/// @{



//////
//
// Structs and enums
//

/// Categories of loading, processing and registry failures reported in <tt>MeshError::code</tt>.
enum class MeshErrorCode
{
	/// An input, path, hint, option or registration argument is invalid.
	InvalidArgument,

	/// A file cannot be opened, sized or read.
	IOFailure,

	/// No handler accepts the format, or a requested encoding is unsupported.
	UnsupportedFormat,

	/// Decoding failed; a registry error includes the failed handlers' IDs and messages.
	DecodeFailure,

	/// OpenMesh cannot construct the requested connectivity under the selected policy.
	InvalidTopology,

	/// The registry already contains the requested handler ID.
	DuplicateHandler,

	/// A polygon cannot be triangulated or a processing operation encounters a non-triangle.
	ProcessingFailure
};

/// A mesh operation's failure category and human-readable diagnostic.
struct MeshError {
	/// Failure category.
	MeshErrorCode code;

	/// Human-readable description, including the source face where applicable.
	std::string message;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // __FCG_MESH_ERROR_H__
