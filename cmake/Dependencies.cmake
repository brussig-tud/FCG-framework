# FCG framework external dependencies.
#
# System packages with CMake package configs are preferred; anything not found
# is fetched from source via CPM (vendored as CPM.cmake, MIT). CPM cache/option
# settings only apply to source builds - a system-provided package is used as-is.
#
# Caveat: a system-provided package is used as-is (we don't control how the
# distro built it). Known mismatch scenarios: an old system version fails the
# VERSION gate below and we fall back to source; a feature-stripped system
# build could fail at runtime; a non-PIC static system SDL3 could fail to link
# into a shared framework. To force source builds, configure with
# -DCPM_USE_LOCAL_PACKAGES=OFF.

include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

# Mirror the framework's shared/static decision onto source-built dependencies.
# (Presets don't apply to embedded consumers, so this must stay in CMake code.)
if (FCG_USE_SHARED_SDL)
	set(SDL_SHARED ON)
	set(SDL_STATIC OFF)
else()
	set(SDL_SHARED OFF)
	set(SDL_STATIC ON)
endif()

# SDL3: window creation, input events, SDL GPU rendering API.
CPMFindPackage(
	NAME              SDL3
	GITHUB_REPOSITORY libsdl-org/SDL
	GIT_TAG           release-3.4.14
	VERSION           3.4.14
	OPTIONS
		# XTest is a niche runtime feature (X11 mouse warping, currently disabled
		# upstream) also used by SDL's own tests - off avoids the libxtst dependency
		"SDL_X11_XTEST OFF"
		# We don't need any screensaver functionality - off avoids this niche dependency
		"SDL_X11_XSCRNSAVER OFF"
)

# GLM: header-only math library (vector/matrix types, geometry utilities).
CPMFindPackage(
	NAME              glm
	GITHUB_REPOSITORY g-truc/glm
	GIT_TAG           1.0.1
	VERSION           1.0.1
)

# ImGui: immediate mode GUI for all graphical user interaction.
CPMFindPackage(
	NAME              imgui
	GITHUB_REPOSITORY ocornut/imgui
	GIT_TAG           v1.92.9
	VERSION           1.92
	DOWNLOAD_ONLY     TRUE  # upstream ships no CMake build system
)
if (NOT TARGET imgui)
	# Must be shared when the framework is shared: applets call ImGui global
	# functions directly, and the framework owns the global ImGui context
	# (GImGui). Two static copies would give the applet a null context.
	add_library(imgui
		"${imgui_SOURCE_DIR}/imgui.cpp"
		"${imgui_SOURCE_DIR}/imgui_draw.cpp"
		"${imgui_SOURCE_DIR}/imgui_tables.cpp"
		"${imgui_SOURCE_DIR}/imgui_widgets.cpp"
		"${imgui_SOURCE_DIR}/imgui_demo.cpp"
		"${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
		"${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp"
	)
	target_include_directories(imgui PUBLIC
		"${imgui_SOURCE_DIR}"
		"${imgui_SOURCE_DIR}/backends"
	)
	target_link_libraries(imgui PUBLIC "$<BUILD_INTERFACE:SDL3::SDL3>")
	# Upstream has no export macros; make all symbols visible in shared builds.
	set_target_properties(imgui PROPERTIES
		CXX_VISIBILITY_PRESET      default
		VISIBILITY_INLINES_HIDDEN  OFF
		WINDOWS_EXPORT_ALL_SYMBOLS ON
	)
	add_library(imgui::imgui ALIAS imgui)
	set(FCG_IMGUI_BUILT_HERE ON)  # used by install logic in the root CMakeLists
endif()

# glslang: GLSL front-end for build-time shader compilation (GLSL -> SPIR-V).
# Fetched, not system-first: SPIR-V is embedded into binaries, so the tool
# version must be locked. The tool is always built statically and without the
# debug postfix - it is used at build time only, and CMake's order-only
# dependency expansion for the tool would otherwise not match the postfixed
# output names. The block restores the surrounding configuration afterwards.
block (SCOPE_FOR VARIABLES)
	set(BUILD_SHARED_LIBS OFF)
	set(CMAKE_DEBUG_POSTFIX "")
	CPMAddPackage(
		NAME              glslang
		GITHUB_REPOSITORY KhronosGroup/glslang
		GIT_TAG           16.5.0
		GIT_SUBMODULES    ""  # SPIRV-Tools submodule unused (no optimizer)
		OPTIONS
			"ENABLE_OPT OFF"           # skip SPIRV-Tools entirely
			"ENABLE_HLSL OFF"          # GLSL-only pipeline
			"GLSLANG_TESTS OFF"
			"GLSLANG_ENABLE_INSTALL OFF"
			"BUILD_EXTERNAL OFF"
	)
endblock ()

# SDL_shadercross: runtime SPIR-V translation for the non-Vulkan SDL GPU
# backends (Metal via SPIRV-Cross, D3D12 via DXC). No upstream releases yet,
# hence the pinned commit. DXC off until the Windows strategy is settled.
CPMAddPackage(
	NAME              SDL_shadercross
	GITHUB_REPOSITORY libsdl-org/SDL_shadercross
	GIT_TAG           1ff05bec573988a98ef9e0260b4da44f512b8367  # main @ 2026-09-05
	OPTIONS
		"SDLSHADERCROSS_DXC OFF"           # SPIR-V passthrough + MSL unaffected
		"SDLSHADERCROSS_VENDORED ON"       # bundle SPIRV-Cross
		"SDLSHADERCROSS_SPIRVCROSS_SHARED OFF"
		"SDLSHADERCROSS_SHARED OFF"        # statically linked into Core
		"SDLSHADERCROSS_STATIC ON"
		"SDLSHADERCROSS_CLI OFF"
		"SDLSHADERCROSS_INSTALL OFF"
)
