
#ifndef __FCG_EXTRAS_FILE_DIALOG_H__
#define __FCG_EXTRAS_FILE_DIALOG_H__


//////
//
// Module documentation
//

/// \defgroup fcg_file_dialogs File and folder dialogs
/// \ingroup fcg_modules
/// \brief Native open/save file and open-folder dialogs with future or callback completion.
///
/// <tt>\ref fcg::extra::showOpenFileDialog "showOpenFileDialog"</tt>,
/// <tt>\ref fcg::extra::showSaveFileDialog "showSaveFileDialog"</tt>, and
/// <tt>\ref fcg::extra::showOpenFolderDialog "showOpenFolderDialog"</tt> show the platform's native dialogs. Each
/// comes in three variants: an <em>async</em> one that returns a <tt>std::future</tt>, a <em>callback</em> one that
/// invokes a function when the user finishes, and a <em>blocking</em> one that returns the result directly. All accept
/// owned <tt>\ref fcg::extra::FileDialogOptions "FileDialogOptions"</tt>: an optional parent window, a title, default
/// locations, filters, and selection flags.
///
/// \section fcg_file_dialogs_usage Getting started
/// Show a dialog from an applet and pick up the result in a later frame:
/// \snippet file_dialog_examples.cpp future
///
/// \section fcg_file_dialogs_workflows Common tasks
/// \subsection fcg_file_dialogs_workflows_variants Choosing a completion variant
/// Use the async variant when the dialog should stay open while the application keeps running: keep the future in
/// your applet and poll it from <tt>\ref fcg::Applet::update "update"</tt> while SDL events are processed. Request
/// continuous redraw so the loop stays active while waiting, and balance the request when the result arrives. Use the
/// callback variant to react without storing a future:
/// \snippet file_dialog_examples.cpp callback
/// Callback delivery follows <tt>\ref fcg::extra::FileDialogCallbackThread "FileDialogCallbackThread"</tt>, with
/// <tt>\ref fcg::extra::FileDialogCallbackThread::MainThread "MainThread"</tt> as the default. Callbacks may run before
/// the launching call returns and must not throw. Use the blocking variant only when nothing else needs to happen
/// meanwhile: it pumps SDL events until the dialog closes, so the application does not render or update during that
/// time.
///
/// \subsection fcg_file_dialogs_workflows_filters Filtering by file type
/// Filters pair a label with a list of extensions without leading dots, e.g. <tt>{"Images", {"png", "jpg"}}</tt>.
/// Use <tt>\ref fcg::extra::imageFileFilters "imageFileFilters"</tt> to build a filter list from the formats an
/// <tt>\ref fcg::ImageLoader "ImageLoader"</tt> supports. The helper excludes <tt>"*"</tt> from its combined "All
/// supported images" filter, preserves named wildcard filters, and appends "All files" by default. Pass \c false as its
/// second argument to omit the appended filter. Link \verbatim FCG-Framework::Image \endverbatim alongside
/// \verbatim FCG-Framework::Extras \endverbatim when using image-loader filters:
/// \snippet file_dialog_examples.cpp image_filters
/// <tt>"*"</tt> is accepted only as the sole extension within a filter; separate all-files filters are allowed. An
/// empty filter list leaves file types unrestricted. Save dialogs ignore multiple selection; folder dialogs ignore
/// filters. Platforms may ignore individual settings.
///
/// \section fcg_file_dialogs_lifetime Ownership and lifetime
/// Launch dialogs from the main thread. Keep SDL and the parent window alive until the dialog completes; dialogs
/// cannot be cancelled through this wrapper, and destroying a future neither blocks nor cancels. The selected paths
/// use <tt>std::filesystem::path</tt>, and results do not borrow SDL strings.
///
/// \section fcg_file_dialogs_errors Handling errors
/// The result is a <tt>std::expected</tt>-style <tt>\ref fcg::extra::FileDialogResult "FileDialogResult"</tt>: empty
/// selected paths mean cancellation, and failures carry a
/// <tt>\ref fcg::extra::FileDialogError "FileDialogError"</tt>
/// with a category (invalid arguments, invalid launch state such as missing SDL event processing, or SDL failure) and
/// message. Empty callbacks are rejected with \c std::invalid_argument at launch;
/// exceptions inside a callback terminate the application. Nothing logs.
///
/// \see \ref fcg_applets, \ref fcg_events



