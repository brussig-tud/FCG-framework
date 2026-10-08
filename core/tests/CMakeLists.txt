# Core component tests. Configured after applications so optional app probes are available.
add_subdirectory(resources)
add_subdirectory(buffers)
add_subdirectory(textures)
add_subdirectory(cameras)

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
