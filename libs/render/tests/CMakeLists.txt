add_library(render-examples OBJECT render_examples.cpp)
target_link_libraries(render-examples PRIVATE Render)

add_executable(render-smoke main.cpp)
target_link_libraries(render-smoke PRIVATE Render fcg-render-shaders SDL3_shadercross::SDL3_shadercross)
add_test(NAME render-smoke COMMAND render-smoke)
set_tests_properties(render-smoke PROPERTIES TIMEOUT 90
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

add_executable(render-lifecycle lifecycle.cpp)
target_link_libraries(render-lifecycle PRIVATE Render)
add_test(NAME render-lifecycle COMMAND render-lifecycle)
set_tests_properties(render-lifecycle PROPERTIES TIMEOUT 60
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")
