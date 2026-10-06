
# ImGui provenance is owned by this startup step, including when the caller supplies a target.
set(FCG_IMGUI_BUILT_HERE OFF)
if(NOT TARGET imgui::imgui AND NOT TARGET imgui)
	# ImGui: immediate mode GUI for all graphical user interaction.
	CPMFindPackage(
		NAME              imgui
		GITHUB_REPOSITORY ocornut/imgui
		GIT_TAG           v1.92.9
		VERSION           1.92
		DOWNLOAD_ONLY     TRUE  # upstream ships no CMake build system
	)
endif()

if(NOT TARGET imgui::imgui AND NOT TARGET imgui)
	# Must be shared when the framework is shared: applets call ImGui global functions directly, and the framework owns
	# the global ImGui context (GImGui). Two static copies would give the applet a null context.
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
	set(FCG_IMGUI_BUILT_HERE ON)
endif()

if(NOT TARGET imgui::imgui AND TARGET imgui)
	add_library(imgui::imgui ALIAS imgui)
endif()