//////
//
// Includes
//

// C++ STL
#include <expected>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <vector>
#include <functional>

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

/// The library top-level namespace.
namespace fcg {

/// The library's namespace for assorted extra utilities.
namespace extra {

/// \addtogroup fcg_file_dialogs
/// @{



//////
//
// Structs and enums
//

/// A named file-type filter offered by open-file and save-file dialogs.
struct FileDialogFilter
{
	/// Human-readable UTF-8 label shown in the dialog. Must be nonempty and must not contain NUL characters.
	std::string name;

	/// \brief
	/// 	File extensions matched by this filter, without leading dots (e.g. \c "png"), or a single \c "*" to match
	/// 	all files.
	///
	/// The list must be nonempty. Extensions may contain ASCII letters, digits, hyphens, underscores and interior
	/// dots. \c "*" is only accepted as the sole entry of a filter. Separate filters may each contain <tt>"*"</tt>.
	std::vector<std::string> extensions;
};

/// \brief
/// 	Settings for launching a native file or folder dialog.
///
/// Invalid settings are reported as <tt>\ref FileDialogErrorCode::InvalidArgument</tt> through the dialog result.
/// Platforms may ignore individual settings.
struct FileDialogOptions
{
	/// \brief
	/// 	Optional window the dialog is attached to, or \c nullptr for none.
	///
	/// The window is borrowed and must stay alive until the dialog has completed.
	Window *parent = nullptr;

	/// UTF-8 dialog title. Must not contain NUL characters. If empty, the platform default title is used.
	std::string title;

	/// Whether the user may select more than one file or folder. Ignored by save-file dialogs.
	bool allowMultiple = false;

	/// File or directory the dialog initially shows. If empty, the platform default location is used.
	std::filesystem::path defaultLocation;

	/// \brief
	/// 	File-type filters offered to the user, in the given order. Ignored by folder dialogs.
	///
	/// If empty, the dialog does not restrict file types.
	std::vector<FileDialogFilter> filters;
};

/// The outcome of a dialog that completed without error.
struct FileDialogSelection
{
	/// The selected files or folders. An empty list means the user cancelled the dialog.
	std::vector<std::filesystem::path> paths;

	/// \brief
	/// 	Zero-based index into <tt>\ref FileDialogOptions::filters</tt> of the filter the user had selected.
	///
	/// Empty if the platform does not report the selected filter, or reports an index outside the submitted list.
	std::optional<std::size_t> selectedFilter;
};

/// Categories of errors reported by file dialogs.
enum class FileDialogErrorCode
{
	/// The <tt>\ref FileDialogOptions</tt> contained an invalid filter, title or location.
	InvalidArgument,

	/// The dialog was launched from a thread other than the main thread, or before the SDL event subsystem was
	/// initialized.
	InvalidState,

	/// SDL failed to prepare, show or complete the native dialog.
	SDLFailure
};

/// \brief
/// 	Error returned by file-dialog operations: a category plus a human-readable message (including SDL's message
/// 	where relevant).
struct FileDialogError
{
	/// The error category.
	FileDialogErrorCode code;

	/// Human-readable description of the error.
	std::string message;
};

/// \brief
/// 	Result of a file dialog: the user's selection (an empty selection means cancellation), or an error.
using FileDialogResult = std::expected<FileDialogSelection, FileDialogError>;

/// Selects the thread on which a dialog completion callback is invoked.
enum class FileDialogCallbackThread
{
	/// \brief
	/// 	Invoke the callback on the main thread.
	///
	/// If the dialog completes on the main thread, the callback runs immediately. Otherwise, it runs the next time
	/// the application processes SDL events on the main thread. If dispatch fails, the callback instead runs on the
	/// completing thread with \c FileDialogErrorCode::SDLFailure. If main-thread delivery is unavailable without SDL
	/// events, it runs on the completing thread with the existing error, or \c FileDialogErrorCode::InvalidState if
	/// there was no error. Error handling in the callback must therefore be safe on a background thread.
	MainThread,

