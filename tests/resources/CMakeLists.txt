# Reuse the framework shader name in an independent collection, with a different source basename and stage.
fcg_add_shader(triangle SOURCES work.comp.glsl)
fcg_finalize_shaders(TARGET test-resources NAMESPACE probe::embedded)

# A collection without source text exercises the optional diagnostic-source contract.
cpp_embedlib_add(test-bytecode-only
	NAMESPACE probe::bytecode
	BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders"
	FILES "${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle/triangle.comp.spv"
)
# Both collections consume the same generated blob; serialize its producer for Makefile generators too.
add_dependencies(test-bytecode-only test-resources)

add_executable(resource-smoke main.cpp)
target_link_libraries(resource-smoke PRIVATE Core test-resources test-bytecode-only)
set(resource_roots "${PROJECT_BINARY_DIR}/shaders/shaders" "${CMAKE_CURRENT_BINARY_DIR}/shaders")
if (TARGET hello-resources)
	target_link_libraries(resource-smoke PRIVATE hello-resources)
	target_compile_definitions(resource-smoke PRIVATE FCG_TEST_HELLO)
	list(APPEND resource_roots "${PROJECT_BINARY_DIR}/app/hello/shader/shaders")
endif ()
add_test(NAME resource-smoke COMMAND resource-smoke ${resource_roots})

# No filesystem comparison arguments: lookup must work using only the loaded binaries.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/empty")
add_test(NAME resource-smoke-embedded-only COMMAND resource-smoke)
set_tests_properties(resource-smoke-embedded-only PROPERTIES
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/empty"
)
