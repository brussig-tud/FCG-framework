
#ifndef __FCG_EXTRAS_FILE_FILTERS_H__
#define __FCG_EXTRAS_FILE_FILTERS_H__


//////
//
// Includes
//

// C++ STL
#include <span>

// FCG Framework
#include <FCG/Extras/export.h>
#include <FCG/Extras/file_dialog.h>
#include <FCG/Image/image_loader.h>



//////
//
// Namespaces open
//

/// The framework top-level namespace.
namespace fcg {

/// The library namespace containing our assorted extra utilities.
namespace extra {



//////
//
// Functions
//

/// \brief Build both aggregate and individual filters from the supported formats in the \ref fcg::ImageLoader::global
/// registry.
[[nodiscard]] FCG_EXTRAS_EXPORT auto imageFileFilters (std::span<const ImageFileFormat> formats)
    -> std::vector<FileDialogFilter>;



//////
//
// Namespaces close
//

// namespace extra
}

// namespace fcg
}


#endif // ifndef __FCG_EXTRAS_FILE_FILTERS_H__
