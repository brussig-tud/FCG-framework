# Shader compilation (GLSL -> SPIR-V via glslang) and embedding via cpp-embedlib. Logical shaders are registered with
# fcg_add_shader() and turned into a caller-selected resource library by fcg_finalize_shaders().

include_guard(GLOBAL)

# Register a logical shader. SOURCES end in .<stage>.glsl with <stage> one of vert|frag|comp. Each is compiled to SPIR-V
# and embedded together with its raw source at <name>/<name>.<stage>.{spv,glsl}, independently of its source basename.
# Names contain only ASCII letters, digits, underscores and hyphens. Paths are fixed at configure time; file contents
# are produced at build time and are identical across build configurations.
function (fcg_add_shader NAME)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES")
	if (NOT NAME MATCHES "^[A-Za-z0-9_-]+$")
		message(FATAL_ERROR "fcg_add_shader(): NAME must contain only letters, digits, underscores and hyphens")
	endif ()
	if (ARG_UNPARSED_ARGUMENTS OR ARG_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "fcg_add_shader(${NAME}): invalid arguments")
	endif ()
	if (NOT ARG_SOURCES)
		message(FATAL_ERROR "fcg_add_shader(${NAME}): SOURCES is required")
	endif ()
	get_property(finalized DIRECTORY PROPERTY FCG_SHADERS_FINALIZED)
	if (finalized)
		message(FATAL_ERROR "fcg_add_shader(${NAME}): this directory's shaders have already been finalized")
	endif ()
	get_property(names DIRECTORY PROPERTY FCG_SHADER_NAMES)
	if (NAME IN_LIST names)
		message(FATAL_ERROR "fcg_add_shader(${NAME}): duplicate shader name in this directory")
	endif ()
	set_property(DIRECTORY APPEND PROPERTY FCG_SHADER_NAMES "${NAME}")

	set(out_root "${CMAKE_CURRENT_BINARY_DIR}/shaders/${NAME}")
	set(stages "")

	foreach (src IN LISTS ARG_SOURCES)
		get_filename_component(src "${src}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
		if (NOT EXISTS "${src}" OR IS_DIRECTORY "${src}")
			message(FATAL_ERROR "fcg_add_shader(${NAME}): source file does not exist: '${src}'")
		endif ()
		get_filename_component(fname "${src}" NAME)
		if (NOT fname MATCHES "\\.(vert|frag|comp)\\.glsl$")
			message(FATAL_ERROR "fcg_add_shader(${NAME}): unknown shader stage in '${fname}' (expected .vert/.frag/.comp.glsl)")
		endif ()
		set(stage "${CMAKE_MATCH_1}")
		if (stage IN_LIST stages)
			message(FATAL_ERROR "fcg_add_shader(${NAME}): duplicate shader stage '${stage}'")
		endif ()
		list(APPEND stages "${stage}")

		set(spv "${out_root}/${NAME}.${stage}.spv")
		set(source "${out_root}/${NAME}.${stage}.glsl")
		set(depfile "${spv}.d")
		add_custom_command(
			OUTPUT "${spv}"
			BYPRODUCTS "${depfile}"
			COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_root}"
			COMMAND "$<TARGET_FILE:glslang-standalone>" -V --depfile "${depfile}" -o "${spv}" "${src}"
			DEPENDS "${src}" glslang-standalone
			DEPFILE "${depfile}"
			VERBATIM
		)
		add_custom_command(
			OUTPUT "${source}"
			COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_root}"
			COMMAND "${CMAKE_COMMAND}" -E copy "${src}" "${source}"
			DEPENDS "${src}"
			VERBATIM
		)
		set_property(DIRECTORY APPEND PROPERTY FCG_SHADER_RESOURCES
			"${spv}" "${source}"
		)
	endforeach ()
endfunction ()

# Create the resource library from shaders registered in this directory. The target and namespace are explicit so
# independent subdirectories can provide separate resource libraries.
function (fcg_finalize_shaders)
	cmake_parse_arguments(PARSE_ARGV 0 ARG "" "TARGET;NAMESPACE" "")
	if (ARG_UNPARSED_ARGUMENTS OR ARG_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "fcg_finalize_shaders(): invalid arguments")
	endif ()
	if (NOT ARG_TARGET OR NOT ARG_NAMESPACE)
		message(FATAL_ERROR "fcg_finalize_shaders(): TARGET and NAMESPACE are required")
	endif ()

	get_property(resources DIRECTORY PROPERTY FCG_SHADER_RESOURCES)
	if (NOT resources)
		return()
	endif ()
	get_property(finalized DIRECTORY PROPERTY FCG_SHADERS_FINALIZED)
	if (finalized)
		message(FATAL_ERROR "fcg_finalize_shaders(): this directory's shaders have already been finalized")
	endif ()
	set_property(DIRECTORY PROPERTY FCG_SHADERS_FINALIZED TRUE)
	# Explicit FILES include future build outputs and exclude stale files left by removed registrations.
	cpp_embedlib_add(${ARG_TARGET}
		NAMESPACE "${ARG_NAMESPACE}"
		BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders"
		FILES ${resources}
	)
endfunction ()
