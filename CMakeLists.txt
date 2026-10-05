cmake_minimum_required(VERSION 3.31)

# Setup CMake project.
set(FCG_PROJECT_NAME FCG-framework)
project(${FCG_PROJECT_NAME})

# Library version
set(FCG_FRAMEWORK_VERSION 0.0.1)
set(FCG_FRAMEWORK_SOVERSION 0)
set(FCG_CXX_STANDARD cxx_std_23)

# Pre-configuration.
# Not using C++20 modules: skip the per-TU module scanner (also keeps
# compilations ccache-compatible).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

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
include(cmake/Shaders.cmake)
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
# Runtime SPIR-V translation for non-Vulkan backends (Metal, Direct3D 12).
target_link_libraries(Core PRIVATE "$<BUILD_INTERFACE:SDL3_shadercross::SDL3_shadercross>")
# Enable the experimental GLM GTX extensions (e.g. GLM_GTX_color_space used for color handling).
target_compile_definitions(Core PUBLIC GLM_ENABLE_EXPERIMENTAL)
add_subdirectory(src)

# Embedded shaders (cmake/Shaders.cmake).
add_subdirectory(shaders)
target_link_libraries(Core PRIVATE "$<BUILD_INTERFACE:fcg-resources>")

# (Optional) apps to build.
add_subdirectory(app)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)
option(FCG_INCLUDE_PACKAGING "Include packaging rules for the FCG Framework library" "${is_top_level}")

# Smoke tests (Tier 0: installability, Tier 1: app lifecycle) - see tests/.
enable_testing()
add_subdirectory(tests)

# Packaging rules. Based on https://alexreinking.com/blog/building-a-dual-shared-and-static-library-with-cmake.html
if (FCG_INCLUDE_PACKAGING)
	add_subdirectory(packaging)
endif()
