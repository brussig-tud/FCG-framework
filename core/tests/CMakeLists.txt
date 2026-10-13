# Core component tests. Configured after applications so optional app probes are available.
add_subdirectory(resources)
add_subdirectory(buffers)
add_subdirectory(textures)
add_subdirectory(cameras)
add_subdirectory(guides)

# Portable callback ownership and compile contracts; no SDL or GPU initialization.
add_executable(presentation-callback-smoke lifecycle/presentation_callback.cpp)
target_link_libraries(presentation-callback-smoke PRIVATE Core)
add_test(NAME presentation-callback-smoke COMMAND presentation-callback-smoke)
set_tests_properties(presentation-callback-smoke PROPERTIES TIMEOUT 10)

# ---------------------------------------------------------------------------
# Tier 1 - app lifecycle: run a trivial probe applet through fcg::run()
# headlessly. The probe (core/tests/lifecycle/main.cpp) forces continuous redraws
# and requests shutdown after a fixed number of frames, so the test exercises
# SDL init, GPU device creation, ImGui backend, the frame loop and teardown
# end-to-end without any test hooks inside the framework.
# ---------------------------------------------------------------------------
add_executable(lifecycle-smoke lifecycle/main.cpp)
target_link_libraries(lifecycle-smoke PRIVATE Core)

add_test(
	NAME lifecycle-smoke
	COMMAND "${CMAKE_COMMAND}" -E env
		"$<TARGET_FILE:lifecycle-smoke>"
)
set_tests_properties(lifecycle-smoke PROPERTIES
	TIMEOUT 60
	# Validation diagnostics can be emitted even when the process exits successfully, including during teardown.
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-"
)

# Public camera lookup and handled/unhandled Escape behavior through the actual runtime loop.
add_executable(runtime-contract-smoke lifecycle/contracts.cpp)
target_link_libraries(runtime-contract-smoke PRIVATE Core)
add_test(NAME runtime-contract-smoke COMMAND runtime-contract-smoke)
set_tests_properties(runtime-contract-smoke PROPERTIES TIMEOUT 60
	FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")

# Real runtime presentation registration, failure handling and capture lifetimes.
add_executable(presentation-smoke lifecycle/presentation.cpp)
target_link_libraries(presentation-smoke PRIVATE Core)
foreach(scenario lifecycle mutation error throw init minimized)
	add_test(NAME presentation-${scenario}-smoke COMMAND presentation-smoke ${scenario})
	set_tests_properties(presentation-${scenario}-smoke PROPERTIES TIMEOUT 60
		FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-")
endforeach()
# Unsupported no-image acquisition is reported by the probe, not a skip code:
# CTest skip codes take precedence over validation-diagnostic failures.
