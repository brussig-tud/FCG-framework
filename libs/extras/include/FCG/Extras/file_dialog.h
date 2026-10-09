
#ifndef __FCG_EXTRAS_FILE_DIALOG_H__
#define __FCG_EXTRAS_FILE_DIALOG_H__


//////
//
// Includes
//

// C++ STL
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

// Local includes
#include "FCG/Extras/export.h"



//////
//
// Forward declarations
//

// Framework types.
namespace fcg {
	class Window;
}



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
// Structs and enums
//

/// \brief Owned native file-dialog filter. \ingroup fcg_file_dialogs
struct FileDialogFilter
{
	/// Human-readable UTF-8 label; must be nonempty and contain no NULs.
	std::string name;

	/// Extensions without leading dots, or a single \c "*" for all files.
	/// Letters, digits, hyphens, underscores, and interior dots are allowed.
	std::vector<std::string> extensions;
};

/// \brief Native file-dialog launch options. \ingroup fcg_file_dialogs
struct FileDialogOptions
{
	/// Borrowed parent, possibly \c nullptr; must outlive completion.
	Window *parent = nullptr;

	/// UTF-8 title; empty uses the platform default.
	std::string title;

	/// Allow multiple files or folders; ignored for save dialogs.
	bool allowMultiple = false;

	/// Native filesystem file or directory to start at; empty uses the platform default.
	std::filesystem::path defaultLocation;

	/// Owned filters; ignored for folder dialogs.
	std::vector<FileDialogFilter> filters;
};

/// \brief Owned native file-dialog selection. \ingroup fcg_file_dialogs
struct FileDialogSelection
{
	/// Selected native filesystem paths; an empty list means cancellation.
	std::vector<std::filesystem::path> paths;

	/// Zero-based filter index when supplied by the platform and within the submitted filter list.
	std::optional<std::size_t> selectedFilter;
};

/// \brief Recoverable dialog error categories. \ingroup fcg_file_dialogs
enum class FileDialogErrorCode
{
	/// Invalid filters, strings, or filesystem encoding.
	InvalidArgument,
	/// Launch outside the main thread or without initialized SDL events.
	InvalidState,
	/// SDL failed to create properties, launch, or complete the native dialog.
	SDLFailure
};

/// \brief Owned file-dialog diagnostic. \ingroup fcg_file_dialogs
struct FileDialogError
{
	/// Error category.
	FileDialogErrorCode code;

	/// Error text captured before SDL's thread-local error can change.
	std::string message;
};

/// \brief Successful selection (including cancellation), or an owned error. \ingroup fcg_file_dialogs
using FileDialogResult = std::expected<FileDialogSelection, FileDialogError>;

/// \brief Callback dispatch policy. \ingroup fcg_file_dialogs
enum class FileDialogCallbackThread
{
	/// Dispatch during SDL main-thread event processing; inline when already on the main thread.
	MainThread,
	/// Invoke directly on SDL's completion thread, which may be the main thread or a worker.
	SDLThread
};



//////
//
// Functions
//

/// \brief Show a native open-file dialog. \ingroup fcg_file_dialogs
///
/// \pre Launch on the main thread with SDL events initialized; keep SDL and the parent alive until completion.
///
/// \note Native dialogs cannot be cancelled through this wrapper. Destroying the future does not block or cancel.
///
/// \param options Owned launch settings; platform support for individual settings varies.
///
/// \return A future fulfilled directly on SDL completion, including launch errors and cancellation.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showOpenFileDialogAsync (FileDialogOptions options={})
	-> std::future<FileDialogResult>;

/// \brief Show a native open-file dialog with callback completion. \ingroup fcg_file_dialogs
///
/// \pre Launch on the main thread with SDL events initialized; keep SDL and the parent alive until completion.
///
/// \note
/// 	Pump SDL events for main-thread callbacks. Completion may be inline. Callbacks must not throw;
/// 	callback exceptions, unrecoverable allocation failures in SDL callbacks, and dispatch failure terminate.
/// 	Native dialogs cannot be cancelled through this wrapper. An empty callback throws \c std::invalid_argument.
///
/// \param options Owned launch settings.
/// \param callback Move-only completion function, retained until invoked exactly once.
/// \param thread Callback dispatch policy, including for validation errors.
FCG_EXTRAS_EXPORT void showOpenFileDialogCallback (
	FileDialogOptions options, std::move_only_function<void(FileDialogResult)> callback,
	FileDialogCallbackThread thread=FileDialogCallbackThread::MainThread
);

/// \brief Blocking convenience wrapper for \ref showOpenFileDialogAsync. \ingroup fcg_file_dialogs
///
/// \param options Owned launch settings; platform support for individual settings varies.
///
/// \return The user selection (including cancellation), or a \ref FileDialogError if there was a problem.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showOpenFileDialog (FileDialogOptions options={}) -> FileDialogResult;

/// \brief Show a native save-file dialog; uses the same lifetime contract as \ref showOpenFileDialogAsync.
/// \ingroup fcg_file_dialogs
///
/// \param options Owned launch settings; multiple selection is ignored.
///
/// \return A nonblocking-destructor future fulfilled directly on SDL completion.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showSaveFileDialogAsync (FileDialogOptions options={})
	-> std::future<FileDialogResult>;

/// \brief Show a save-file dialog with the callback contract of \ref showOpenFileDialogCallback. \ingroup fcg_file_dialogs
///
/// \param options Owned launch settings; multiple selection is ignored.
/// \param callback Nonempty completion function; must not throw.
/// \param thread Callback dispatch policy.
FCG_EXTRAS_EXPORT void showSaveFileDialogCallback (
	FileDialogOptions options, std::move_only_function<void(FileDialogResult)> callback,
	FileDialogCallbackThread thread=FileDialogCallbackThread::MainThread
);

/// \brief Blocking convenience wrapper for \ref showSaveFileDialogAsync. \ingroup fcg_file_dialogs
///
/// \param options Owned launch settings; platform support for individual settings varies.
///
/// \return The user selection (including cancellation), or a \ref FileDialogError if there was a problem.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showSaveFileDialog (FileDialogOptions options={}) -> FileDialogResult;

/// \brief Show a native folder dialog; uses the lifetime contract of \ref showOpenFileDialogAsync.
/// \ingroup fcg_file_dialogs
///
/// \param options Owned launch settings; filters are ignored and multiple selection is supported.
///
/// \return A nonblocking-destructor future fulfilled directly on SDL completion.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showOpenFolderDialogAsync (FileDialogOptions options={})
	-> std::future<FileDialogResult>;

/// \brief Show a folder dialog with the callback contract of \ref showOpenFileDialogCallback. \ingroup fcg_file_dialogs
///
/// \param options Owned launch settings; filters are ignored.
/// \param callback Nonempty completion function; must not throw.
/// \param thread Callback dispatch policy.
FCG_EXTRAS_EXPORT void showOpenFolderDialogCallback (
	FileDialogOptions options, std::move_only_function<void(FileDialogResult)> callback,
	FileDialogCallbackThread thread=FileDialogCallbackThread::MainThread
);

/// \brief Blocking convenience wrapper for \ref showOpenFolderDialogAsync. \ingroup fcg_file_dialogs
///
/// \param options Owned launch settings; platform support for individual settings varies.
///
/// \return The user selection (including cancellation), or a \ref FileDialogError if there was a problem.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showOpenFolderDialog (FileDialogOptions options={}) -> FileDialogResult;



//////
//
// Namespaces close
//

// namespace extra
}

// namespace fcg
}


#endif // ifndef __FCG_EXTRAS_FILE_DIALOG_H__
