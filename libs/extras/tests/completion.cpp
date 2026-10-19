
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// FCG Framework implementation
// The implementation's private backend is injectable; this test owns its copy without exporting a testing API.
#include "../src/file_dialog.cpp"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Deferred native completion function supplied by the production launcher.
SDL_DialogFileCallback pendingNative = nullptr;

/// Owned native completion token retained until the fake dialog finishes.
void *pendingNativeData = nullptr;

/// Deferred main-thread completion function supplied by the production completion path.
SDL_MainThreadCallback pendingDispatch = nullptr;

/// Owned main-thread completion token retained until the main thread drains it.
void *pendingDispatchData = nullptr;

/// Whether main-thread dispatch should fail.
bool rejectDispatch = false;

/// Whether the native backend should finish inline before the launch returns.
bool inlineNative = false;

/// Number of native dialog launches.
unsigned launches = 0;

/// Fail the test with the given diagnostic.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Return an owned selection, cancellation or native error through the production native callback.
void finishNative (int outcome)
{
	const auto callback = std::exchange(pendingNative, nullptr);
	auto *data = std::exchange(pendingNativeData, nullptr);
	const char *selected[] = {"selected.png", nullptr};
	const char *cancelled[] = {nullptr};
	SDL_SetError("native sentinel");
	callback(data, outcome == 0 ? selected : outcome == 1 ? cancelled : nullptr, 0);
}

/// Retain a native launch, optionally completing it inline to exercise shared launch-state ownership.
void SDLCALL showDialog (SDL_FileDialogType, SDL_DialogFileCallback callback, void *data, SDL_PropertiesID)
{
	++launches;
	pendingNative = callback;
	pendingNativeData = data;
	if (inlineNative)
		finishNative(0);
}

/// Either fail dispatch, deliver inline on the main thread, or retain a worker's completion for main-thread draining.
auto SDLCALL dispatch (SDL_MainThreadCallback callback, void *data, bool) -> bool
{
	if (rejectDispatch)
		return SDL_SetError("dispatch sentinel");
	if (SDL_IsMainThread())
		callback(data);
	else {
		pendingDispatch = callback;
		pendingDispatchData = data;
	}
	return true;
}

/// Complete one queued main-thread delivery.
void drainDispatch () {
	const auto callback = std::exchange(pendingDispatch, nullptr);
	auto *data = std::exchange(pendingDispatchData, nullptr);
	callback(data);
}

/// Build settings with an actual filter, allowing the native selection's filter index to be checked.
auto options () -> fcg::extra::FileDialogOptions {
	return {.filters = {{"Images", {"png"}}}};
}

/// Check successful selection, cancellation, and native failure for futures without dispatching to the main thread.
void futures (FileDialogBackend backend)
{
	rejectDispatch = true;
	for (int outcome = 0; outcome < 3; ++outcome)
	{
		auto future = showFileDialog(SDL_FILEDIALOG_OPENFILE, options(), backend);
		std::thread worker([outcome] { finishNative(outcome); });
		worker.join();
		auto result = future.get();
		if (outcome == 0)
			require(result && result->paths == std::vector<std::filesystem::path>{"selected.png"}
				&& result->selectedFilter == 0, "Future lost the native selection");
		else if (outcome == 1)
			require(result && result->paths.empty(), "Future lost cancellation");
		else
			require(!result && result.error().code == fcg::extra::FileDialogErrorCode::SDLFailure
				&& result.error().message.find("native sentinel") != std::string::npos, "Future lost the native error");
		require(!pendingDispatch, "Future unnecessarily dispatched completion");
	}
	rejectDispatch = false;
}

/// Check inline/deferred delivery, explicit SDL-thread delivery, and exactly-once dispatch-failure fallback.
void callbacks (FileDialogBackend backend)
{
	for (int mode = 0; mode < 4; ++mode)
	{
		unsigned calls = 0;
		bool valid = false;
		auto lifetime = std::make_shared<int>(0);
		std::weak_ptr<int> observed = lifetime;
		inlineNative = mode == 0;
		rejectDispatch = mode == 2;
		const auto thread = mode == 3 ? fcg::extra::FileDialogCallbackThread::SDLThread
			: fcg::extra::FileDialogCallbackThread::MainThread;
		showFileDialog(SDL_FILEDIALOG_OPENFILE, options(),
			[&, lifetime = std::move(lifetime)] (fcg::extra::FileDialogResult result) {
				++calls;
				valid = mode == 2
					? !result && result.error().code == fcg::extra::FileDialogErrorCode::SDLFailure
						&& result.error().message.find("dispatch sentinel") != std::string::npos && !SDL_IsMainThread()
					: result && result->paths.size() == 1 && SDL_IsMainThread() == (mode < 2);
				++*lifetime;
			}, thread, backend);
		if (mode != 0) {
			std::thread worker([] { finishNative(0); });
			worker.join();
		}
		if (mode == 1) {
			require(!calls && !observed.expired() && pendingDispatch, "Deferred callback ran on the worker");
			drainDispatch();
		}
		require(calls == 1 && valid && observed.expired(), "Callback result, thread, count or capture lifetime is wrong");
		require(!pendingDispatch && !pendingNative, "Completion retained a consumed token");
	}
	inlineNative = false;
	rejectDispatch = false;
}

