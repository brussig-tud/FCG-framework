# Build a disposable copy so dependency and removal tests never modify the repository's shader sources.
file(REMOVE_RECURSE "${TEST_BINARY}")
set(source "${TEST_BINARY}/source with spaces")
set(binary "${TEST_BINARY}/build with spaces")
file(MAKE_DIRECTORY "${source}")
file(COPY "${TEST_SOURCE}/" DESTINATION "${source}")

function (run)
	execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
	if (NOT result EQUAL 0)
		message(FATAL_ERROR "Command failed (${result}): ${ARGV}\n${output}\n${error}")
	endif ()
endfunction ()

set(configure "${CMAKE_COMMAND}" -S "${source}" -B "${binary}" -G "${TEST_GENERATOR}"
	"-DCMAKE_CXX_COMPILER=${TEST_COMPILER}" -DCMAKE_BUILD_TYPE=Debug
	"-DFCG_SHADER_CMAKE=${FCG_SHADER_CMAKE}" "-DEMBEDLIB_CMAKE=${EMBEDLIB_CMAKE}"
	"-DGLSLANG_EXECUTABLE=${GLSLANG_EXECUTABLE}"
)
set(build "${CMAKE_COMMAND}" --build "${binary}" --config Debug --parallel 4)
run(${configure})
run(${build})
file(READ "${binary}/probe-Debug.txt" probe)
run("${probe}")

set(blob "${binary}/first/shaders/logical/logical.comp.spv")
set(raw "${binary}/first/shaders/logical/logical.comp.glsl")
set(generated "${binary}/first/cpp-embedlib/first-resources/_data_logical_logical_comp_spv.cpp")

# An unchanged build must not regenerate arrays or relink the binary.
file(TIMESTAMP "${generated}" generated_before "%s.%f")
file(TIMESTAMP "${probe}" probe_before "%s.%f")
run(${build})
file(TIMESTAMP "${generated}" generated_after "%s.%f")
file(TIMESTAMP "${probe}" probe_after "%s.%f")
if (NOT generated_before STREQUAL generated_after OR NOT probe_before STREQUAL probe_after)
	message(FATAL_ERROR "No-op build regenerated resources or relinked the probe")
endif ()

# Changing an included file must recompile and re-embed the shader without a configure step.
file(SHA256 "${blob}" blob_before)
file(SHA256 "${generated}" generated_before)
file(WRITE "${source}/first/common.glsl" "const uint VALUE = 2;\n")
run(${build})
file(SHA256 "${blob}" blob_after)
file(SHA256 "${generated}" generated_after)
if (blob_before STREQUAL blob_after OR generated_before STREQUAL generated_after)
	message(FATAL_ERROR "GLSL include edit did not recompile and re-embed SPIR-V")
endif ()

# A source edit must update the diagnostic source stored beside the compiled blob.
set(source_array "${binary}/first/cpp-embedlib/first-resources/_data_logical_logical_comp_glsl.cpp")
file(SHA256 "${source_array}" source_before)
file(APPEND "${source}/first/work.comp.glsl" "\n// Updated source diagnostic text.\n")
run(${build})
file(SHA256 "${source}/first/work.comp.glsl" expected_source)
file(SHA256 "${raw}" actual_source)
file(SHA256 "${source_array}" source_after)
if (NOT expected_source STREQUAL actual_source OR source_before STREQUAL source_after)
	message(FATAL_ERROR "GLSL source edit did not update the embedded source")
endif ()

# Recover all staged artifacts after their entire output directory has been removed.
file(REMOVE_RECURSE "${binary}/first/shaders")
run(${build})
run("${probe}")

# Reconfigure without a registration while its old artifacts still exist on disk.
run(${configure} -DINCLUDE_EXTRA=OFF)
run(${build})
if (NOT EXISTS "${binary}/first/shaders/obsolete/obsolete.comp.spv")
	message(FATAL_ERROR "Stale-file test did not retain its old artifact")
endif ()
run("${probe}")

# All configuration errors should have an actionable diagnostic from the shader helper.
set(cases name missing suffix stage sources duplicate)
set(messages "NAME must contain" "source file does not exist" "unknown shader stage"
	"duplicate shader stage" "SOURCES is required" "duplicate shader name")
foreach (case expected IN ZIP_LISTS cases messages)
	execute_process(COMMAND ${configure} "-DFAILURE_CASE=${case}"
		RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
	if (result EQUAL 0 OR NOT "${output}\n${error}" MATCHES "${expected}")
		message(FATAL_ERROR "Missing expected configure failure for ${case}:\n${output}\n${error}")
	endif ()
endforeach ()

# Run a copied binary from an empty directory, with all fixture sources and staged blobs unavailable.
file(MAKE_DIRECTORY "${TEST_BINARY}/isolated")
file(COPY "${probe}" DESTINATION "${TEST_BINARY}/isolated")
get_filename_component(probe_name "${probe}" NAME)
file(REMOVE_RECURSE "${source}" "${binary}/first/shaders" "${binary}/second/shaders")
execute_process(COMMAND "${TEST_BINARY}/isolated/${probe_name}"
	WORKING_DIRECTORY "${TEST_BINARY}/isolated" RESULT_VARIABLE result)
if (NOT result EQUAL 0)
	message(FATAL_ERROR "Embedded resources depend on loose shader files")
endif ()
message(STATUS "Shader build graph, isolation, validation and incremental rebuilds passed")
