
#ifndef __FCG_RENDER_DETAIL_MESH_DATA_H__
#define __FCG_RENDER_DETAIL_MESH_DATA_H__


//////
//
// Includes
//

// C++ STL
#include <array>
#include <optional>
#include <vector>

// Framework
#include <FCG/Mesh/mesh_processing.h>
#include <FCG/Render/export.h>
#include <FCG/Render/gpu_mesh_layout.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



/// Mesh conversion implementation details.
namespace detail {



//////
//
// Structs and enums
//

/// One source vertex; distinct entries are never welded.
struct GPUVertexData
{
	/// Source position in object coordinates.
	OpenMesh::Vec3d position;

	/// Authored vertex normal, when supplied.
	std::optional<OpenMesh::Vec3d> normal;

	/// Generated smooth normal, when requested.
	std::optional<OpenMesh::Vec3d> generated;
};

/// One triangle corner with resolved authored attributes.
struct GPUCornerData
{
	/// Index of the original vertex.
	std::size_t vertex;

	/// Authored corner, vertex, or face normal in priority order.
	std::optional<OpenMesh::Vec3d> normal;

	/// Authored corner, vertex, or face RGBA color.
	std::optional<std::array<float, 4>> color;

	/// Authored corner or vertex UV coordinates.
	std::optional<std::array<float, 2>> uv;
};

/// CPU conversion data, with consecutive triples defining triangle winding.
struct GPUMeshData {
	/// Original source vertices.
	std::vector<GPUVertexData> vertices;

	/// Triangulated face corners.
	std::vector<GPUCornerData> corners;
};

/// Contiguous vertex incidence used by directly invoked weighting callbacks.
struct MeshIncidence {
	/// Vertex ranges delimited by consecutive offsets.
	std::vector<std::size_t> offsets;

	/// Triangles rotated to put their target vertex first.
	std::vector<MeshIncidentTriangle> triangles;
};

/// Canonically packed GPU geometry.
struct PackedMesh
{
	/// Final channel selection.
	GPUMeshLayout layout;

	/// Interleaved floating-point components.
	std::vector<float> vertices;

	/// Triangle-list vertex indices.
	std::vector<std::uint32_t> indices;
};



//////
//
// Functions
//

/// Build vertex incidence without retaining references into the input.
/// \param data Triangulated conversion data.
/// \return Contiguous incident-triangle ranges.
[[nodiscard]] FCG_RENDER_EXPORT auto meshIncidence (const GPUMeshData &data) -> MeshIncidence;

/// Pack geometry and reuse indices within each original source vertex.
/// \param data Borrowed source geometry and attributes.
/// \param requested Explicit channel selection, or automatic inference.
/// \return Packed geometry; allocation and size-limit exceptions propagate.
[[nodiscard]] FCG_RENDER_EXPORT auto packMesh (const GPUMeshData &data, std::optional<GPUMeshLayout> requested)
	-> PackedMesh;

} // namespace detail



//////
//
// Namespaces close
//

// namespace fcg
}


#endif // __FCG_RENDER_DETAIL_MESH_DATA_H__
