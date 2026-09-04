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

# Dependencies.
include(FetchContent)
# - SDL3: provides window creation, input events and the cross-platform SDL GPU API used for rendering. A
#   system-installed SDL3 (with CMake package config) is preferred, otherwise it is fetched from source.
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE) #  only used for SDL's own tests
if (FCG_USE_SHARED_SDL)
	set(SDL_SHARED ON CACHE BOOL "" FORCE)
	set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
	set(SDL_SHARED OFF CACHE BOOL "" FORCE)
	set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(
	SDL3
	GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
	GIT_TAG        release-3.4.14
	GIT_SHALLOW    TRUE
	FIND_PACKAGE_ARGS NAMES SDL3
)
FetchContent_MakeAvailable(SDL3)
# - GLM: header-only math library providing vector/matrix types and geometry utilities. A system-installed GLM
#   (with CMake package config) is preferred, otherwise it is fetched from source.
FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG        1.0.1
	GIT_SHALLOW    TRUE
	FIND_PACKAGE_ARGS NAMES glm
)
FetchContent_MakeAvailable(glm)
# - ImGui: graphics API-agnostic immediate mode GUI. Used for all graphical user interaction. A system-installed
#   ImGui (with CMake package config) is preferred, otherwise it is fetched from source. Note that upstream ImGui
#   ships no CMake build system of its own, so in the FetchContent case we have to assemble the library target
#   ourselves. The version gate makes sure we don't pick up packages too old to ship the SDL3 GPU backend.
find_package(imgui 1.92 CONFIG QUIET)
if (NOT imgui_FOUND)
	FetchContent_Declare(
		imgui
		GIT_REPOSITORY https://github.com/ocornut/imgui.git
		GIT_TAG        v1.92.9
		GIT_SHALLOW    TRUE
		# Upstream has no CMakeLists.txt – point at a non-existing subdirectory so FetchContent_MakeAvailable
		# does not attempt to add_subdirectory() the fetched repo (supported since CMake 3.18).
		SOURCE_SUBDIR  _never_add_subdirectory
	)
	FetchContent_MakeAvailable(imgui)
	# Build ImGui as a normal library (static for static framework builds, shared for shared framework
	# builds). It must be shared when the framework is shared, because applets call ImGui global functions
	# directly and the framework initializes the global ImGui context (GImGui). Two copies of the static
	# library would mean the applet sees a null context while the framework owns a separate one.
	add_library(imgui
		"${imgui_SOURCE_DIR}/imgui.cpp"
		"${imgui_SOURCE_DIR}/imgui_draw.cpp"
		"${imgui_SOURCE_DIR}/imgui_tables.cpp"
		"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
		"${imgui_SOURCE_DIR}/imgui_demo.cpp"
		"${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
		"${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp"
	)
	target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
	target_link_libraries(imgui PUBLIC "$<BUILD_INTERFACE:SDL3::SDL3>")
	# Upstream ImGui has no export macros, so make sure its symbols are visible when it is built as a
	# shared library. On Windows, CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS exports all regular functions.
	set_target_properties(imgui PROPERTIES
		CXX_VISIBILITY_PRESET      default
		VISIBILITY_INLINES_HIDDEN  OFF
		WINDOWS_EXPORT_ALL_SYMBOLS ON
	)
	add_library(imgui::imgui ALIAS imgui)
endif()

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
if (TARGET imgui AND NOT imgui_FOUND)
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
