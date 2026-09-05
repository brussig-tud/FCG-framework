# Shader compilation (GLSL -> SPIR-V via glslang) and embedding via cmrc.
# Logical shaders are registered with fcg_add_shader() and turned into the
# 'fcg-resources' library by fcg_finalize_shaders().

include(${CMAKE_CURRENT_LIST_DIR}/CMakeRC.cmake)

# Register a logical shader. SOURCES are GLSL files named <name>.<stage>.glsl
# with <stage> one of vert|frag|comp. Each is compiled to SPIR-V and embedded
# into the resource filesystem together with its raw source, at path
# "<name>/<source filename>".
function (fcg_add_shader NAME)
	cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
	if (NOT ARG_SOURCES)
		message(FATAL_ERROR "fcg_add_shader(${NAME}): SOURCES is required")
	endif ()

	set(out_root "${CMAKE_BINARY_DIR}/shaders/${NAME}")
	file(MAKE_DIRECTORY "${out_root}")

	foreach (src IN LISTS ARG_SOURCES)
		get_filename_component(src "${src}" ABSOLUTE)
		get_filename_component(fname "${src}" NAME)
		string(REGEX REPLACE "\\.glsl$" "" base "${fname}")
		if (NOT base MATCHES "\\.(vert|frag|comp)$")
			message(FATAL_ERROR "fcg_add_shader(${NAME}): unknown shader stage in '${fname}' (expected .vert/.frag/.comp.glsl)")
		endif ()

		set(spv "${out_root}/${base}.spv")
		add_custom_command(
			OUTPUT "${spv}"
			COMMAND "$<TARGET_FILE:glslang-standalone>" -V -o "${spv}" "${src}"
			DEPENDS "${src}" glslang-standalone
			VERBATIM
		)
		configure_file("${src}" "${out_root}/${fname}" COPYONLY)  # raw source next to the blob
		set_property(GLOBAL APPEND PROPERTY FCG_SHADER_RESOURCES
			"${spv}" "${out_root}/${fname}"
		)
	endforeach ()
endfunction ()

# Create the resource library from all shaders registered so far. Link it into
# whatever needs the embedded resources.
function (fcg_finalize_shaders)
	get_property(resources GLOBAL PROPERTY FCG_SHADER_RESOURCES)
	if (NOT resources)
		return()
	endif ()
	cmrc_add_resource_library(fcg-resources NAMESPACE res
		WHENCE "${CMAKE_BINARY_DIR}/shaders"
		${resources}
	)
endfunction ()
