cmake_minimum_required(VERSION 3.31)

function(run)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed: ${ARGV}\n${out}\n${err}")
    endif()
endfunction()

set(common "-DFCG_SOURCE=${FCG_SOURCE}" "-DIMAGE_DEPENDENCY_SOURCE=${IMAGE_DEPENDENCY_SOURCE}"
    "-DSDL3_DIR=${SDL3_DIR}" "-DCMAKE_C_COMPILER=${TEST_C_COMPILER}" "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}")
foreach(mode provided minimal)
    run("${CMAKE_COMMAND}" -S "${TEST_SOURCE}" -B "${TEST_BINARY}/${mode}" -G "${TEST_GENERATOR}"
        ${common} "-DMODE=${mode}")
endforeach()
# Optional external-codec checks are conditional on an installed development package. No test-time network fetches.
if(TEST_WEBP)
    foreach(mode normal cache)
        run("${CMAKE_COMMAND}" -S "${TEST_SOURCE}" -B "${TEST_BINARY}/${mode}" -G "${TEST_GENERATOR}"
            ${common} "-DMODE=${mode}")
        run("${CMAKE_COMMAND}" --build "${TEST_BINARY}/${mode}" --target optional-codec -j 2)
        run("${TEST_BINARY}/${mode}/optional-codec" "${TEST_SOURCE}/../fixtures/colors.webp")
    endforeach()
else()
    message(STATUS "WebP development package absent; optional-codec checks skipped")
endif()
message(STATUS "Image dependency defaults, overrides, and provided-target checks passed")
