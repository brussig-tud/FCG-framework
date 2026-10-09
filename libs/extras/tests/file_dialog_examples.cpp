
//////
//
// Includes
//

// C++ STL
#include <chrono>
#include <memory>
#include <utility>

// FCG Framework
#include <FCG/Extras/file_dialog.h>



//////
//
// Functions
//

/// Launch once, then poll in an applet update while its parent and SDL remain alive.
void futureExample (fcg::Window *parent)
{
// [future]
	fcg::extra::FileDialogOptions options;
	options.parent = parent;
	options.filters = {{"Images", {"png", "jpg", "jpeg"}}, {"All files", {"*"}}};
	auto pending = fcg::extra::showOpenFileDialog(std::move(options));
	// Keep this future in the applet and poll from subsequent update calls.
	if (pending.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
		auto result = pending.get();
		if (result && !result->paths.empty())
			(void)result->paths.front(); // Load the selected native filesystem path.
	}
// [future]
}

/// Move-only callbacks default to main-thread delivery during SDL event processing.
void callbackExample ()
{
// [callback]
	fcg::extra::showOpenFolderDialog({.allowMultiple = true},
		[state = std::make_unique<int>(0)] (fcg::extra::FileDialogResult result) {
			if (result)
				*state = (int)result->paths.size(); // Zero means cancellation.
		});
// [callback]
}

/// Compile the remaining public overloads and the explicit completion-thread policy.
void overloadExamples ()
{
	auto save = fcg::extra::showSaveFileDialog();
	auto folder = fcg::extra::showOpenFolderDialog();
	fcg::extra::showOpenFileDialog({}, [] (fcg::extra::FileDialogResult) {}, fcg::extra::FileDialogCallbackThread::SDLThread);
	fcg::extra::showSaveFileDialog({}, [] (fcg::extra::FileDialogResult) {});
}
