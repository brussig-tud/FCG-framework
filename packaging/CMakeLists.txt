
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
# Install fetched ImGui binaries alongside the framework so that shared-framework executables can find
# the library at runtime. Exporting the ImGui CMake target is left for future packaging work, mirroring
# how SDL3 and GLM are currently handled.
if (FCG_IMGUI_BUILT_HERE)
	install(TARGETS imgui
		RUNTIME COMPONENT FCG_Runtime
		LIBRARY COMPONENT FCG_Runtime
	)
endif()
install(
	DIRECTORY "${PROJECT_SOURCE_DIR}/include/" "${PROJECT_BINARY_DIR}/include/"
	TYPE INCLUDE
	COMPONENT FCG_Development
)
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
