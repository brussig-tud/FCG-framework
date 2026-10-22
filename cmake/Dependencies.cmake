
# Framework dependency startup. Include at the framework root after setting build options,
# before adding any libraries. Package-specific policy belongs in Dependencies/<package>.cmake.
include_guard(DIRECTORY)
include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

# Explicit order: SDL3 precedes its consumers; optional tooling follows the build dependencies.
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/SDL3.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/SDL3_image.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/stb_image.cmake")
set(FCG_OPENEXR_BUILT_HERE OFF)
set(FCG_IMATH_BUILT_HERE OFF)
if(FCG_IMAGE_OPENEXR)
	include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/OpenEXR.cmake")
	if(NOT TARGET fcg-openexr)
		message(FATAL_ERROR "OpenEXR image support requires OpenEXR::OpenEXR")
	endif()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/glm.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/imgui.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/glslang.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/SDL_shadercross.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/cpp-embedlib.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/Doxygen.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Dependencies/WebP.cmake")

# Fail at the dependency boundary rather than later in a consumer's link or shader setup.
foreach(dependency SDL3::SDL3 SDL3_image::SDL3_image fcg-sdl-image fcg-stb-image glm::glm imgui::imgui
	glslang-standalone SDL3_shadercross::SDL3_shadercross)
	if(NOT TARGET ${dependency})
		message(FATAL_ERROR "FCG dependency resolution did not provide required target ${dependency}")
	endif()
endforeach()
foreach(dependency_command cpp_embedlib_add)
	if(NOT COMMAND ${dependency_command})
		message(FATAL_ERROR "FCG dependency resolution did not provide required command ${dependency_command}")
	endif()
endforeach()
