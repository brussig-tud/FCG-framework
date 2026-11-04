cmake_minimum_required(VERSION 3.31)
execute_process(COMMAND "${CMAKE_COMMAND}" --fresh -S "${SOURCE}" -B "${BINARY}"
    "-DCMAKE_CXX_COMPILER=${COMPILER}" "-DMESH_INSTALL_PREFIX=${PREFIX}" "-DDEPENDENCIES=${DEPENDENCIES}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Mesh consumer configure failed:\n${output}\n${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BINARY}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Mesh consumer build failed:\n${output}\n${error}")
endif()
include("${BINARY}/consumer.cmake")
if(WIN32)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "PATH=${PREFIX}/bin;$ENV{PATH}" "${consumer}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
else()
    execute_process(COMMAND "${consumer}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
endif()
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Mesh consumer failed:\n${output}\n${error}")
endif()
