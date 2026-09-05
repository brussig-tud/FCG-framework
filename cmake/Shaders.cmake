# Shader compilation (GLSL -> SPIR-V via glslang) and embedding via cmrc.
# Logical shaders are registered with fcg_add_shader() and turned into a
# caller-selected resource library by fcg_finalize_shaders().

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

	set(out_root "${CMAKE_CURRENT_BINARY_DIR}/shaders/${NAME}")
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
		set_property(DIRECTORY APPEND PROPERTY FCG_SHADER_RESOURCES
			"${spv}" "${out_root}/${fname}"
		)
	endforeach ()
endfunction ()

# Create the resource library from shaders registered in this directory. The
# target and namespace are explicit so independent subdirectories can provide
# separate resource libraries.
function (fcg_finalize_shaders)
	cmake_parse_arguments(ARG "" "TARGET;NAMESPACE" "" ${ARGN})
	if (NOT ARG_TARGET OR NOT ARG_NAMESPACE)
		message(FATAL_ERROR "fcg_finalize_shaders(): TARGET and NAMESPACE are required")
	endif ()

	get_property(resources DIRECTORY PROPERTY FCG_SHADER_RESOURCES)
	if (NOT resources)
		return()
	endif ()
	cmrc_add_resource_library(${ARG_TARGET} NAMESPACE ${ARG_NAMESPACE}
		WHENCE "${CMAKE_CURRENT_BINARY_DIR}/shaders"
		${resources}
	)
endfunction ()
