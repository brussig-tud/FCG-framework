# ============================================================================
# Smoke tests for the FCG framework.
#
# Tier 0 tests verify the buildsystem's own contracts (installability), Tier 1
# tests exercise the framework's runtime lifecycle headlessly. Both target the
# stable outer shell of the project (packaging, lifecycle), not the API
# surface, which is still in flux.
#
# Note: install-smoke installs from the *current* build tree, so CTest must run
# on a fully built tree (e.g. via `cmake --build` then `ctest`, or testPresets).
# ============================================================================

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
		"SDL_VIDEODRIVER=offscreen"
		"$<TARGET_FILE:lifecycle-smoke>"
)
set_tests_properties(lifecycle-smoke PROPERTIES
	TIMEOUT 60
)
