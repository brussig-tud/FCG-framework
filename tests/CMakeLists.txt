# ============================================================================
# Smoke tests for the FCG framework.
#
# Tier 0 tests verify the buildsystem's own contracts (installability), Tier 1
# tests exercise the framework's runtime lifecycle headlessly. Resource tests additionally verify the shader
# build pipeline and the contract between configure-time resource names and C++ lookup.
#
# Note: install-smoke installs from the *current* build tree, so CTest must run
# on a fully built tree (e.g. via `cmake --build` then `ctest`, or testPresets).
# ============================================================================

# Shader resources: exercise lookup without a GPU and validate the custom-command dependency graph independently.
add_subdirectory(resources)
add_subdirectory(buffers)
add_test(
	NAME shader-build-smoke
	COMMAND "${CMAKE_COMMAND}"
		"-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/shader-build/fixture"
		"-DTEST_BINARY=${CMAKE_CURRENT_BINARY_DIR}/shader-build"
		"-DFCG_SHADER_CMAKE=${PROJECT_SOURCE_DIR}/cmake/Shaders.cmake"
		"-DEMBEDLIB_CMAKE=${cpp-embedlib_SOURCE_DIR}/cmake/cpp-embedlib.cmake"
		"-DGLSLANG_EXECUTABLE=$<TARGET_FILE:glslang-standalone>"
		"-DTEST_GENERATOR=${CMAKE_GENERATOR}"
		"-DTEST_COMPILER=${CMAKE_CXX_COMPILER}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/shader-build/run.cmake"
)
set_tests_properties(shader-build-smoke PROPERTIES TIMEOUT 180)

# ---------------------------------------------------------------------------
# Tier 0 - installability: install the current build tree into a scratch
# prefix and verify the deliverables. Anchors every packaging change
# (components, destinations, export sets) with a test instead of a broken
# future consumer.
# ---------------------------------------------------------------------------
if (FCG_INCLUDE_PACKAGING)
	add_test(
		NAME install-smoke
		COMMAND "${CMAKE_COMMAND}"
			--install "${CMAKE_BINARY_DIR}"
			--prefix "${CMAKE_CURRENT_BINARY_DIR}/install-prefix"
	)

	add_test(
		NAME install-smoke-verify
		COMMAND "${CMAKE_COMMAND}"
			"-DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/install-prefix"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/verify-install.cmake"
	)
	set_tests_properties(install-smoke-verify PROPERTIES
		DEPENDS install-smoke
	)
endif()

# ---------------------------------------------------------------------------
# Tier 1 - app lifecycle: run a trivial probe applet through fcg::run()
# headlessly. The probe (tests/lifecycle/main.cpp) forces continuous redraws
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

# Documentation registration, independent of binary targets and optional Doxygen.
add_test(
    NAME documentation-smoke
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/documentation/fixture"
        "-DTEST_BINARY=${CMAKE_CURRENT_BINARY_DIR}/documentation"
        "-DFCG_DOCUMENTATION_CMAKE=${PROJECT_SOURCE_DIR}/cmake/Documentation.cmake"
        "-DFCG_DOXYFILE_TEMPLATE=${PROJECT_SOURCE_DIR}/docs/Doxyfile.in"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/documentation/run.cmake"
)
set_tests_properties(documentation-smoke PROPERTIES TIMEOUT 180)
