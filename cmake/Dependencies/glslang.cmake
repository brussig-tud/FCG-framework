
# glslang: GLSL front-end for build-time shader compilation (GLSL -> SPIR-V). Fetched, not system-first: SPIR-V is
# embedded into binaries, so the tool version must be locked. The tool is always built statically and without the debug
# postfix - it is used at build time only, and CMake's order-only dependency expansion for the tool would otherwise not
# match the postfixed output names. The block restores the surrounding configuration afterwards.
block (SCOPE_FOR VARIABLES PROPAGATE glslang_SOURCE_DIR glslang_BINARY_DIR)
	set(BUILD_SHARED_LIBS OFF)
	set(CMAKE_DEBUG_POSTFIX "")
	CPMAddPackage(
		NAME              glslang
		GITHUB_REPOSITORY KhronosGroup/glslang
		GIT_TAG           16.5.0
		GIT_SUBMODULES    ""  # SPIRV-Tools submodule unused (no optimizer)
		OPTIONS
			"ENABLE_OPT OFF"           # skip SPIRV-Tools entirely
			"ENABLE_HLSL OFF"          # GLSL-only pipeline
			"GLSLANG_TESTS OFF"
			"GLSLANG_ENABLE_INSTALL OFF"
			"BUILD_EXTERNAL OFF"
	)
endblock ()
