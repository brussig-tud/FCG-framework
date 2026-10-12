
#ifndef __FCG_EXTRAS_ALL_H__
#define __FCG_EXTRAS_ALL_H__


//////
//
// Library documentation
//

/// \page fcg_extras_overview Extras library overview
///
/// Extras collects optional desktop conveniences on top of Core: functionality an application only needs on some
/// platforms or in some applications, kept out of the Core library. Currently, it provides native file and folder
/// dialogs.
///
/// Link your application against \verbatim FCG-Framework::Extras \endverbatim and include the header of the module
/// you use (or <tt>\#include &lt;FCG/Extras/all.h&gt;</tt> for everything at once).
///
/// \section fcg_extras_start Where to start
/// - Show file and folder dialogs, with or without blocking: \ref fcg_file_dialogs.



//////
//
// Includes
//

// Local includes
#include <FCG/Extras/file_dialog.h>
#include <FCG/Extras/file_filters.h>


#endif // ifndef __FCG_EXTRAS_ALL_H__
