
//////
//
// Includes
//

// C++ STL
#include <iostream>

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
	SDL_Quit();
	std::cout << "Public dialog overloads completed " << calls << " validations\n";
	return calls == 6 ? 0 : 1;
}
