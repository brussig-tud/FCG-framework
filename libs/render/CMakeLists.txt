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
target_link_libraries(Render PUBLIC Core)
target_sources(Render PRIVATE
	src/primitive_attributes.cpp
	src/primitive_renderer.cpp
	src/quad_renderer.cpp
	src/box_renderer.cpp
	src/primitive_resources.cpp
	src/primitive_resources.h
)
target_sources(Render PRIVATE
	include/FCG/Render/error.h
	include/FCG/Render/primitive_attributes.h
	include/FCG/Render/primitive_renderer.h
	include/FCG/Render/quad_renderer.h
	include/FCG/Render/box_renderer.h
)

fcg_add_shader(primitive SOURCES shaders/primitive.vert.glsl shaders/primitive.frag.glsl)
fcg_add_shader(textured SOURCES shaders/primitive.vert.glsl shaders/textured.frag.glsl)
fcg_finalize_shaders(TARGET fcg-render-shaders NAMESPACE fcg::render::embedded)
target_link_libraries(Render PRIVATE fcg-render-shaders)
# Generated and vendor headers belong only to the build interface of the private archive.
get_target_property(render_shader_includes fcg-render-shaders INTERFACE_INCLUDE_DIRECTORIES)
set_property(TARGET fcg-render-shaders PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
foreach(directory IN LISTS render_shader_includes)
	target_include_directories(fcg-render-shaders PUBLIC "$<BUILD_INTERFACE:${directory}>")
endforeach()
