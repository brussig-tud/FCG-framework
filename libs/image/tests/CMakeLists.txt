# Compile the exact snippets included in the public component guides.
add_library(image-examples OBJECT image_examples.cpp pixel_examples.cpp hdr_examples.cpp)
target_link_libraries(image-examples PRIVATE ${FCG_PROJECT_NAME}::Image)

add_executable(image-smoke main.cpp)
target_link_libraries(image-smoke PRIVATE ${FCG_PROJECT_NAME}::Image)
add_test(NAME image-smoke COMMAND image-smoke "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" "${CMAKE_CURRENT_BINARY_DIR}/scratch")

# This consumer names no concrete handler: it proves automatic backend retention in static and shared builds.
add_executable(image-global-smoke global.cpp)
target_link_libraries(image-global-smoke PRIVATE ${FCG_PROJECT_NAME}::Image)
add_test(NAME image-global-smoke COMMAND image-global-smoke "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/float.exr")

add_executable(image-hdr-smoke hdr.cpp application_stb.cpp)
set_source_files_properties(application_stb.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
target_link_libraries(image-hdr-smoke PRIVATE Image fcg-stb-image)
add_test(NAME image-hdr-smoke COMMAND image-hdr-smoke "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" "${CMAKE_CURRENT_BINARY_DIR}/hdr-scratch")
if(FCG_IMAGE_OPENEXR)
    add_executable(image-exr-smoke exr.cpp)
    target_link_libraries(image-exr-smoke PRIVATE Image fcg-openexr)
    add_test(NAME image-exr-smoke COMMAND image-exr-smoke "${CMAKE_CURRENT_BINARY_DIR}/exr-scratch")

    # Local third-party images are optional; an incomplete set is reported as skipped at runtime.
    add_test(NAME image-exr-assets COMMAND image-exr-smoke
        "${CMAKE_CURRENT_BINARY_DIR}/exr-scratch" "${PROJECT_SOURCE_DIR}/assets")
    set_tests_properties(image-exr-assets PROPERTIES SKIP_RETURN_CODE 77)
    set_tests_properties(image-exr-smoke image-exr-assets PROPERTIES FAIL_REGULAR_EXPRESSION "EXR_ERR_")
endif()

# A custom registrar lives in an independently compiled translation unit with no main() references to its symbols.
add_library(image-test-registration OBJECT registration.cpp)
target_link_libraries(image-test-registration PRIVATE ${FCG_PROJECT_NAME}::Image)
add_executable(image-registration-smoke registration_main.cpp)
target_link_libraries(image-registration-smoke PRIVATE ${FCG_PROJECT_NAME}::Image image-test-registration)
add_test(NAME image-registration-smoke COMMAND image-registration-smoke)

add_executable(image-metadata-smoke metadata.cpp)
target_link_libraries(image-metadata-smoke PRIVATE Image Extras)
target_include_directories(image-metadata-smoke PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../private" "${PROJECT_SOURCE_DIR}/app/imgview/src")
add_test(NAME image-metadata-smoke COMMAND image-metadata-smoke)

add_executable(image-pixels-smoke pixels.cpp)
target_link_libraries(image-pixels-smoke PRIVATE ${FCG_PROJECT_NAME}::Image)
add_test(NAME image-pixels-smoke COMMAND image-pixels-smoke)

option(FCG_IMAGE_PIXEL_BENCHMARKS "Build the opt-in Image native row benchmark (use Release)" OFF)
if(FCG_IMAGE_PIXEL_BENCHMARKS)
    add_executable(image-pixel-benchmark pixel_benchmark.cpp)
    target_link_libraries(image-pixel-benchmark PRIVATE ${FCG_PROJECT_NAME}::Image)
endif()

if(FCG_INCLUDE_PACKAGING)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/installed-dependencies.cmake"
        CONTENT "set(PIXEL_CONSUMER_INCLUDE_DIRS \"$<TARGET_GENEX_EVAL:SDL3::SDL3,$<TARGET_PROPERTY:SDL3::SDL3,INTERFACE_INCLUDE_DIRECTORIES>>;$<TARGET_GENEX_EVAL:glm::glm,$<TARGET_PROPERTY:glm::glm,INTERFACE_INCLUDE_DIRECTORIES>>\")\n")
    add_test(NAME image-installed-headers COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed"
        "-DBINARY=${CMAKE_CURRENT_BINARY_DIR}/installed-consumer"
        "-DCOMPILER=${CMAKE_CXX_COMPILER}"
        "-DPREFIX=${PROJECT_BINARY_DIR}/tests/install-prefix"
        "-DDEPENDENCIES=${CMAKE_CURRENT_BINARY_DIR}/installed-dependencies.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/installed/run.cmake")
    set_tests_properties(image-installed-headers PROPERTIES DEPENDS install-smoke-verify)
endif()
