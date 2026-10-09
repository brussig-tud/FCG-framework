# Compile the exact snippets included in the public component guides.
add_library(image-examples OBJECT image_examples.cpp)
target_link_libraries(image-examples PRIVATE ${FCG_PROJECT_NAME}::Image)

add_executable(image-smoke main.cpp)
target_link_libraries(image-smoke PRIVATE ${FCG_PROJECT_NAME}::Image)
add_test(NAME image-smoke COMMAND image-smoke "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" "${CMAKE_CURRENT_BINARY_DIR}/scratch")

# This consumer names no concrete handler: it proves automatic backend retention in static and shared builds.
add_executable(image-global-smoke global.cpp)
target_link_libraries(image-global-smoke PRIVATE ${FCG_PROJECT_NAME}::Image)
add_test(NAME image-global-smoke COMMAND image-global-smoke)

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