	/// Invoke the callback directly on the thread on which SDL completes the dialog. This can be the main thread or a
	/// background thread.
	SDLThread
};



//////
//
// Functions
//

/// \brief
/// 	Shows a native open-file dialog and returns a future for its result.
///
/// Call from the main thread after SDL's event subsystem has been initialized. The function returns immediately.
/// Check the future regularly (e.g. once per frame with <tt>wait_for(0s)</tt>) while the application keeps processing
/// SDL events; on some platforms the dialog only completes while SDL events are being processed. Do not block the main
/// thread on <tt>get()</tt> or <tt>wait()</tt>; use <tt>\ref showOpenFileDialog</tt> for a blocking dialog instead.
/// The future may already be ready when the function returns, e.g. if the launch failed.
///
/// A running native dialog cannot be cancelled. Destroying the future neither blocks nor closes the dialog.
///
/// \pre
/// 	SDL and <tt>\ref FileDialogOptions::parent</tt> (if set) stay alive until the dialog has completed.
///
/// \param options
/// 	The dialog settings.
///
/// \return
/// 	A future that becomes ready when the dialog completes. Its value is the selection, an empty selection if the
/// 	user cancelled, or an error: <tt>\ref FileDialogErrorCode::InvalidState</tt> if called from a thread other than
/// 	the main thread or without the SDL event subsystem, <tt>\ref FileDialogErrorCode::InvalidArgument</tt> for
/// 	invalid \c options, or <tt>\ref FileDialogErrorCode::SDLFailure</tt> if SDL failed.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showOpenFileDialogAsync (FileDialogOptions options={})
	-> std::future<FileDialogResult>;

/// \brief
/// 	Shows a native open-file dialog and invokes a callback with its result.
///
/// Call from the main thread after SDL's event subsystem has been initialized. The callback is invoked exactly once,
/// according to \c thread, including its error-delivery fallback. It may be invoked before this function returns,
/// e.g. if the launch failed.
/// With <tt>\ref FileDialogCallbackThread::MainThread</tt>, the application must keep processing SDL events to receive
/// the callback.
///
/// A running native dialog cannot be cancelled.
///
/// \pre
/// 	SDL and <tt>\ref FileDialogOptions::parent</tt> (if set) stay alive until the callback has been invoked.
///
/// \note
/// 	The callback must not throw. An exception leaving the callback terminates the program.
///
/// \param options
/// 	The dialog settings.
/// \param callback
/// 	Function receiving the result, which is a selection, an empty selection if the user cancelled, or an error
/// 	with the same meaning as described for <tt>\ref showOpenFileDialogAsync</tt>. Must not be empty. The callback,
/// 	including its captures, is kept alive until it has been invoked. The callable and its captures must be
/// 	copyable to be stored in <tt>std::function</tt>.
/// \param thread
/// 	The delivery policy for \c callback, including errors detected at launch; see \c FileDialogCallbackThread.
///
/// \throws std::invalid_argument
/// 	If \c callback is empty. No dialog is shown in this case.
FCG_EXTRAS_EXPORT void showOpenFileDialogCallback (
	FileDialogOptions options, std::function<void(FileDialogResult)> callback,
	FileDialogCallbackThread thread=FileDialogCallbackThread::MainThread
);

/// \brief
/// 	Shows a native open-file dialog and waits until the user has closed it.
///
/// Call from the main thread after SDL's event subsystem has been initialized. While waiting, the function pumps SDL
/// events so that the dialog can complete. The events stay queued for the application, which does not render or
/// update until the function returns.
///
/// \param options
/// 	The dialog settings.
///
/// \return
/// 	The selection, an empty selection if the user cancelled, or an error with the same meaning as described for
/// 	<tt>\ref showOpenFileDialogAsync</tt>.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showOpenFileDialog (FileDialogOptions options={}) -> FileDialogResult;

/// \brief
/// 	Shows a native save-file dialog and returns a future for its result.
///
/// Usage, requirements and errors are the same as for <tt>\ref showOpenFileDialogAsync</tt>. The user selects at
/// most one path.
///
/// \pre
/// 	SDL and <tt>\ref FileDialogOptions::parent</tt> (if set) stay alive until the dialog has completed.
///
/// \param options
/// 	The dialog settings. <tt>\ref FileDialogOptions::allowMultiple</tt> is ignored.
///
/// \return
/// 	A future that becomes ready when the dialog completes, with a result as described for
/// 	<tt>\ref showOpenFileDialogAsync</tt>.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showSaveFileDialogAsync (FileDialogOptions options={})
	-> std::future<FileDialogResult>;

/// \brief
/// 	Shows a native save-file dialog and invokes a callback with its result.
///
/// Usage, requirements and callback rules are the same as for <tt>\ref showOpenFileDialogCallback</tt>.
///
/// \pre
/// 	SDL and <tt>\ref FileDialogOptions::parent</tt> (if set) stay alive until the callback has been invoked.
///
/// \note
/// 	The callback must not throw. An exception leaving the callback terminates the program.
///
/// \param options
/// 	The dialog settings. <tt>\ref FileDialogOptions::allowMultiple</tt> is ignored.
/// \param callback
/// 	Function receiving the result as described for <tt>\ref showOpenFileDialogCallback</tt>. Must not be empty.
/// \param thread
/// 	The delivery policy for \c callback, including errors detected at launch; see \c FileDialogCallbackThread.
///
/// \throws std::invalid_argument
/// 	If \c callback is empty. No dialog is shown in this case.
FCG_EXTRAS_EXPORT void showSaveFileDialogCallback (
	FileDialogOptions options, std::function<void(FileDialogResult)> callback,
	FileDialogCallbackThread thread=FileDialogCallbackThread::MainThread
);

/// \brief
/// 	Shows a native save-file dialog and waits until the user has closed it.
///
/// Usage and waiting behavior are the same as for <tt>\ref showOpenFileDialog</tt>.
///
/// \param options
/// 	The dialog settings. <tt>\ref FileDialogOptions::allowMultiple</tt> is ignored.
///
/// \return
/// 	The selection, an empty selection if the user cancelled, or an error with the same meaning as described for
/// 	<tt>\ref showOpenFileDialogAsync</tt>.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showSaveFileDialog (FileDialogOptions options={}) -> FileDialogResult;

/// \brief
/// 	Shows a native folder dialog and returns a future for its result.
///
/// Usage, requirements and errors are the same as for <tt>\ref showOpenFileDialogAsync</tt>.
///
/// \pre
/// 	SDL and <tt>\ref FileDialogOptions::parent</tt> (if set) stay alive until the dialog has completed.
///
/// \param options
/// 	The dialog settings. <tt>\ref FileDialogOptions::filters</tt> is ignored.
///
/// \return
/// 	A future that becomes ready when the dialog completes, with a result as described for
/// 	<tt>\ref showOpenFileDialogAsync</tt>.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showOpenFolderDialogAsync (FileDialogOptions options={})
	-> std::future<FileDialogResult>;

/// \brief
/// 	Shows a native folder dialog and invokes a callback with its result.
///
/// Usage, requirements and callback rules are the same as for <tt>\ref showOpenFileDialogCallback</tt>.
///
/// \pre
/// 	SDL and <tt>\ref FileDialogOptions::parent</tt> (if set) stay alive until the callback has been invoked.
///
/// \note
/// 	The callback must not throw. An exception leaving the callback terminates the program.
///
/// \param options
/// 	The dialog settings. <tt>\ref FileDialogOptions::filters</tt> is ignored.
/// \param callback
/// 	Function receiving the result as described for <tt>\ref showOpenFileDialogCallback</tt>. Must not be empty.
/// \param thread
/// 	The delivery policy for \c callback, including errors detected at launch; see \c FileDialogCallbackThread.
///
/// \throws std::invalid_argument
/// 	If \c callback is empty. No dialog is shown in this case.
FCG_EXTRAS_EXPORT void showOpenFolderDialogCallback (
	FileDialogOptions options, std::function<void(FileDialogResult)> callback,
	FileDialogCallbackThread thread=FileDialogCallbackThread::MainThread
);

/// \brief
/// 	Shows a native folder dialog and waits until the user has closed it.
///
/// Usage and waiting behavior are the same as for <tt>\ref showOpenFileDialog</tt>.
///
/// \param options
/// 	The dialog settings. <tt>\ref FileDialogOptions::filters</tt> is ignored.
///
/// \return
/// 	The selection, an empty selection if the user cancelled, or an error with the same meaning as described for
/// 	<tt>\ref showOpenFileDialogAsync</tt>.
[[nodiscard]] FCG_EXTRAS_EXPORT auto showOpenFolderDialog (FileDialogOptions options={}) -> FileDialogResult;



//////
//
// Namespaces close
//

/// @}

// namespace extra
}

// namespace fcg
}


#endif // ifndef __FCG_EXTRAS_FILE_DIALOG_H__
