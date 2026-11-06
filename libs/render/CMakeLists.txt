# Instanced filled primitives and shared GPU attributes.
add_library(Render)
add_library(${FCG_PROJECT_NAME}::Render ALIAS Render)
set_target_properties(Render PROPERTIES VERSION ${FCG_FRAMEWORK_VERSION} SOVERSION ${FCG_FRAMEWORK_SOVERSION})
target_compile_features(Render PUBLIC ${FCG_CXX_STANDARD})
target_include_directories(Render PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)
generate_export_header(Render BASE_NAME FCG_RENDER EXPORT_FILE_NAME include/FCG/Render/export.h)
target_compile_definitions(Render PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:FCG_RENDER_STATIC_DEFINE>")
target_link_libraries(Render PUBLIC Core Mesh)
target_sources(Render PRIVATE
	src/gpu_mesh.cpp
	src/mesh_renderer.cpp
	src/primitive_attributes.cpp
	src/primitive_renderer.cpp
	src/primitive_connectivity.cpp
	src/connected_primitive_renderer.cpp
	src/line_renderer.cpp
	src/wire_box_renderer.cpp
	src/line_set_builder.cpp
	src/quad_renderer.cpp
	src/box_renderer.cpp
	src/ellipsoid_renderer.cpp
	src/primitive_resources.cpp
	src/primitive_resources.h
)
target_sources(Render PRIVATE
	include/FCG/Render/error.h
	include/FCG/Render/primitive_attributes.h
	include/FCG/Render/primitive_renderer.h
	include/FCG/Render/primitive_connectivity.h
	include/FCG/Render/connected_primitive_renderer.h
	include/FCG/Render/line_renderer.h
	include/FCG/Render/line_style.h
	include/FCG/Render/wire_box_renderer.h
	include/FCG/Render/line_set_builder.h
	include/FCG/Render/quad_renderer.h
	include/FCG/Render/box_renderer.h
	include/FCG/Render/ellipsoid_renderer.h
)

fcg_add_shader(primitive SOURCES shaders/primitive.vert.glsl shaders/primitive.frag.glsl)
fcg_add_shader(textured SOURCES shaders/primitive.vert.glsl shaders/textured.frag.glsl)
fcg_add_shader(ellipsoid SOURCES shaders/ellipsoid.vert.glsl shaders/ellipsoid.frag.glsl)
fcg_add_shader(ellipsoid_textured SOURCES shaders/ellipsoid.vert.glsl shaders/ellipsoid_textured.frag.glsl)
fcg_add_shader(line SOURCES shaders/line.vert.glsl shaders/line.frag.glsl)
fcg_add_shader(wire_box SOURCES shaders/wire_box.vert.glsl shaders/line.frag.glsl)
# Configure concrete vertex/fragment interfaces for each canonical layout.
configure_file(shaders/blinn_phong.glsl "${CMAKE_CURRENT_BINARY_DIR}/mesh-shaders/blinn_phong.glsl" COPYONLY)
foreach(mesh_key RANGE 0 7)
	math(EXPR MESH_NORMALS "${mesh_key} & 1")
	math(EXPR MESH_COLORS "(${mesh_key} >> 1) & 1")
	math(EXPR MESH_UVS "(${mesh_key} >> 2) & 1")
	foreach(MESH_TEXTURED RANGE 0 ${MESH_UVS})
		set(mesh_name "mesh_${mesh_key}")
		if(MESH_TEXTURED)
			string(APPEND mesh_name "_textured")
		endif()
		set(mesh_generated "${CMAKE_CURRENT_BINARY_DIR}/mesh-shaders/${mesh_name}")
		configure_file(shaders/mesh.vert.glsl.in "${mesh_generated}.vert.glsl" @ONLY)
		configure_file(shaders/mesh.frag.glsl.in "${mesh_generated}.frag.glsl" @ONLY)
		fcg_add_shader(${mesh_name} SOURCES "${mesh_generated}.vert.glsl" "${mesh_generated}.frag.glsl")
	endforeach()
endforeach()
fcg_finalize_shaders(TARGET fcg-render-shaders NAMESPACE fcg::render::embedded)
target_link_libraries(Render PRIVATE fcg-render-shaders)
# Generated and vendor headers belong only to the build interface of the private archive.
get_target_property(render_shader_includes fcg-render-shaders INTERFACE_INCLUDE_DIRECTORIES)
set_property(TARGET fcg-render-shaders PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
foreach(directory IN LISTS render_shader_includes)
	target_include_directories(fcg-render-shaders PUBLIC "$<BUILD_INTERFACE:${directory}>")
endforeach()
