# Compile the exact snippets included in the public component guides.
add_library(image-examples OBJECT image_examples.cpp)
target_link_libraries(image-examples PRIVATE FCG-framework::Image)

add_executable(image-smoke main.cpp)
target_link_libraries(image-smoke PRIVATE FCG-framework::Image)
add_test(NAME image-smoke COMMAND image-smoke "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" "${CMAKE_CURRENT_BINARY_DIR}/scratch")

# This consumer names no concrete handler: it proves automatic backend retention in static and shared builds.
add_executable(image-global-smoke global.cpp)
target_link_libraries(image-global-smoke PRIVATE FCG-framework::Image)
add_test(NAME image-global-smoke COMMAND image-global-smoke)

# A custom registrar lives in an independently compiled translation unit with no main() references to its symbols.
add_library(image-test-registration OBJECT registration.cpp)
target_link_libraries(image-test-registration PRIVATE FCG-framework::Image)
add_executable(image-registration-smoke registration_main.cpp)
target_link_libraries(image-registration-smoke PRIVATE FCG-framework::Image image-test-registration)
add_test(NAME image-registration-smoke COMMAND image-registration-smoke)

# Configuration fixtures reuse the already-fetched dependency source and SDL package; no network is needed in CTest.
if(FCG_SDL_IMAGE_BUILT_HERE AND SDL3_BINARY_DIR)
	find_package(WebP QUIET CONFIG)
	get_target_property(image_dependency_source SDL3_image::SDL3_image SOURCE_DIR)
	add_test(NAME image-dependencies-smoke COMMAND "${CMAKE_COMMAND}"
		"-DFCG_SOURCE=${PROJECT_SOURCE_DIR}"
		"-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dependencies"
		"-DTEST_BINARY=${CMAKE_CURRENT_BINARY_DIR}/dependencies"
		"-DIMAGE_DEPENDENCY_SOURCE=${image_dependency_source}"
		"-DSDL3_DIR=${SDL3_BINARY_DIR}"
		"-DTEST_C_COMPILER=${CMAKE_C_COMPILER}"
		"-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
		"-DTEST_GENERATOR=${CMAKE_GENERATOR}"
		"-DTEST_WEBP=${WebP_FOUND}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/run.cmake"
	)
	set_tests_properties(image-dependencies-smoke PROPERTIES TIMEOUT 180)
endif()
