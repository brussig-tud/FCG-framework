
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if (NOT DEFINED FCG_INSTALL_CMAKEDIR)
	set(FCG_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/FCG" CACHE STRING "Path to FCG Framework CMake files")
endif()

# Install logic. Based on https://alexreinking.com/blog/building-a-dual-shared-and-static-library-with-cmake.html
# - configuration-time feedback about installed targets
if(NOT FCG_APP_TARGETS)
	set(FCG_INCLUDED_APPS "<none>")
else()
	set(FCG_INCLUDED_APPS "${FCG_APP_TARGETS}")
endif()
message("Targets configured for install: ${FCG_INCLUDED_LIBS} – apps: ${FCG_INCLUDED_APPS}")
# - installation rules
install(
	TARGETS ${FCG_INCLUDED_LIBS} ${FCG_APP_TARGETS} EXPORT FCG_Targets
	RUNTIME COMPONENT FCG_Runtime
	LIBRARY COMPONENT FCG_Runtime
	NAMELINK_COMPONENT FCG_Development
	ARCHIVE COMPONENT FCG_Development
	INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
# Static Render consumers also need the private embedded shader archive.
if(NOT BUILD_SHARED_LIBS)
	install(TARGETS fcg-render-shaders EXPORT FCG_Targets ARCHIVE COMPONENT FCG_Development)
endif()
# Install fetched ImGui binaries alongside the framework so that shared-framework executables can find
# the library at runtime. Exporting the ImGui CMake target is left for future packaging work, mirroring
# how SDL3 and GLM are currently handled.
if (FCG_IMGUI_BUILT_HERE)
	install(TARGETS imgui
		RUNTIME COMPONENT FCG_Runtime
		LIBRARY COMPONENT FCG_Runtime
	)
endif()
foreach(library IN LISTS FCG_INCLUDED_LIBS)
	get_target_property(library_source_dir ${library} SOURCE_DIR)
	get_target_property(library_binary_dir ${library} BINARY_DIR)
	install(
		DIRECTORY "${library_source_dir}/include/" "${library_binary_dir}/include/"
		TYPE INCLUDE
		COMPONENT FCG_Development
	)
endforeach()
# A source-built shared SDL_image is part of the framework runtime.
if(FCG_SDL_IMAGE_BUILT_HERE AND FCG_USE_SHARED_SDL)
	# Copy runtime artifacts without introducing a partial third-party target export. Installing this target
	# directly would make CMake require it in Image's export set, including all of its upstream dependencies.
	if(WIN32)
		install(FILES "$<TARGET_FILE:SDL3_image-shared>" DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT FCG_Runtime)
	else()
		install(FILES "$<TARGET_FILE:SDL3_image-shared>" "$<TARGET_SONAME_FILE:SDL3_image-shared>"
			DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT FCG_Runtime)
	endif()
endif()
if (BUILD_SHARED_LIBS)
	set(type shared)
else ()
	set(type static)
endif ()
install(
	EXPORT FCG_Targets
	DESTINATION "${FCG_INSTALL_CMAKEDIR}"
	NAMESPACE ${FCG_PROJECT_NAME}::
	FILE FCG-${type}-targets.cmake
	COMPONENT FCG_Development
)
write_basic_package_version_file(
	"${PROJECT_BINARY_DIR}/FCGConfigVersion.cmake"
	VERSION ${FCG_FRAMEWORK_VERSION}
	COMPATIBILITY SameMajorVersion
)
install(
	FILES "${PROJECT_SOURCE_DIR}/FCGConfig.cmake" "${PROJECT_BINARY_DIR}/FCGConfigVersion.cmake"
	DESTINATION "${FCG_INSTALL_CMAKEDIR}"
	COMPONENT FCG_Development
)
