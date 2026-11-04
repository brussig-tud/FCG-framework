cmake_minimum_required(VERSION 3.31)
# All source-mode checks reuse resolved checkouts; no test-time downloads.
set(modes package provided local-missing)
if(EXISTS "${OPENMESH_SOURCE}/CMakeLists.txt" AND EXISTS "${TINYOBJ_SOURCE}/tiny_obj_loader.h" AND EXISTS "${TINYPLY_SOURCE}/source/tinyply.h")
    list(APPEND modes source)
endif()
foreach(mode IN LISTS modes)
    execute_process(COMMAND "${CMAKE_COMMAND}" --fresh -S "${SOURCE}" -B "${BINARY}/${mode}"
        "-DMODE=${mode}" "-DFCG_SOURCE=${FCG_SOURCE}" "-DCMAKE_CXX_COMPILER=${COMPILER}"
        "-DOPENMESH_SOURCE=${OPENMESH_SOURCE}" "-DTINYOBJ_SOURCE=${TINYOBJ_SOURCE}" "-DTINYPLY_SOURCE=${TINYPLY_SOURCE}"
        "-DDEPENDENCIES=${DEPENDENCIES}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(mode STREQUAL "local-missing")
        if(result EQUAL 0 OR NOT "${output}${error}" MATCHES "local package|installed|OpenMeshCore")
            message(FATAL_ERROR "Missing local-only dependency did not fail correctly:\n${output}\n${error}")
        endif()
    elseif(NOT result EQUAL 0)
        message(FATAL_ERROR "Mesh dependency ${mode} failed:\n${output}\n${error}")
    endif()
endforeach()
