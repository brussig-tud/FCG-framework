
#ifndef __FCG_IMAGE_ERROR_H__
#define __FCG_IMAGE_ERROR_H__


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

/// \addtogroup fcg_images
/// @{



//////
//
// Structs and enums
//

/// Categories of image loading, pixel access, conversion, upload and registry failures, reported in \c ImageError::code.
enum class ImageErrorCode
{
	/// An argument is invalid, including coordinates, loss policies, input paths, encoded data or adopted storage.
	InvalidArgument,

	/// An image file could not be opened, sized or read.
	IOFailure,

	/// The operation requires a supported pixel representation or an accepting image decoder.
	UnsupportedFormat,

	/// \brief
	/// 	Decoding failed. When reported by \c ImageLoader::load, the message lists the ID and error message of every
	/// 	handler that failed, one per line, in the order the handlers were tried.
	DecodeFailure,

	/// An SDL operation, such as creating a memory stream or converting a surface, failed.
	SDLFailure,

	/// \c ImageLoader::registerHandler was given an ID that is already registered.
	DuplicateHandler,

	/// The image is empty or has a conflicting active pixel access session.
	InvalidState,

	/// The device cannot sample a suitable texture format, or creating or uploading the texture failed.
	GPUFailure,

	/// A conversion or color write requires explicit permission for potential loss.
	LossyConversion
};

/// \brief
/// 	Error returned by image loading, access, conversion, upload and registry operations: a category plus a human-readable message
/// 	(including SDL's message where relevant).
struct ImageError
{
	/// Failure category.
	ImageErrorCode code;

	/// Human-readable description of the failure.
	std::string message;
};

/// @}



//////
//
// Namespaces close
//

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_ERROR_H__
