# Prefer an existing target/system SDL_image; source defaults can be overridden with upstream SDLIMAGE_* variables.
# Scoped defaults avoid affecting parent projects or overwriting cache entries. Do not put these in CPM OPTIONS:
# CPM's normal variables would shadow the caller's cache choices.
if(NOT TARGET SDL3_image::SDL3_image)
	block(SCOPE_FOR VARIABLES PROPAGATE FCG_SDL_IMAGE_BUILT_HERE)
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
