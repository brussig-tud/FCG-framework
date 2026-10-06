
# Prefer an existing target/system SDL_image; source defaults can be overridden with upstream SDLIMAGE_* variables.
# Scoped defaults avoid affecting parent projects or overwriting cache entries. Do not put these in CPM OPTIONS:
# CPM's normal variables would shadow the caller's cache choices.
set(FCG_SDL_IMAGE_BUILT_HERE OFF)
if(NOT TARGET SDL3_image::SDL3_image)
	block(SCOPE_FOR VARIABLES PROPAGATE FCG_SDL_IMAGE_BUILT_HERE SDL3_image_SOURCE_DIR SDL3_image_BINARY_DIR)
		foreach(feature AVIF JXL TIF WEBP PNG_LIBPNG VENDORED DEPS_SHARED SAMPLES TESTS)
			if(NOT DEFINED SDLIMAGE_${feature})
				set(SDLIMAGE_${feature} OFF)
			endif()
		endforeach()
		# SDL_image 3.4 uses BUILD_SHARED_LIBS rather than separate SDLIMAGE_SHARED/STATIC options.
		set(BUILD_SHARED_LIBS "${FCG_USE_SHARED_SDL}")
		if(SDLIMAGE_VENDORED)
			# The release git checkout needs its external codec repositories when vendoring is requested.
			set(image_submodules)
			foreach(pair "AVIF;external/libavif;external/dav1d;external/aom" "JXL;external/libjxl"
				"TIF;external/libtiff" "WEBP;external/libwebp" "PNG_LIBPNG;external/libpng;external/zlib")
				list(POP_FRONT pair feature)
				if(SDLIMAGE_${feature})
					list(APPEND image_submodules ${pair})
				endif()
			endforeach()
			if(DEFINED SDLIMAGE_BACKEND_STB AND NOT SDLIMAGE_BACKEND_STB)
				list(APPEND image_submodules external/jpeg)
			endif()
		else()
			set(image_submodules "")
		endif()
		CPMFindPackage(
			NAME SDL3_image
			GITHUB_REPOSITORY libsdl-org/SDL_image
			GIT_TAG release-3.4.4
			VERSION 3.4.4
			GIT_SUBMODULES "${image_submodules}"
		)
		if(SDL3_image_ADDED)
			set(FCG_SDL_IMAGE_BUILT_HERE ON)
		endif()
	endblock()
endif()

# Consumers link this build-only adapter without knowing the backend's linkage or packaging details.
add_library(fcg-sdl-image INTERFACE)
get_target_property(image_backend_type SDL3_image::SDL3_image TYPE)
if(BUILD_SHARED_LIBS AND image_backend_type STREQUAL "SHARED_LIBRARY")
	# CMake exports private shared target dependencies even through BUILD_INTERFACE. Use the linker
	# artifact so installed framework targets do not require an unexported third-party target.
	target_include_directories(fcg-sdl-image INTERFACE
		"$<TARGET_PROPERTY:SDL3_image::SDL3_image,INTERFACE_INCLUDE_DIRECTORIES>")
	target_link_libraries(fcg-sdl-image INTERFACE "$<TARGET_LINKER_FILE:SDL3_image::SDL3_image>")
	add_dependencies(fcg-sdl-image SDL3_image::SDL3_image)
else()
	# Static backends need their transitive codec dependencies to reach the final linker.
	target_link_libraries(fcg-sdl-image INTERFACE SDL3_image::SDL3_image)
endif()
