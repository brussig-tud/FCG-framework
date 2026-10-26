add_library(render-examples OBJECT render_examples.cpp)
target_link_libraries(render-examples PRIVATE Render)

add_executable(render-smoke main.cpp)
target_link_libraries(render-smoke PRIVATE Render fcg-render-shaders SDL3_shadercross::SDL3_shadercross)
add_test(NAME render-smoke COMMAND render-smoke)
set_tests_properties(render-smoke PROPERTIES TIMEOUT 90
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

# Exercise production shader dispatch with real MSL translation on all platforms, without a Metal device.
add_executable(render-shader-targets shader_targets.cpp)
target_compile_definitions(render-shader-targets PRIVATE FCG_FRAMEWORK_STATIC_DEFINE)
target_link_libraries(render-shader-targets PRIVATE Render fcg-render-shaders SDL3_shadercross::SDL3_shadercross)
add_test(NAME render-shader-targets COMMAND render-shader-targets)
set_tests_properties(render-shader-targets PROPERTIES TIMEOUT 90
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

add_executable(render-lifecycle lifecycle.cpp)
target_link_libraries(render-lifecycle PRIVATE Render)
add_test(NAME render-lifecycle COMMAND render-lifecycle)
set_tests_properties(render-lifecycle PROPERTIES TIMEOUT 60
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

# Set FCG_ELLIPSOID_BENCHMARK=1 when running render-ellipsoid to report optional 1024x1024 completion timings.
add_executable(render-ellipsoid ellipsoid.cpp)
target_link_libraries(render-ellipsoid PRIVATE Render)
add_test(NAME render-ellipsoid COMMAND render-ellipsoid)
set_tests_properties(render-ellipsoid PROPERTIES TIMEOUT 90
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")
