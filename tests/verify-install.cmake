# Post-install deliverable verification for the install-smoke test.
# Expects PREFIX - the installation prefix used.

set(missing "")

# Libraries (static with debug postfix, static release, or shared).
foreach(library Core Image Render Extras)
    if(NOT EXISTS "${PREFIX}/lib/lib${library}d.a"
        AND NOT EXISTS "${PREFIX}/lib/lib${library}.a"
        AND NOT EXISTS "${PREFIX}/lib/lib${library}d.so"
        AND NOT EXISTS "${PREFIX}/lib/lib${library}.so"
        AND NOT EXISTS "${PREFIX}/lib/lib${library}d.dylib"
        AND NOT EXISTS "${PREFIX}/lib/lib${library}.dylib"
        AND NOT EXISTS "${PREFIX}/lib/${library}d.lib"
        AND NOT EXISTS "${PREFIX}/lib/${library}.lib")
        list(APPEND missing "lib${library} (static or shared) in lib/")
    endif()
endforeach()

# The source-built shared backend must accompany the installed framework runtime.
foreach(runtime IMAGE_RUNTIME AVIF_RUNTIME)
    if(${runtime} AND NOT EXISTS "${PREFIX}/lib/${${runtime}}" AND NOT EXISTS "${PREFIX}/bin/${${runtime}}")
        list(APPEND missing "${${runtime}} runtime")
    endif()
endforeach()

# Public headers
foreach (header run.h window.h applet.h event.h export.h buffer.h texture.h fullscreen.h
    viewing.h camera_focus.h applet/orbit_camera.h applet/camera_2d.h
    Image/export.h Image/image.h Image/image_loader.h Image/sdl_image.h
    Extras/export.h Extras/file_dialog.h
    render_target.h Render/export.h Render/error.h Render/primitive_attributes.h
    Render/primitive_renderer.h Render/quad_renderer.h Render/box_renderer.h)
	if (NOT EXISTS "${PREFIX}/include/FCG/${header}")
		list(APPEND missing "include/FCG/${header}")
	endif()
endforeach()

# Fresh installs must expose Image only through its library-specific include directory.
foreach(header image_export.h image.h image_loader.h sdl_image.h)
    if(EXISTS "${PREFIX}/include/FCG/${header}")
        message(FATAL_ERROR "install-smoke: obsolete header include/FCG/${header}; use a clean installation prefix")
    endif()
endforeach()

# CMake package: config + version file, plus at least one target export set
foreach (file FCGConfig.cmake FCGConfigVersion.cmake)
	if (NOT EXISTS "${PREFIX}/lib/cmake/FCG/${file}")
		list(APPEND missing "lib/cmake/FCG/${file}")
	endif()
endforeach()
if (NOT EXISTS "${PREFIX}/lib/cmake/FCG/FCG-static-targets.cmake"
	AND NOT EXISTS "${PREFIX}/lib/cmake/FCG/FCG-shared-targets.cmake")
	list(APPEND missing "lib/cmake/FCG/FCG-<type>-targets.cmake (static or shared)")
endif()

# All public library targets must be exported.
file(GLOB target_files "${PREFIX}/lib/cmake/FCG/FCG-*-targets.cmake")
foreach(target_file IN LISTS target_files)
    file(READ "${target_file}" targets)
    foreach(library Core Image Render Extras)
        if(NOT targets MATCHES "add_library\\(FCG-Framework::${library}")
            list(APPEND missing "FCG-Framework::${library} in ${target_file}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE private_headers "${PREFIX}/include/*sdl_image_capabilities*" "${PREFIX}/include/*file_dialog_internal*")
if(private_headers)
    message(FATAL_ERROR "Private dependency/test headers were installed: ${private_headers}")
endif()

if (missing)
	message(FATAL_ERROR "install-smoke: missing deliverables:\n  ${missing}")
endif()

if(EXISTS "${PREFIX}/lib/cmake/FCG/FCG-static-targets.cmake")
    file(READ "${PREFIX}/lib/cmake/FCG/FCG-static-targets.cmake" static_targets)
    if(NOT static_targets MATCHES "add_library\\(FCG-Framework::fcg-render-shaders")
        message(FATAL_ERROR "Static Render shader archive is not exported")
    endif()
    if(NOT EXISTS "${PREFIX}/lib/libfcg-render-shadersd.a" AND NOT EXISTS "${PREFIX}/lib/libfcg-render-shaders.a"
        AND NOT EXISTS "${PREFIX}/lib/fcg-render-shadersd.lib" AND NOT EXISTS "${PREFIX}/lib/fcg-render-shaders.lib")
        message(FATAL_ERROR "Static Render shader archive is not installed")
    endif()
endif()

if(EXISTS "${PREFIX}/lib/cmake/FCG/FCG-static-targets.cmake")
    if(NOT static_targets MATCHES "add_library\\(FCG-Framework::fcg-shaders"
        OR (NOT EXISTS "${PREFIX}/lib/libfcg-shadersd.a" AND NOT EXISTS "${PREFIX}/lib/libfcg-shaders.a"
            AND NOT EXISTS "${PREFIX}/lib/fcg-shadersd.lib" AND NOT EXISTS "${PREFIX}/lib/fcg-shaders.lib"))
        message(FATAL_ERROR "Static Core shader archive is not installed and exported")
    endif()
endif()
if(EXISTS "${PREFIX}/bin/imgview" AND NOT EXISTS "${PREFIX}/bin/assets/cgvlogo.png")
    message(FATAL_ERROR "Image viewer executable-relative logo is not installed")
endif()

message(STATUS "install-smoke-verify: all expected deliverables present in ${PREFIX}")
