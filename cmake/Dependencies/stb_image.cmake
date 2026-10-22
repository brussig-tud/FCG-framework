
# Independent, private implementation: installed SDL_image packages do not expose their internal stb header.
CPMAddPackage(
	NAME fcg_stb
	GITHUB_REPOSITORY nothings/stb
	GIT_TAG a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d
	DOWNLOAD_ONLY YES
)
if(NOT EXISTS "${fcg_stb_SOURCE_DIR}/stb_image.h")
	message(FATAL_ERROR "The fcg_stb source must provide stb_image.h")
endif()
add_library(fcg-stb-image INTERFACE)
target_include_directories(fcg-stb-image SYSTEM INTERFACE "${fcg_stb_SOURCE_DIR}")
