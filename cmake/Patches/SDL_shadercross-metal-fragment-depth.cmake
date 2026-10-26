# Expose SPIRV-Cross's fragment-depth output switch through the pinned SDL_shadercross property API. Metal rejects
# depth outputs in pipelines without a depth attachment, even when depth testing and writing are disabled.
# Validate both files before changing either; repeated configuration accepts the exact patched contexts.
function (fcg_patch_shadercross_fragment_depth source_dir)
	set(header "${source_dir}/include/SDL3_shadercross/SDL_shadercross.h")
	set(source "${source_dir}/src/SDL_shadercross.c")
	if(NOT EXISTS "${header}" OR NOT EXISTS "${source}")
		return()
	endif()

	set(header_before [=[#define SDL_SHADERCROSS_PROP_SPIRV_MSL_VERSION_STRING "SDL_shadercross.spirv.msl.version"]=])
	set(header_after [=[#define SDL_SHADERCROSS_PROP_SPIRV_MSL_VERSION_STRING "SDL_shadercross.spirv.msl.version"
/* Whether MSL fragment shaders output depth; defaults to true. Disable for targets without a depth attachment. */
#define SDL_SHADERCROSS_PROP_SPIRV_MSL_ENABLE_FRAG_DEPTH_BUILTIN_BOOLEAN "SDL_shadercross.spirv.msl.enable_frag_depth_builtin"]=])
	set(source_before [=[        spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, mslVersion);]=])
	set(source_after [=[        spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, mslVersion);
        if (shaderStage == SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT) {
            result = spvc_compiler_options_set_bool(
                options,
                SPVC_COMPILER_OPTION_MSL_ENABLE_FRAG_DEPTH_BUILTIN,
                SDL_GetBooleanProperty(props, SDL_SHADERCROSS_PROP_SPIRV_MSL_ENABLE_FRAG_DEPTH_BUILTIN_BOOLEAN, true));
            if (result < 0) {
                SPVC_ERROR(spvc_compiler_options_set_bool);
                spvc_context_destroy(context);
                return NULL;
            }
        }]=])

	foreach(part header source)
		file(READ "${${part}}" ${part}_contents)
		string(FIND "${${part}_contents}" "${${part}_after}" patched)
		if(NOT patched EQUAL -1)
			set(${part}_changed OFF)
			continue()
		endif()
		string(FIND "${${part}_contents}" "${${part}_before}" original)
		string(FIND "${${part}_contents}" "SDL_SHADERCROSS_PROP_SPIRV_MSL_ENABLE_FRAG_DEPTH_BUILTIN_BOOLEAN" partial)
		if(original EQUAL -1 OR NOT partial EQUAL -1)
			message(FATAL_ERROR "SDL_shadercross fragment-depth patch: unexpected context in ${${part}}; update the patch for the selected dependency source")
		endif()
		string(REPLACE "${${part}_before}" "${${part}_after}" ${part}_contents "${${part}_contents}")
		set(${part}_changed ON)
	endforeach()

	foreach(part header source)
		if(${part}_changed)
			file(WRITE "${${part}}" "${${part}_contents}")
			message(STATUS "Applied SDL_shadercross Metal fragment-depth property to ${part}")
		endif()
	endforeach()
endfunction()

fcg_patch_shadercross_fragment_depth("${SDL_shadercross_SOURCE_DIR}")
