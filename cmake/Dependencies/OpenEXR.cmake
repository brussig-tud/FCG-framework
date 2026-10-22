
# Included only when explicitly enabled. Source fallbacks are static/PIC, independent of framework linkage.
if(NOT TARGET OpenEXR::OpenEXR)
	block(SCOPE_FOR VARIABLES PROPAGATE FCG_OPENEXR_BUILT_HERE FCG_IMATH_BUILT_HERE
		OpenEXR_SOURCE_DIR OpenEXR_BINARY_DIR Imath_SOURCE_DIR Imath_BINARY_DIR)
		CPMFindPackage(
			NAME OpenEXR
			GITHUB_REPOSITORY AcademySoftwareFoundation/openexr
			GIT_TAG v3.4.14
			VERSION 3.4.14
			DOWNLOAD_ONLY YES
		)
		if(NOT TARGET OpenEXR::OpenEXR AND OpenEXR_SOURCE_DIR)
			set(BUILD_SHARED_LIBS OFF)
			set(CMAKE_POSITION_INDEPENDENT_CODE ON)
			foreach(feature BUILD_TOOLS BUILD_EXAMPLES BUILD_PYTHON BUILD_OSS_FUZZ TEST_LIBRARIES
				TEST_TOOLS TEST_PYTHON INSTALL INSTALL_TOOLS INSTALL_DEVELOPER_TOOLS INSTALL_DOCS)
				if(NOT DEFINED OPENEXR_${feature})
					set(OPENEXR_${feature} OFF)
				endif()
			endforeach()
			if(NOT TARGET Imath::Imath)
				CPMFindPackage(
					NAME Imath
					GITHUB_REPOSITORY AcademySoftwareFoundation/Imath
					GIT_TAG v3.2.2
					VERSION 3.1
					DOWNLOAD_ONLY YES
				)
				if(NOT TARGET Imath::Imath AND Imath_SOURCE_DIR)
					if(NOT DEFINED IMATH_INSTALL)
						set(IMATH_INSTALL OFF)
					endif()
					add_subdirectory("${Imath_SOURCE_DIR}" "${Imath_BINARY_DIR}" EXCLUDE_FROM_ALL)
					set(FCG_IMATH_BUILT_HERE ON)
				endif()
			endif()
			if(NOT TARGET Imath::Imath)
				message(FATAL_ERROR "OpenEXR source builds require Imath::Imath")
			endif()
			# OpenEXR calls find_package even for an existing target. Imath's uninstalled build config
			# has no exported targets file, so publish a build-only redirect for the resolved target.
			include(CMakePackageConfigHelpers)
			if(NOT Imath_VERSION)
				set(Imath_VERSION 3.1.0)
			endif()
			file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/imath-config.cmake"
				"set(Imath_FOUND TRUE)\nset(Imath_VERSION ${Imath_VERSION})\n")
			write_basic_package_version_file("${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/imath-config-version.cmake"
				VERSION "${Imath_VERSION}" COMPATIBILITY SameMajorVersion)
			add_subdirectory("${OpenEXR_SOURCE_DIR}" "${OpenEXR_BINARY_DIR}" EXCLUDE_FROM_ALL)
			set(FCG_OPENEXR_BUILT_HERE ON)
		endif()
	endblock()
endif()
if(NOT TARGET OpenEXR::OpenEXR)
	message(FATAL_ERROR "OpenEXR image support requires OpenEXR::OpenEXR (3.4.14 or newer)")
endif()
add_library(fcg-openexr INTERFACE)
get_target_property(openexr_type OpenEXR::OpenEXR TYPE)
if(BUILD_SHARED_LIBS AND openexr_type STREQUAL "SHARED_LIBRARY")
	# Match the SDL_image adapter: avoid exporting a dependency on an unexported third-party target.
	target_link_libraries(fcg-openexr INTERFACE "$<COMPILE_ONLY:OpenEXR::OpenEXR>"
		"$<TARGET_LINKER_FILE:OpenEXR::OpenEXR>")
	add_dependencies(fcg-openexr OpenEXR::OpenEXR)
else()
	target_link_libraries(fcg-openexr INTERFACE OpenEXR::OpenEXR)
endif()
