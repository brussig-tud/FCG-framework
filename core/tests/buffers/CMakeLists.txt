# Compile the exact snippets included in the public Doxygen module.
add_library(buffer-examples OBJECT buffer_examples.cpp)
target_link_libraries(buffer-examples PRIVATE Core)

fcg_add_shader(buffer_compute SOURCES buffer.comp.glsl)
fcg_add_shader(buffer_graphics SOURCES buffer.vert.glsl buffer.frag.glsl)
fcg_finalize_shaders(TARGET buffer-resources NAMESPACE buffer_test::embedded)

add_executable(buffer-smoke main.cpp)
target_link_libraries(buffer-smoke PRIVATE Core buffer-resources SDL3_shadercross::SDL3_shadercross)
add_test(NAME buffer-smoke COMMAND buffer-smoke)
set_tests_properties(buffer-smoke PROPERTIES
	TIMEOUT 60
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-"
)

if (TARGET hello)
	add_executable(shape-buffer-smoke shape_smoke.cpp ${PROJECT_SOURCE_DIR}/app/hello/src/shapes.cpp)
	target_include_directories(shape-buffer-smoke PRIVATE ${PROJECT_SOURCE_DIR}/app/hello/src)
	target_link_libraries(shape-buffer-smoke PRIVATE Core)
	add_test(NAME shape-buffer-smoke COMMAND shape-buffer-smoke)
	set_tests_properties(shape-buffer-smoke PROPERTIES
		TIMEOUT 60
		FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-"
	)
endif()
