
# Embedded logical shaders. Each call compiles the listed stage files to SPIR-V and embeds them (plus their raw sources)
# under "<name>/" in the resource filesystem.
fcg_add_shader(simple_shapes
	SOURCES
		simple_shapes/simple_shapes.vert.glsl
		simple_shapes/simple_shapes.frag.glsl
)

# Custom-command outputs are directory-scoped; finalize here (same scope).
fcg_finalize_shaders(TARGET ${APP_NAME}-shaders NAMESPACE res::shaders)

# Inject dependency
target_link_libraries(${APP_NAME} PRIVATE ${APP_NAME}-shaders)
