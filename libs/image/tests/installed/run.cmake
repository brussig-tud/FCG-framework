cmake_minimum_required(VERSION 3.31)
execute_process(COMMAND "${CMAKE_COMMAND}" --fresh -S "${SOURCE}" -B "${BINARY}"
    "-DCMAKE_CXX_COMPILER=${COMPILER}" "-DPIXEL_INSTALL_PREFIX=${PREFIX}"
    "-DDEPENDENCIES=${DEPENDENCIES}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Image consumer configure failed:\n${output}\n${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BINARY}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Image consumer compile failed:\n${output}\n${error}")
endif()
