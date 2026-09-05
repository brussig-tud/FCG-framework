# Embedded logical shaders. Each call compiles the listed stage files to
# SPIR-V and embeds them (plus their raw sources) under <name>/ in the
# resource filesystem.
fcg_add_shader(triangle
	SOURCES triangle/triangle.vert.glsl triangle/triangle.frag.glsl
)

# Custom-command outputs are directory-scoped; finalize here (same scope).
fcg_finalize_shaders()
