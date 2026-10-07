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
add_subdirectory("${PROJECT_SOURCE_DIR}/core/tests" "${PROJECT_BINARY_DIR}/core/tests")
add_subdirectory("${PROJECT_SOURCE_DIR}/libs/image/tests" "${PROJECT_BINARY_DIR}/libs/image/tests")
add_subdirectory("${PROJECT_SOURCE_DIR}/libs/render/tests" "${PROJECT_BINARY_DIR}/libs/render/tests")
include(dependencies/Tests.cmake)
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
	set(image_runtime "")
	if(FCG_SDL_IMAGE_BUILT_HERE AND FCG_USE_SHARED_SDL)
		set(image_runtime "$<TARGET_FILE_NAME:SDL3_image-shared>")
	endif()
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
			"-DIMAGE_RUNTIME=${image_runtime}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/verify-install.cmake"
	)
	set_tests_properties(install-smoke-verify PROPERTIES
		DEPENDS install-smoke
	)
endif()

# Documentation registration, independent of binary targets and optional Doxygen.
add_test(
    NAME documentation-smoke
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/documentation/fixture"
        "-DTEST_BINARY=${CMAKE_CURRENT_BINARY_DIR}/documentation"
        "-DFCG_DOCUMENTATION_CMAKE=${PROJECT_SOURCE_DIR}/cmake/Documentation.cmake"
        "-DFCG_DOXYFILE_TEMPLATE=${PROJECT_SOURCE_DIR}/docs/Doxyfile.in"
        "-DFCG_DEPENDENCIES_DIR=${PROJECT_SOURCE_DIR}/cmake/Dependencies"
        "-DTEST_DOXYGEN_FOUND=${DOXYGEN_FOUND}"
        "-DDOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/documentation/run.cmake"
)
set_tests_properties(documentation-smoke PROPERTIES TIMEOUT 180)
