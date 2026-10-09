# Exercise the common startup with the already-resolved SDK and source checkouts. No test-time downloads.
if(FCG_SDL_IMAGE_BUILT_HERE AND SDL3_BINARY_DIR)
    get_target_property(image_dependency_source SDL3_image::SDL3_image SOURCE_DIR)
    set(dependency_sources)
    foreach(package glslang SDL_shadercross cpp-embedlib)
        list(APPEND dependency_sources "-DCPM_${package}_SOURCE=${${package}_SOURCE_DIR}")
    endforeach()
    if(FCG_LIBAVIF_BUILT_HERE)
        get_directory_property(avif_aom_source DIRECTORY "${libavif_SOURCE_DIR}" DEFINITION libaom_SOURCE_DIR)
        list(APPEND dependency_sources "-DTEST_LIBAVIF_SOURCE=${libavif_SOURCE_DIR}" "-DTEST_LIBAOM_SOURCE=${avif_aom_source}")
    endif()
    add_test(NAME dependencies-smoke COMMAND "${CMAKE_COMMAND}"
        "-DFCG_SOURCE=${PROJECT_SOURCE_DIR}"
        "-DTEST_SOURCE=${CMAKE_CURRENT_LIST_DIR}"
        "-DTEST_BINARY=${CMAKE_CURRENT_BINARY_DIR}/dependencies"
        "-DIMAGE_DEPENDENCY_SOURCE=${image_dependency_source}"
        "-DGLM_SOURCE=${glm_SOURCE_DIR}"
        "-DIMGUI_SOURCE=${imgui_SOURCE_DIR}"
        "-DIMGUI_LIBRARY=$<TARGET_FILE:imgui::imgui>"
        "-DSDL3_DIR=${SDL3_BINARY_DIR}"
        "-DTEST_BACKEND_SHARED=${FCG_USE_SHARED_SDL}"
        "-DTEST_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        "-DTEST_WEBP=${WebP_FOUND}"
        ${dependency_sources}
        -P "${CMAKE_CURRENT_LIST_DIR}/run.cmake"
    )
    set_tests_properties(dependencies-smoke PROPERTIES TIMEOUT 600)
endif()
