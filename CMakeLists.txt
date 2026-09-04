cmake_minimum_required(VERSION 3.31)

# Setup CMake project.
set(FCG_PROJECT_NAME FCG-framework)
project(${FCG_PROJECT_NAME})

# Library version
set(FCG_FRAMEWORK_VERSION 0.0.1)
set(FCG_FRAMEWORK_SOVERSION 0)
set(FCG_CXX_STANDARD cxx_std_23)

# Pre-configuration.
set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if (NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET AND NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
	set(CMAKE_CXX_VISIBILITY_PRESET hidden)
	set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif()
# Compile everything position-independent. This is required to make sure dependencies which we want to link in
# statically to our shared libraries can actually be linked that way. Note that any OS-installed packages of our
# dependencies may not be compiled with PIC, in that case it will be necessary to force using the source-fetched
# versions of those dependencies if this configuration is desired.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GenerateExportHeader)

# Library shared/static decision.
if (DEFINED FCG_SHARED_LIBS)
	# Locally (that is, for the FCG-build only) override the global BUILD_SHARED_LIBS variable according to indicated
	# client preference
	set(BUILD_SHARED_LIBS "${FCG_SHARED_LIBS}")
	set(SDL_SHARED_DEFAULT "${FCG_SHARED_LIBS}")
elseif(DEFINED BUILD_SHARED_LIBS)
	set(SDL_SHARED_DEFAULT "${BUILD_SHARED_LIBS}")
else()
	set(SDL_SHARED_DEFAULT OFF)
endif()
option(FCG_USE_SHARED_SDL "Use shared SDL3 library" "${SDL_SHARED_DEFAULT}")

# Dependencies (SDL3, GLM, ImGui) - declared in cmake/Dependencies.cmake.
include(cmake/Dependencies.cmake)

# The framework core library.
add_library(Core)
list(APPEND FCG_INCLUDED_LIBS Core)
add_library(${FCG_PROJECT_NAME}::Core ALIAS Core)
set_target_properties(Core PROPERTIES VERSION ${FCG_FRAMEWORK_VERSION} SOVERSION ${FCG_FRAMEWORK_SOVERSION})
target_include_directories(Core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(Core PUBLIC ${FCG_CXX_STANDARD})
generate_export_header(Core BASE_NAME ${FCG_PROJECT_NAME} EXPORT_FILE_NAME include/FCG/export.h)
target_compile_definitions(Core PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:FCG_STATIC_DEFINE>")
target_include_directories(Core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

# Link against SDL3 and GLM. Public, so that clients writing applets also get the SDL GPU and GLM headers. For
# now the dependencies are limited to the build interface – properly exporting them is left for future packaging
# work.
target_link_libraries(
	Core PUBLIC "$<BUILD_INTERFACE:SDL3::SDL3>" "$<BUILD_INTERFACE:glm::glm>" "$<BUILD_INTERFACE:imgui::imgui>"
)
# Enable the experimental GLM GTX extensions (e.g. GLM_GTX_color_space used for color handling).
target_compile_definitions(Core PUBLIC GLM_ENABLE_EXPERIMENTAL)
add_subdirectory(src)

# (Optional) apps to build.
add_subdirectory(app)

# Smoke tests (Tier 0: installability, Tier 1: app lifecycle) - see tests/.
enable_testing()
add_subdirectory(tests)

# Packaging rules. Based on https://alexreinking.com/blog/building-a-dual-shared-and-static-library-with-cmake.html
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)
option(FCG_INCLUDE_PACKAGING "Include packaging rules for the FCG Framework library" "${is_top_level}")
if (FCG_INCLUDE_PACKAGING)
	add_subdirectory(packaging)
endif()

# Install logic. Based on https://alexreinking.com/blog/building-a-dual-shared-and-static-library-with-cmake.html
# - configuration-time feedback about installed targets
if(NOT FCG_APP_TARGETS)
	set(FCG_INCLUDED_APPS "<none>")
else()
	set(FCG_INCLUDED_APPS "${FCG_APP_TARGETS}")
endif()
message("Targets configured for install: ${FCG_INCLUDED_LIBS} – apps: ${FCG_INCLUDED_APPS}")
# - actual isntall logic
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
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"FCGConfigVersion.cmake"
	VERSION ${FCG_FRAMEWORK_VERSION}
	COMPATIBILITY SameMajorVersion
)
install(
	FILES "${CMAKE_CURRENT_SOURCE_DIR}/FCGConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/FCGConfigVersion.cmake"
	DESTINATION "${FCG_INSTALL_CMAKEDIR}"
	COMPONENT FCG_Development
)
