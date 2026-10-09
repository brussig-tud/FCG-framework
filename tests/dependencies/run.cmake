cmake_minimum_required(VERSION 3.31)

function(run)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed: ${ARGV}\n${out}\n${err}")
    endif()
endfunction()

set(common "-DFCG_SOURCE=${FCG_SOURCE}" "-DIMAGE_DEPENDENCY_SOURCE=${IMAGE_DEPENDENCY_SOURCE}"
    "-DSDL3_DIR=${SDL3_DIR}" "-DCMAKE_C_COMPILER=${TEST_C_COMPILER}" "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}"
    "-DTEST_BACKEND_SHARED=${TEST_BACKEND_SHARED}" "-DCMAKE_BUILD_TYPE=Debug"
    "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    "-DGLM_SOURCE=${GLM_SOURCE}" "-DIMGUI_SOURCE=${IMGUI_SOURCE}" "-DIMGUI_LIBRARY=${IMGUI_LIBRARY}")
foreach(package glslang SDL_shadercross cpp-embedlib)
    list(APPEND common "-DCPM_${package}_SOURCE=${CPM_${package}_SOURCE}")
endforeach()
foreach(mode provided provided-namespaced provided-imported provided-empty package package-visible png-normal png-cache disabled unresolved)
    run("${CMAKE_COMMAND}" -S "${TEST_SOURCE}" -B "${TEST_BINARY}/${mode}" -G "${TEST_GENERATOR}"
        ${common} "-DMODE=${mode}" -DFRAMEWORK_SHARED=OFF)
endforeach()
foreach(mode provided-missing provided-invalid)
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${TEST_SOURCE}" -B "${TEST_BINARY}/${mode}" -G "${TEST_GENERATOR}"
        ${common} "-DMODE=${mode}" -DFRAMEWORK_SHARED=OFF RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(result EQUAL 0 OR NOT "${out}\n${err}" MATCHES "FCG_SDL_IMAGE_FORMATS")
        message(FATAL_ERROR "Expected capability-declaration failure for ${mode}: ${result}\n${out}\n${err}")
    endif()
endforeach()
# Each parent SDK tests both framework linkages. Static and shared parent builds cover all four combinations.
foreach(shared OFF ON)
    set(binary "${TEST_BINARY}/minimal-${shared}")
    run("${CMAKE_COMMAND}" -S "${TEST_SOURCE}" -B "${binary}" -G "${TEST_GENERATOR}"
        ${common} -DMODE=minimal "-DFRAMEWORK_SHARED=${shared}")
    run("${CMAKE_COMMAND}" --build "${binary}" --target backend-consumer Extras --config Debug -j 2)
    include("${binary}/consumer-Debug.cmake")
    run("${consumer}")
    run("${CMAKE_COMMAND}" --install "${binary}" --config Debug --prefix "${binary}/prefix")
    file(GLOB exports "${binary}/prefix/lib/cmake/Fixture/*.cmake")
    foreach(export IN LISTS exports)
        file(READ "${export}" contents)
        if(contents MATCHES "fcg-sdl-image|SDL3_image::|FCG_SDL_IMAGE_BUILT_HERE")
            message(FATAL_ERROR "Dependency implementation details leaked into ${export}")
        endif()
    endforeach()
endforeach()
# Optional external-codec checks require an installed development package; no test-time fetches.
if(TEST_WEBP)
    foreach(mode normal cache)
        run("${CMAKE_COMMAND}" -S "${TEST_SOURCE}" -B "${TEST_BINARY}/${mode}" -G "${TEST_GENERATOR}"
            ${common} "-DMODE=${mode}" -DFRAMEWORK_SHARED=OFF)
        run("${CMAKE_COMMAND}" --build "${TEST_BINARY}/${mode}" --target optional-codec --config Debug -j 2)
        include("${TEST_BINARY}/${mode}/consumer-Debug.cmake")
        run("${consumer}" "${FCG_SOURCE}/libs/image/tests/fixtures/colors.webp")
    endforeach()
else()
    message(STATUS "WebP development package absent; external-codec checks skipped")
endif()
message(STATUS "Common dependency startup, caller overrides, target reuse, linkage and export checks passed")
