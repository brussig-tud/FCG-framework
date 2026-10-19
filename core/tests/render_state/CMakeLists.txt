# Matrix and cache contracts use the real device required by RenderState's constructor.
add_executable(render-state-smoke main.cpp)
target_link_libraries(render-state-smoke PRIVATE Core)
add_test(NAME render-state-smoke COMMAND render-state-smoke)
set_tests_properties(render-state-smoke PROPERTIES TIMEOUT 60
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")
