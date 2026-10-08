
# Embedded logical shaders. Each call compiles the listed stage files to SPIR-V and embeds them (plus their raw sources)
# under "<name>/" in the resource filesystem.
fcg_add_shader(triangle
	SOURCES triangle/triangle.vert.glsl triangle/triangle.frag.glsl
)

fcg_add_shader(fullscreen SOURCES fullscreen/fullscreen.vert.glsl)
fcg_add_shader(passthrough SOURCES fullscreen/passthrough.frag.glsl)
fcg_add_shader(srgb SOURCES fullscreen/srgb.frag.glsl)

# Custom-command outputs are directory-scoped; finalize here (same scope).
fcg_finalize_shaders(TARGET fcg-shaders NAMESPACE fcg::res::shaders)

# Generated and vendor includes are private archive build dependencies, never installed interfaces.
get_target_property(core_shader_includes fcg-shaders INTERFACE_INCLUDE_DIRECTORIES)
set_property(TARGET fcg-shaders PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
foreach(directory IN LISTS core_shader_includes)
	target_include_directories(fcg-shaders PUBLIC "$<BUILD_INTERFACE:${directory}>")
endforeach()