/// Check that separate wildcard filters reach native properties while mixed wildcard lists fail validation.
void filters (FileDialogBackend backend)
{
	backend.show = [] (SDL_FileDialogType, SDL_DialogFileCallback callback, void *data, SDL_PropertiesID props) {
		++launches;
		const auto *nativeFilters = (const SDL_DialogFileFilter*)SDL_GetPointerProperty(
			props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, nullptr);
		require(SDL_GetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 0) == 3 && nativeFilters,
			"Native launch lost separate filters");
		require(std::string_view(nativeFilters[0].name) == "Images" && std::string_view(nativeFilters[0].pattern) == "png;jpg"
			&& std::string_view(nativeFilters[1].name) == "Any image" && std::string_view(nativeFilters[1].pattern) == "*"
			&& std::string_view(nativeFilters[2].name) == "All files" && std::string_view(nativeFilters[2].pattern) == "*",
			"Native launch changed filter labels, patterns, or order");
		const char *cancelled[] = {nullptr};
		callback(data, cancelled, 2);
	};
	const auto previousLaunches = launches;
	auto future = showFileDialog(SDL_FILEDIALOG_OPENFILE, {
		.filters = {{"Images", {"png", "jpg"}}, {"Any image", {"*"}}, {"All files", {"*"}}}
	}, backend);
	const auto result = future.get();
	require(result && result->paths.empty() && result->selectedFilter == 2 && launches == previousLaunches + 1,
		"Separate wildcard filters were rejected or their selection index was lost");
	for (const auto &extensions : {std::vector<std::string>{"png", "*"}, std::vector<std::string>{"*", "png"}})
	{
		auto invalid = showFileDialog(SDL_FILEDIALOG_OPENFILE, {.filters = {{"Invalid", extensions}}}, backend);
		const auto error = invalid.get();
		require(!error && error.error().code == fcg::extra::FileDialogErrorCode::InvalidArgument
			&& error.error().message == "Invalid filter extension: *" && launches == previousLaunches + 1,
			"Mixed wildcard filter did not fail before native launch");
	}
}

/// Check launch errors, including SDL's inline worker fallback without the event subsystem.
void launchErrors (FileDialogBackend backend)
{
	const auto previousLaunches = launches;
	unsigned calls = 0;
	showFileDialog(SDL_FILEDIALOG_OPENFILE, {.title = std::string("bad\0title", 9)},
		[&] (fcg::extra::FileDialogResult result) {
			if (!result && result.error().code == fcg::extra::FileDialogErrorCode::InvalidArgument)
				++calls;
		}, fcg::extra::FileDialogCallbackThread::MainThread, backend);
	require(calls == 1 && launches == previousLaunches, "Invalid launch reached the native backend");
	SDL_Quit();
	bool valid = false;
	backend.dispatch = SDL_RunOnMainThread;
	std::thread worker([&] {
		auto future = showFileDialog(SDL_FILEDIALOG_OPENFILE, {}, backend);
		const auto result = future.get();
		valid = !result && result.error().code == fcg::extra::FileDialogErrorCode::InvalidState;
		showFileDialog(SDL_FILEDIALOG_OPENFILE, {}, [&] (fcg::extra::FileDialogResult error) {
			++calls;
			valid = valid && !error && error.error().code == fcg::extra::FileDialogErrorCode::InvalidState
				&& !SDL_IsMainThread();
		}, fcg::extra::FileDialogCallbackThread::MainThread, backend);
	});
	worker.join();
	require(valid && calls == 2 && launches == previousLaunches, "Unavailable main-thread delivery lost its error");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Exercise completion ownership and failures without displaying native dialogs.
auto main () -> int
{
	try {
		require(SDL_Init(SDL_INIT_EVENTS), "Could not initialize events");
		const FileDialogBackend backend{.show = showDialog, .dispatch = dispatch};
		futures(backend);
		callbacks(backend);
		filters(backend);
		launchErrors(backend);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		SDL_Quit();
		return 1;
	}
}
