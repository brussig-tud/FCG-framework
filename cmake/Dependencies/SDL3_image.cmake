
# Prefer an existing target/system SDL_image; source defaults can be overridden with upstream SDLIMAGE_* variables.
# Scoped defaults avoid affecting parent projects or overwriting cache entries. Do not put these in CPM OPTIONS:
# CPM's normal variables would shadow the caller's cache choices.
set(FCG_SDL_IMAGE_BUILT_HERE OFF)
set(image_backend_lookup_performed OFF)
if(NOT TARGET SDL3_image::SDL3_image)
	set(image_backend_lookup_performed ON)
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

# Requested options do not describe resolved capabilities. Source builds retain the final values in their
# CMake directory; packages report the resolved values in SDL3_imageConfig.cmake. CPM's find_package function
# hides those variables, so recover them here after it has made the package/source selection.
set(fcg_image_codecs AVIF BMP GIF JPG JXL LBM PCX PNG PNM QOI SVG TGA TIF WEBP XCF XPM XV)
set(fcg_image_formats)
set(fcg_image_metadata_available ON)
get_target_property(image_backend_imported SDL3_image::SDL3_image IMPORTED)
if(NOT image_backend_imported)
	get_target_property(image_backend_source SDL3_image::SDL3_image SOURCE_DIR)
	get_directory_property(image_backend_variables DIRECTORY "${image_backend_source}" VARIABLES)
	foreach(codec IN LISTS fcg_image_codecs)
		if(NOT "SDLIMAGE_${codec}_ENABLED" IN_LIST image_backend_variables)
			set(fcg_image_metadata_available OFF)
			break()
		endif()
		get_directory_property(enabled DIRECTORY "${image_backend_source}" DEFINITION SDLIMAGE_${codec}_ENABLED)
		if(enabled)
			list(APPEND fcg_image_formats "${codec}")
		endif()
	endforeach()
else()
	block(SCOPE_FOR VARIABLES PROPAGATE fcg_image_formats fcg_image_metadata_available)
		# Already-visible package reports are useful for caller-supplied imported packages too.
		set(package_report_available "${SDL3_image_FOUND}")
		foreach(codec IN LISTS fcg_image_codecs)
			if(NOT DEFINED SDLIMAGE_${codec})
				set(package_report_available OFF)
			endif()
		endforeach()
		if(NOT package_report_available AND image_backend_lookup_performed)
			foreach(codec IN LISTS fcg_image_codecs)
				set(SDLIMAGE_${codec} FCG_UNREPORTED)
			endforeach()
			find_package(SDL3_image 3.4.4 CONFIG QUIET)
		elseif(NOT package_report_available)
			set(fcg_image_metadata_available OFF)
		endif()
		foreach(codec IN LISTS fcg_image_codecs)
			if(NOT SDL3_image_FOUND OR NOT DEFINED SDLIMAGE_${codec} OR SDLIMAGE_${codec} STREQUAL "FCG_UNREPORTED")
				set(fcg_image_metadata_available OFF)
				break()
			endif()
			if(SDLIMAGE_${codec})
				list(APPEND fcg_image_formats "${codec}")
			endif()
		endforeach()
	endblock()
endif()
if(NOT fcg_image_metadata_available)
	if(NOT DEFINED FCG_SDL_IMAGE_FORMATS)
		message(FATAL_ERROR "SDL_image has no trustworthy format metadata. Set FCG_SDL_IMAGE_FORMATS to a list of SDL codec identifiers (an explicitly empty list is accepted).")
	endif()
	set(fcg_image_formats "${FCG_SDL_IMAGE_FORMATS}")
endif()
# ANI is accepted in a manual declaration but is deliberately omitted from the still-image API.
foreach(codec IN LISTS fcg_image_formats)
	if(NOT codec IN_LIST fcg_image_codecs AND NOT codec STREQUAL "ANI")
		message(FATAL_ERROR "Unknown SDL codec identifier in FCG_SDL_IMAGE_FORMATS: ${codec}")
	endif()
endforeach()
list(REMOVE_DUPLICATES fcg_image_formats)
set_property(TARGET fcg-sdl-image PROPERTY FCG_SDL_IMAGE_FORMATS "${fcg_image_formats}")
