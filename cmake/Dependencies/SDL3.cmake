
if(NOT TARGET SDL3::SDL3)
	# Mirror the framework's shared/static decision onto source-built dependencies. (Presets don't apply to embedded
	# consumers, so this must stay in CMake code.)
	if (FCG_USE_SHARED_SDL)
		set(SDL_SHARED ON)
		set(SDL_STATIC OFF)
	else()
		set(SDL_SHARED OFF)
		set(SDL_STATIC ON)
	endif()

	# SDL3: window creation, input events, SDL GPU rendering API.
	CPMFindPackage(
		NAME              SDL3
		GITHUB_REPOSITORY libsdl-org/SDL
		GIT_TAG           release-3.4.14
		VERSION           3.4.14
		OPTIONS
			# XTest is a niche runtime feature (X11 mouse warping, currently disabled upstream) also used by SDL's own tests
			# - off avoids the libxtst dependency
			"SDL_X11_XTEST OFF"
			# We don't need any screensaver functionality - off avoids this niche dependency
			"SDL_X11_XSCRNSAVER OFF"
	)
endif()
