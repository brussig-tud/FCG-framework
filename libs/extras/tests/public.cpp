
//////
//
// Includes
//

// C++ STL
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/Extras/file_dialog.h>



//////
//
// Functions
//

/// Link and exercise all public overloads against the production static or shared library without native UI.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_EVENTS))
		return 1;
	const fcg::extra::FileDialogOptions invalid{.title = std::string("bad\0title", 9)};
	int calls = 0;
	auto check = [&] (fcg::extra::FileDialogResult result) {
		if (!result && result.error().code == fcg::extra::FileDialogErrorCode::InvalidArgument)
			++calls;
	};
	check(fcg::extra::showOpenFileDialogAsync(invalid).get());
	check(fcg::extra::showSaveFileDialogAsync(invalid).get());
	check(fcg::extra::showOpenFolderDialogAsync(invalid).get());
	fcg::extra::showOpenFileDialogCallback(invalid, check);
	fcg::extra::showSaveFileDialogCallback(invalid, check, fcg::extra::FileDialogCallbackThread::SDLThread);
	fcg::extra::showOpenFolderDialogCallback(invalid, check);
	check(fcg::extra::showOpenFileDialog(invalid));
	check(fcg::extra::showSaveFileDialog(invalid));
	check(fcg::extra::showOpenFolderDialog(invalid));
	int rejected = 0;
	for (auto launch : {fcg::extra::showOpenFileDialogCallback, fcg::extra::showSaveFileDialogCallback,
		fcg::extra::showOpenFolderDialogCallback})
	{
		try {
			launch(invalid, {}, fcg::extra::FileDialogCallbackThread::MainThread);
		}
		catch (const std::invalid_argument&) {
			++rejected;
		}
	}
	// Copies of the callback must retain captured state and receive exactly one result per launch.
	auto completed = std::make_shared<int>(0);
	std::function<void(fcg::extra::FileDialogResult)> callback = [completed] (auto result) {
		if (!result && result.error().code == fcg::extra::FileDialogErrorCode::InvalidArgument)
			++*completed;
	};
	for (auto launch : {fcg::extra::showOpenFileDialogCallback, fcg::extra::showSaveFileDialogCallback,
		fcg::extra::showOpenFolderDialogCallback})
		launch(invalid, callback, fcg::extra::FileDialogCallbackThread::MainThread);
	SDL_PumpEvents();
	SDL_Quit();
	std::cout << "Public dialog overloads completed " << calls << " validations\n";
	return calls == 9 && rejected == 3 && *completed == 3 ? 0 : 1;
}
