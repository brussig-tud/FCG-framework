
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

// SDL3 library
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>

// Local includes
#include "FCG/window.h"

// Implemented header
#include "FCG/Extras/file_dialog.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

// Convenience for our own namespace
using namespace fcg::extra;

/// Private per-launch SDL glue
struct FileDialogBackend
{
	/// Native launch function.
	decltype(&SDL_ShowFileDialogWithProperties) show = SDL_ShowFileDialogWithProperties;

	/// Main-thread dispatch function.
	decltype(&SDL_RunOnMainThread) dispatch = SDL_RunOnMainThread;

	/// Property creation function.
	decltype(&SDL_CreateProperties) createProperties = SDL_CreateProperties;
};

/// Scoped SDL properties, needed only until the native launch function returns.
class DialogProperties
{
public:

	////
	// Object construction/destruction

	/// Take ownership of a possibly empty properties handle.
	explicit DialogProperties(SDL_PropertiesID handle) : m_handle(handle) {}

	/// Release the launch properties on the launching thread.
	~DialogProperties() {
		if (m_handle)
			SDL_DestroyProperties(m_handle);
	}

	/// Properties have a single owner and cannot be copied.
	DialogProperties(const DialogProperties&) = delete;

	/// Properties ownership cannot be duplicated through assignment.
	auto operator= (const DialogProperties&) -> DialogProperties& = delete;


	////
	// Accessors

	/// Borrow the launch properties handle.
	[[nodiscard]] auto handle () const -> SDL_PropertiesID { return m_handle; }


private:

	////
	// Fields

	/// SDL-owned property storage released after launch, before any deferred future can trigger SDL shutdown.
	SDL_PropertiesID m_handle = 0;
};

/// Owned launch data shared by the launching stack, native callback, and optional main-thread dispatch.
struct DialogState
{
	/// Original settings, including stable filter labels and the borrowed parent.
	FileDialogOptions options;

	/// UTF-8 location passed to SDL.
	std::string location;

	/// Owned semicolon-separated patterns.
	std::vector<std::string> patterns;

	/// Stable array referencing owned labels and patterns.
	std::vector<SDL_DialogFileFilter> filters;

	/// Per-launch backend functions, copied for deferred dispatch.
	FileDialogBackend backend;

	/// Future completion, absent for callback launches.
	std::optional<std::promise<FileDialogResult>> promise;

	/// Callback completion, absent for future launches.
	std::move_only_function<void(FileDialogResult)> callback;

	/// Requested callback thread.
	FileDialogCallbackThread thread = FileDialogCallbackThread::MainThread;

	/// Owned result carried to a deferred main-thread callback.
	std::optional<FileDialogResult> result;
};

/// Shared lifetime token passed to SDL and consumed exactly once by each C callback.
using StateToken = std::shared_ptr<DialogState>;

/// Capture the calling thread's SDL error before any cleanup can overwrite it.
auto sdlFailure (std::string_view operation) -> std::unexpected<FileDialogError> {
	const std::string error = SDL_GetError();
	return std::unexpected(FileDialogError{
		FileDialogErrorCode::SDLFailure, std::string(operation) + ": " + error});
}

/// Validate one extension according to SDL's pattern grammar, without accepting separators or globbing.
auto validExtension (std::string_view extension) -> bool {
	return !extension.empty() && !extension.starts_with('.') && std::ranges::all_of(extension, [] (char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
			|| c == '-' || c == '_' || c == '.';
	});
}

/// Validate settings and build all backing strings before taking native pointers.
auto prepare (DialogState &state, SDL_FileDialogType type) -> std::expected<void, FileDialogError>
{
	using enum FileDialogErrorCode;
	if (!SDL_IsMainThread() || !(SDL_WasInit(SDL_INIT_EVENTS) & SDL_INIT_EVENTS))
		return std::unexpected(FileDialogError{InvalidState, "Dialogs require the main thread and SDL events"});
	if (state.options.title.find('\0') != std::string::npos)
		return std::unexpected(FileDialogError{InvalidArgument, "Dialog title contains a NUL"});
	try {
		const auto utf8 = state.options.defaultLocation.u8string();
		state.location.assign((const char*)utf8.data(), utf8.size());
	} catch (const std::filesystem::filesystem_error &error) {
		return std::unexpected(FileDialogError{InvalidArgument, error.what()});
	}
	if (state.location.find('\0') != std::string::npos)
		return std::unexpected(FileDialogError{InvalidArgument, "Dialog location contains a NUL"});
	if (type == SDL_FILEDIALOG_OPENFOLDER)
		return {};
	if (state.options.filters.size() > (std::size_t)std::numeric_limits<int>::max())
		return std::unexpected(FileDialogError{InvalidArgument, "Too many dialog filters"});
	state.patterns.reserve(state.options.filters.size());
	for (const auto &filter : state.options.filters)
	{
		if (filter.name.empty() || filter.name.find('\0') != std::string::npos || filter.extensions.empty())
			return std::unexpected(FileDialogError{InvalidArgument, "Filters require a label and extensions"});
		std::string pattern;
		for (const auto &extension : filter.extensions)
		{
			if (!(extension == "*" && filter.extensions.size() == 1) && !validExtension(extension))
				return std::unexpected(FileDialogError{InvalidArgument, "Invalid filter extension: " + extension});
			if (!pattern.empty())
				pattern += ';';
			pattern += extension;
		}
		state.patterns.push_back(std::move(pattern));
	}
	state.filters.reserve(state.patterns.size());
	for (std::size_t i = 0; i < state.patterns.size(); ++i)
		state.filters.push_back({state.options.filters[i].name.c_str(), state.patterns[i].c_str()});
	return {};
}

