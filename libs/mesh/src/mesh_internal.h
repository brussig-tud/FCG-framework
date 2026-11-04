
#ifndef __FCG_MESH_INTERNAL_H__
#define __FCG_MESH_INTERNAL_H__


//////
//
// Includes
//

// C++ STL
#include <optional>

// Framework
#include <FCG/Mesh/mesh_loader.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Private mesh decoding and construction helpers.
namespace detail {



//////
//
// Structs and enums
//

/// One source vertex and its optional attributes.
struct MeshVertex
{
	/// Source position.
	Mesh::Point point;

	/// Supplied vertex normal.
	std::optional<Mesh::Normal> normal;

	/// Supplied vertex texture coordinate.
	std::optional<Mesh::TexCoord2D> uv;

	/// Supplied vertex color.
	std::optional<Mesh::Color> color;
};

/// One polygon corner referencing a source vertex.
struct MeshCorner
{
	/// Index in the source vertex array.
	std::size_t vertex;

	/// Supplied corner normal, overriding a vertex normal at this corner.
	std::optional<Mesh::Normal> normal;

	/// Supplied corner texture coordinate, overriding vertex UVs at this corner.
	std::optional<Mesh::TexCoord2D> uv;
};

/// One source polygon and its face attributes.
struct MeshPolygon
{
	/// Ordered polygon corners.
	std::vector<MeshCorner> corners;

	/// Supplied face normal.
	std::optional<Mesh::Normal> normal;

	/// Supplied face color.
	std::optional<Mesh::Color> color;
};

/// Parsed geometry independent of OpenMesh insertion order and repair.
struct MeshSource {
	/// Source vertices, including isolated vertices.
	std::vector<MeshVertex> vertices;

	/// Source faces in file order.
	std::vector<MeshPolygon> polygons;
};



//////
//
// Functions
//

/// Construct an owned mesh after validating the complete source and its attribute values.
[[nodiscard]] auto constructMesh (const MeshSource &source, MeshLoaderOptions options) -> std::expected<Mesh, MeshError>;

/// Normalize an extension, ignoring a leading dot and ASCII case.
[[nodiscard]] auto meshHint (std::string_view hint) -> std::string;

/// Return the encoded bytes as a borrowed character view.
[[nodiscard]] inline auto meshText (std::span<const std::byte> bytes) -> std::string_view {
	return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}



//////
//
// Namespaces close
//

// namespace detail
}

// namespace fcg
}


#endif // __FCG_MESH_INTERNAL_H__
