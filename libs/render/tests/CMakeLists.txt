add_library(render-examples OBJECT render_examples.cpp line_examples.cpp line_set_examples.cpp)
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

add_executable(render-lines lines.cpp)
target_link_libraries(render-lines PRIVATE Render)
add_test(NAME render-lines COMMAND render-lines)
set_tests_properties(render-lines PROPERTIES TIMEOUT 90
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

add_executable(render-line-sets line_sets.cpp)
target_link_libraries(render-line-sets PRIVATE Render)
add_test(NAME render-line-sets COMMAND render-line-sets)
set_tests_properties(render-line-sets PROPERTIES TIMEOUT 90
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

add_executable(render-line-set-failures line_set_failures.cpp)
target_compile_definitions(render-line-set-failures PRIVATE FCG_RENDER_STATIC_DEFINE)
target_link_libraries(render-line-set-failures PRIVATE Render)
add_test(NAME render-line-set-failures COMMAND render-line-set-failures)
set_tests_properties(render-line-set-failures PROPERTIES TIMEOUT 90
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

add_executable(render-connectivity-failures connectivity_failures.cpp)
target_compile_definitions(render-connectivity-failures PRIVATE FCG_RENDER_STATIC_DEFINE)
target_link_libraries(render-connectivity-failures PRIVATE Render)
add_test(NAME render-connectivity-failures COMMAND render-connectivity-failures)
set_tests_properties(render-connectivity-failures PROPERTIES TIMEOUT 60
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

if(FCG_INCLUDE_PACKAGING)
	file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/installed-dependencies.cmake"
		CONTENT "set(RENDER_CONSUMER_INCLUDE_DIRS \"$<TARGET_GENEX_EVAL:SDL3::SDL3,$<TARGET_PROPERTY:SDL3::SDL3,INTERFACE_INCLUDE_DIRECTORIES>>;$<TARGET_GENEX_EVAL:glm::glm,$<TARGET_PROPERTY:glm::glm,INTERFACE_INCLUDE_DIRECTORIES>>\")\n")
	add_test(NAME render-installed-line-sets COMMAND "${CMAKE_COMMAND}"
		"-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed"
		"-DBINARY=${CMAKE_CURRENT_BINARY_DIR}/installed-consumer"
		"-DCOMPILER=${CMAKE_CXX_COMPILER}"
		"-DPREFIX=${PROJECT_BINARY_DIR}/tests/install-prefix"
		"-DDEPENDENCIES=${CMAKE_CURRENT_BINARY_DIR}/installed-dependencies.cmake"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/installed/run.cmake")
	set_tests_properties(render-installed-line-sets PROPERTIES DEPENDS install-smoke-verify TIMEOUT 90)
endif()