/// Consume the dispatch token even when SDL invokes this inline from its dispatch function.
void SDLCALL mainThreadCompletion (void *userdata) noexcept
{
	try {
		std::unique_ptr<StateToken> token((StateToken*)userdata);
		// SDL falls back to inline delivery without initialized events, even on a worker.
		if (!SDL_IsMainThread())
			std::terminate();
		auto &state = **token;
		state.callback(std::move(*state.result));
	} catch (...) {
		std::terminate();
	}
}

/// Fulfil promises on SDL's completion thread or transfer ownership to main-thread event processing.
void complete (const StateToken &state, FileDialogResult result) noexcept
{
	if (state->promise)
		state->promise->set_value(std::move(result));
	else if (state->thread == FileDialogCallbackThread::SDLThread)
		state->callback(std::move(result));
	else
	{
		state->result.emplace(std::move(result));
		auto *token = new StateToken(state);
		// Do not touch the token after successful dispatch: inline completion already consumed it.
		if (!state->backend.dispatch(mainThreadCompletion, token, false)) {
			const auto error = sdlFailure("Dispatching dialog callback");
			delete token;
			std::terminate();
		}
	}
}

/// Copy the borrowed native result before SDL returns and releases its strings.
void SDLCALL nativeCompletion (void *userdata, const char *const *files, int filter) noexcept
{
	try
	{
		std::unique_ptr<StateToken> token((StateToken*)userdata);
		const auto state = *token;
		if (!files) {
			complete(state, sdlFailure("Completing file dialog"));
			return;
		}
		FileDialogSelection selection;
		try {
			for (auto file = files; *file; ++file) {
				const std::string_view utf8(*file);
				selection.paths.emplace_back(std::u8string_view((const char8_t*)utf8.data(), utf8.size()));
			}
		} catch (const std::filesystem::filesystem_error &error) {
			complete(state, std::unexpected(FileDialogError{FileDialogErrorCode::SDLFailure, error.what()}));
			return;
		}
		if (filter >= 0 && (std::size_t)filter < state->filters.size())
			selection.selectedFilter = (std::size_t)filter;
		complete(state, std::move(selection));
	} catch (...) {
		std::terminate();
	}
}

/// Prepare and launch, retaining a separate stack reference for inline native completion.
void launch (SDL_FileDialogType type, const StateToken &state)
{
	if (auto valid = prepare(*state, type); !valid) {
		complete(state, std::unexpected(std::move(valid.error())));
		return;
	}
	const DialogProperties properties(state->backend.createProperties());
	const auto props = properties.handle();
	if (!props) {
		complete(state, sdlFailure("Creating dialog properties"));
		return;
	}
	if (   (   !state->filters.empty()
	        && (   !SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, state->filters.data())
	            || !SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, (Sint64)state->filters.size())))
	    || (   state->options.parent
	        && !SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, state->options.parent->handle()))
	    || (   !state->location.empty()
	        && !SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, state->location.c_str()))
	    || (   !state->options.title.empty()
	        && !SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, state->options.title.c_str()))
	    || !SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, (
	        	state->options.allowMultiple && type != SDL_FILEDIALOG_SAVEFILE
	        )))
	{
		complete(state, sdlFailure("Setting dialog properties"));
		return;
	}
	state->backend.show(type, nativeCompletion, new StateToken(state), props);
}

auto showFileDialog (SDL_FileDialogType type, FileDialogOptions options, FileDialogBackend backend={})
	-> std::future<FileDialogResult>
{
	auto state = std::make_shared<DialogState>();
	state->options = std::move(options);
	state->backend = backend;
	state->promise.emplace();
	auto future = state->promise->get_future();
	launch(type, state);
	return future;
}

void showFileDialog (
	SDL_FileDialogType type, FileDialogOptions options, std::move_only_function<void(FileDialogResult)> callback,
	FileDialogCallbackThread thread, FileDialogBackend backend={}
){
	if (!callback)
		throw std::invalid_argument("File dialog callback is empty");
	auto state = std::make_shared<DialogState>();
	state->options = std::move(options);
	state->backend = backend;
	state->callback = std::move(callback);
	state->thread = thread;
	launch(type, state);
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// Our library namespace.
namespace fcg::extra {



//////
//
// Function implementations
//

auto showOpenFileDialog (FileDialogOptions options) -> std::future<FileDialogResult> {
	return showFileDialog(SDL_FILEDIALOG_OPENFILE, std::move(options));
}

void showOpenFileDialog (
	FileDialogOptions options, std::move_only_function<void(FileDialogResult)> callback, FileDialogCallbackThread thread
){
	showFileDialog(SDL_FILEDIALOG_OPENFILE, std::move(options), std::move(callback), thread);
}

auto showSaveFileDialog (FileDialogOptions options) -> std::future<FileDialogResult> {
	return showFileDialog(SDL_FILEDIALOG_SAVEFILE, std::move(options));
}

void showSaveFileDialog (
	FileDialogOptions options, std::move_only_function<void(FileDialogResult)> callback, FileDialogCallbackThread thread
){
	showFileDialog(SDL_FILEDIALOG_SAVEFILE, std::move(options), std::move(callback), thread);
}

auto showOpenFolderDialog (FileDialogOptions options) -> std::future<FileDialogResult> {
	return showFileDialog(SDL_FILEDIALOG_OPENFOLDER, std::move(options));
}

void showOpenFolderDialog (
	FileDialogOptions options, std::move_only_function<void(FileDialogResult)> callback, FileDialogCallbackThread thread
){
	showFileDialog(SDL_FILEDIALOG_OPENFOLDER, std::move(options), std::move(callback), thread);
}



//////
//
// Module namespace close
//

// namespace fcg::extra
}
