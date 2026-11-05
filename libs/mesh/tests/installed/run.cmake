cmake_minimum_required(VERSION 3.31)

# Configure with the generator and configuration used to build the installed libraries
execute_process(COMMAND "${CMAKE_COMMAND}" --fresh -G "${GENERATOR}" -S "${SOURCE}" -B "${BINARY}"
    "-DCMAKE_CXX_COMPILER=${COMPILER}" "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DMESH_INSTALL_PREFIX=${PREFIX}" "-DDEPENDENCIES=${DEPENDENCIES}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Mesh consumer configure failed:\n${output}\n${error}")
endif()

# Build the matching configuration for both single- and multi-configuration generators
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BINARY}" --config "${CONFIG}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Mesh consumer build failed:\n${output}\n${error}")
endif()

# Run the executable from the configuration that was built
include("${BINARY}/consumer-${CONFIG}.cmake")
if(WIN32)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "PATH=${PREFIX}/bin;$ENV{PATH}" "${consumer}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
else()
    execute_process(COMMAND "${consumer}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
endif()
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installed Mesh consumer failed:\n${output}\n${error}")
endif()
