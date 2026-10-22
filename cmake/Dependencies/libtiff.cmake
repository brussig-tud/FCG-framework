
# Called before configuring a source-built, non-vendored TIFF backend. Native platform decoders and explicit
# discovery/runtime overrides remain SDL_image's responsibility, including ImageIO's default on Apple platforms.
if(CMAKE_DISABLE_FIND_PACKAGE_TIFF OR (WIN32 AND SDLIMAGE_BACKEND_WIC) OR
	(APPLE AND (NOT DEFINED SDLIMAGE_BACKEND_IMAGEIO OR SDLIMAGE_BACKEND_IMAGEIO)))
	return()
endif()

block(SCOPE_FOR VARIABLES PROPAGATE FCG_LIBTIFF_BUILT_HERE TIFF_SOURCE_DIR TIFF_BINARY_DIR)
	# Match SDL_image's package preference, including explicit TIFF_DIR config-package overrides.
	if(NOT DEFINED CMAKE_FIND_PACKAGE_PREFER_CONFIG)
		set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
	endif()
	if(DEFINED SDLIMAGE_TIF_SHARED)
		set(tiff_shared "${SDLIMAGE_TIF_SHARED}")
	else()
		set(tiff_shared "${SDLIMAGE_DEPS_SHARED}")
	endif()
	if(tiff_shared AND DEFINED SDLIMAGE_DYNAMIC_TIF AND EXISTS "${SDLIMAGE_DYNAMIC_TIF}")
		return()
	endif()
	set(tiff_provided_target OFF)
	if(TARGET TIFF::tiff OR TARGET TIFF::TIFF OR TARGET tiff)
		set(tiff_provided_target ON)
	else()
		# VERSION accepts compatible installed libraries; GIT_TAG independently pins the source fallback.
		CPMFindPackage(
			NAME TIFF
			GIT_REPOSITORY https://gitlab.com/libtiff/libtiff.git
			GIT_TAG v4.7.2
			VERSION 4.0
			DOWNLOAD_ONLY YES
		)
	endif()
	if(NOT TARGET TIFF::tiff AND NOT TARGET TIFF::TIFF AND NOT TARGET tiff AND TIFF_SOURCE_DIR)
		foreach(feature tools tests contrib docs cxx opengl install)
			if(NOT DEFINED tiff-${feature})
				set(tiff-${feature} OFF)
			endif()
		endforeach()
		# libtiff has a separate linkage switch in addition to BUILD_SHARED_LIBS.
		set(BUILD_SHARED_LIBS "${tiff_shared}")
		if(NOT DEFINED tiff-static)
			if(tiff_shared)
				set(tiff-static OFF)
			else()
				set(tiff-static ON)
			endif()
		endif()
		set(CMAKE_POSITION_INDEPENDENT_CODE ON)
		# Retain upstream discovery and the caller's choices for optional compression libraries.
		add_subdirectory("${TIFF_SOURCE_DIR}" "${TIFF_BINARY_DIR}" EXCLUDE_FROM_ALL)
		if(NOT TARGET tiff)
			message(FATAL_ERROR "Source-built libtiff did not provide required target tiff")
		endif()
		get_target_property(tiff_type tiff TYPE)
		if(tiff_shared AND NOT tiff_type STREQUAL "SHARED_LIBRARY")
			message(FATAL_ERROR "SDLIMAGE_TIF_SHARED requires shared libtiff; disable tiff-static")
		endif()
		set(FCG_LIBTIFF_BUILT_HERE ON)
	endif()
	if(NOT TARGET TIFF::tiff AND NOT TARGET TIFF::TIFF AND TARGET tiff)
		get_target_property(tiff_alias tiff ALIASED_TARGET)
		if(tiff_alias)
			add_library(TIFF::tiff ALIAS ${tiff_alias})
		else()
			add_library(TIFF::tiff ALIAS tiff)
		endif()
	endif()
	if(NOT TARGET TIFF::tiff AND NOT TARGET TIFF::TIFF)
		message(FATAL_ERROR "libtiff dependency resolution did not provide required target TIFF::tiff or TIFF::TIFF")
	endif()
	# DOWNLOAD_ONLY does not publish a package redirect. Let SDL_image's find_package(TIFF) reuse the target
	# already resolved in this directory without installing or exporting a third-party package.
	if(FCG_LIBTIFF_BUILT_HERE OR tiff_provided_target)
		include(CMakePackageConfigHelpers)
		if(FCG_LIBTIFF_BUILT_HERE)
			get_directory_property(tiff_package_version DIRECTORY "${TIFF_SOURCE_DIR}" DEFINITION PROJECT_VERSION)
		elseif(DEFINED TIFF_VERSION)
			set(tiff_package_version "${TIFF_VERSION}")
		elseif(DEFINED TIFF_VERSION_STRING)
			set(tiff_package_version "${TIFF_VERSION_STRING}")
		else()
			set(tiff_package_version 4.0)
		endif()
		file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/tiff-config.cmake"
			"if(NOT TARGET TIFF::tiff AND NOT TARGET TIFF::TIFF)\n  set(TIFF_FOUND FALSE)\nendif()\n")
		write_basic_package_version_file("${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/tiff-config-version.cmake"
			VERSION "${tiff_package_version}" COMPATIBILITY SameMajorVersion)
	endif()
endblock()
