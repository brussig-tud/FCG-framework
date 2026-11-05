
# CPU polygon meshes and format-independent loading.
add_library(Mesh)
add_library(${FCG_PROJECT_NAME}::Mesh ALIAS Mesh)
set_target_properties(Mesh PROPERTIES VERSION ${FCG_FRAMEWORK_VERSION} SOVERSION ${FCG_FRAMEWORK_SOVERSION})
target_compile_features(Mesh PUBLIC ${FCG_CXX_STANDARD})
target_include_directories(Mesh PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
generate_export_header(Mesh BASE_NAME FCG_MESH EXPORT_FILE_NAME include/FCG/Mesh/export.h)
target_compile_definitions(Mesh PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:FCG_MESH_STATIC_DEFINE>")
# Match the framework's build-interface-only third-party packaging contract.
target_link_libraries(Mesh PUBLIC "$<BUILD_INTERFACE:fcg-openmesh>"
	PRIVATE "$<BUILD_INTERFACE:fcg-tinyobjloader>" "$<BUILD_INTERFACE:fcg-tinyply>")
target_sources(Mesh PRIVATE src/mesh_processing.cpp src/mesh_loader.cpp src/mesh_conversion.cpp src/obj.cpp src/ply.cpp src/stl.cpp)
