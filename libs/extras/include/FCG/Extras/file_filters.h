
#ifndef __FCG_EXTRAS_FILE_FILTERS_H__
#define __FCG_EXTRAS_FILE_FILTERS_H__


//////
//
// Includes
//

// C++ STL
#include <span>
#include <vector>

// FCG Framework
#include <FCG/Extras/export.h>
#include <FCG/Extras/file_dialog.h>
#include <FCG/Image/image_loader.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// The library's namespace for assorted extra utilities.
namespace extra {

/// \addtogroup fcg_file_dialogs
/// @{



//////
//
// Functions
//

/// \brief
/// 	Builds file-dialog filters for the given image file formats.
///
/// To offer every image format the image library can load, pass the formats of the global image loader:
/// <tt>imageFileFilters(ImageLoader::global().fileFormats())</tt>. The result can be assigned to
/// <tt>\ref FileDialogOptions::filters</tt>.
///
/// The returned filters are, in this order:
/// - "All supported images", containing every distinct extension of all \c formats except <tt>"*"</tt>, in order of
///   first appearance. This filter is omitted if no extensions remain after excluding wildcards.
/// - One filter per format, labelled with the format's name, in the order of \c formats. Duplicate extensions within a
///   format are removed. A format is skipped if it has no extensions or if an earlier format has the same set of
///   extensions. A format containing only \c "*" provides a named all-files filter.
/// - "All files", matching any file, if \c includeAllFiles is <tt>true</tt>.
///
/// Setting \c includeAllFiles to \c false preserves any all-files filters supplied in \c formats. Empty input with
/// \c includeAllFiles set to \c false produces an empty list; a dialog given an empty filter list leaves file types
/// unrestricted.
///
/// \param formats
/// 	The image file formats to build filters for. After the deduplication and omissions described above, the
/// 	generated filters must satisfy the rules of <tt>\ref FileDialogFilter</tt>; a dialog launched with invalid
/// 	generated filters reports <tt>\ref FileDialogErrorCode::InvalidArgument</tt>.
/// \param includeAllFiles
/// 	Whether to append the "All files" filter. Defaults to <tt>true</tt>, including when \c formats already supplies
/// 	an all-files filter.
///
/// \return
/// 	The list of filters described above.
[[nodiscard]] FCG_EXTRAS_EXPORT auto imageFileFilters (
	std::span<const ImageFileFormat> formats, bool includeAllFiles=true
) -> std::vector<FileDialogFilter>;



//////
//
// Namespaces close
//

/// @}

// namespace extra
}

// namespace fcg
}


#endif // ifndef __FCG_EXTRAS_FILE_FILTERS_H__
