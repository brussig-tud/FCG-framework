fcg_add_shader(fullscreen_resources SOURCES resources.frag.glsl)
fcg_finalize_shaders(TARGET texture-test-resources NAMESPACE texture_test::embedded)
add_executable(texture-smoke main.cpp)
target_link_libraries(texture-smoke PRIVATE Core Image Render texture-test-resources)
add_test(NAME texture-smoke COMMAND texture-smoke)
set_tests_properties(texture-smoke PROPERTIES TIMEOUT 60
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")
add_library(texture-examples OBJECT texture_examples.cpp)
target_link_libraries(texture-examples PRIVATE Core)
