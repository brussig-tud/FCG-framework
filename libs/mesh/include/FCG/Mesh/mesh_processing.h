
#ifndef __FCG_MESH_MESH_PROCESSING_H__
#define __FCG_MESH_MESH_PROCESSING_H__


//////
//
// Module documentation
//

/// \defgroup fcg_mesh_processing Mesh processing
/// \ingroup fcg_modules
/// \brief Triangulating polygons and computing smooth vertex normals.
///
/// Use <tt>\ref fcg::triangulateMesh "triangulateMesh"</tt> and
/// <tt>\ref fcg::computeVertexNormals "computeVertexNormals"</tt> with mutable OpenMesh meshes.
/// Coordinates and winding retain the source conventions.
///
/// \section fcg_mesh_processing_usage Getting started
/// Triangulate before computing normals:
/// \snippet mesh_processing_examples.cpp processing
///
/// \section fcg_mesh_processing_workflows Common tasks
/// Choose fan triangulation for convex polygons, or projected ear cutting for concave polygons.
/// Select equal, area or corner-angle weights for smooth normals. Corner-angle weighting is the default.
/// \snippet mesh_processing_examples.cpp policy
/// Callbacks borrow their input spans and may own move-only captures. Their output contracts are unchecked.
/// Processing errors retain completed earlier changes; callback and allocation exceptions propagate.



//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

// Framework
#include <FCG/Mesh/export.h>
#include <FCG/Mesh/error.h>
#include <FCG/Mesh/mesh.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_mesh_processing
/// @{



//////
//
// Structs and enums
//

/// An oriented triangle expressed as indices into an ordered face's corners.
using MeshTriangle = std::array<std::size_t, 3>;

/// Geometry borrowed by a triangulation callback for the duration of its invocation.
struct MeshFaceView {
	/// Ordered object-space corner positions, in the source coordinate units.
	std::span<const OpenMesh::Vec3d> positions;
};

/// One oriented incident triangle with the vertex being processed at index zero.
struct MeshIncidentTriangle {
	/// Object-space positions in source coordinate units, retaining the triangle's winding.
	std::array<OpenMesh::Vec3d, 3> positions;
};

/// Which vertex normals an explicit normal computation replaces.
enum class VertexNormalMode {
	/// Compute every vertex normal.
	ReplaceAll,

	/// Compute only vertex normals marked absent by the attribute traits.
	MissingOnly
};



//////
//
// Classes
//

/// Produces a fan rooted at the first corner; concave faces can yield overlapping or reversed triangles.
class FanTriangulator
{
public:

	////
	// Methods

	/// Triangulate a face using its existing corners.
	///
	/// \pre The face contains at least three corners.
	///
	/// \param face Borrowed positions in boundary order.
	///
	/// \return The oriented corner-index triangles.
	[[nodiscard]]
	inline auto operator() (MeshFaceView face) const -> std::expected<std::vector<MeshTriangle>, MeshError> {
		std::vector<MeshTriangle> result;
		result.reserve(face.positions.size() - 2);
		for (std::size_t i = 1; i + 1 < face.positions.size(); ++i)
			result.push_back({0, i, i + 1});
		return result;
	}
};
/// Ready-to-use first-corner fan strategy.
inline constexpr FanTriangulator triangulateFan;

/// Triangulates a simple polygon by ear cutting of its dominant-axis projection.
class FCG_MESH_EXPORT EarCutTriangulator
{
public:

	////
	// Methods

	/// Triangulate a polygon without adding vertices; nonplanar polygons use the projected boundary.
	///
	/// \pre The face contains at least three corners.
	///
	/// \param face Borrowed object-space positions in boundary order.
	///
	/// \return Corner-index triangles, or a processing error when projection or clipping cannot progress.
	[[nodiscard]] auto operator() (MeshFaceView face) const -> std::expected<std::vector<MeshTriangle>, MeshError>;
};
/// Ready-to-use projected ear-cutting strategy.
inline constexpr EarCutTriangulator triangulateEarCut;

/// Assigns equal weights to incident triangles.
class UniformNormalWeighter
{
public:

	////
	// Methods

	/// Fill the output span with equal weights.
	///
	/// \pre Both spans have the same size.
	///
	/// \param triangles Borrowed incident triangles.
	/// \param weights Borrowed output receiving every weight.
	void operator() (std::span<const MeshIncidentTriangle> triangles, std::span<double> weights) const {
		(void)triangles;
		std::fill(weights.begin(), weights.end(), 1.0);
	}
};
/// Ready-to-use uniform normal weights.
inline constexpr UniformNormalWeighter vertexNormalUniformWeights;

/// Assigns triangle-area weights to incident triangles.
class AreaNormalWeighter
{
public:

	////
	// Methods

	/// Fill the output span with triangle areas in squared source units.
	///
	/// \pre Both spans have the same size.
	///
	/// \param triangles Borrowed incident triangles.
	/// \param weights Borrowed output receiving every weight.
	void operator() (std::span<const MeshIncidentTriangle> triangles, std::span<double> weights) const {
		for (std::size_t i = 0; i < triangles.size(); ++i) {
			const auto &p = triangles[i].positions;
			weights[i] = 0.5 * OpenMesh::cross(p[1] - p[0], p[2] - p[0]).norm();
		}
	}
};
/// Ready-to-use area normal weights.
inline constexpr AreaNormalWeighter vertexNormalAreaWeights;

/// Assigns the angle at the target vertex as each incident triangle's weight.
class AngleNormalWeighter
{
public:

	////
	// Methods

	/// Fill the output span with corner angles in radians; collapsed edges receive zero.
	///
	/// \pre Both spans have the same size.
	///
	/// \param triangles Borrowed incident triangles with their target vertex first.
	/// \param weights Borrowed output receiving every weight.
	void operator() (std::span<const MeshIncidentTriangle> triangles, std::span<double> weights) const {
		for (std::size_t i = 0; i < triangles.size(); ++i) {
			const auto &p = triangles[i].positions;
			const auto a = p[1] - p[0], b = p[2] - p[0];
			weights[i] = std::atan2(OpenMesh::cross(a, b).norm(), OpenMesh::dot(a, b));
		}
	}
};
/// Ready-to-use corner-angle normal weights.
inline constexpr AngleNormalWeighter vertexNormalAngleWeights;

/// Availability customization for meshes using OpenMesh's standard attribute properties.
///
/// \tparam MeshT The concrete OpenMesh type; specialize this class for custom sparse availability rules.
template<class MeshT> class MeshAttributeTraits
{
public:

	////
	// Methods

	/// Query normal availability at a vertex, face, or incoming corner halfedge.
	///
	/// \tparam Handle The OpenMesh element handle type.
	/// \param mesh Borrowed source mesh.
	/// \param handle Element whose normal is queried.
	///
	/// \return Whether its normal can be read.
	template<class Handle> static auto hasNormal (const MeshT &mesh, Handle handle) -> bool {
		if constexpr (std::convertible_to<Handle, typename MeshT::VertexHandle>)
			return mesh.has_vertex_normals();
		else if constexpr (std::convertible_to<Handle, typename MeshT::FaceHandle>)
			return mesh.has_face_normals();
		else
			return mesh.has_halfedge_normals();
	}

	/// Query RGBA or RGB color availability.
	///
	/// \tparam Handle The OpenMesh element handle type.
	/// \param mesh Borrowed source mesh.
	/// \param handle Element whose color is queried.
	///
	/// \return Whether its color can be read.
	template<class Handle> static auto hasColor (const MeshT &mesh, Handle handle) -> bool {
		if constexpr (std::convertible_to<Handle, typename MeshT::VertexHandle>)
			return mesh.has_vertex_colors();
		else if constexpr (std::convertible_to<Handle, typename MeshT::FaceHandle>)
			return mesh.has_face_colors();
		else
			return mesh.has_halfedge_colors();
	}

	/// Query 2D texture coordinate availability.
	///
	/// \tparam Handle A vertex or incoming corner halfedge handle.
	/// \param mesh Borrowed source mesh.
	/// \param handle Element whose UVs are queried.
	///
	/// \return Whether its 2D coordinates can be read.
	template<class Handle> static auto hasTexCoord (const MeshT &mesh, Handle handle) -> bool {
		if constexpr (std::convertible_to<Handle, typename MeshT::VertexHandle>)
			return mesh.has_vertex_texcoords2D();
		else
			return mesh.has_halfedge_texcoords2D();
	}

	/// Mark a vertex normal available after writing it; standard properties are dense.
	///
	/// \param mesh Modified mesh.
	/// \param vertex Vertex whose normal was written.
	static void markVertexNormal (MeshT &mesh, typename MeshT::VertexHandle vertex) {}
};

/// Availability rules for the per-element flags of \c Mesh.
template<> class MeshAttributeTraits<Mesh>
{
public:

	////
	// Methods

	/// Query the normal property and the element's availability flag.
	///
	/// \tparam Handle An OpenMesh vertex, face or halfedge handle.
	/// \param mesh Borrowed source mesh.
	/// \param handle Queried element.
	///
	/// \return Whether the normal exists on this element.
	template<class Handle> static auto hasNormal (const Mesh &mesh, Handle handle) -> bool
	{
		bool property;
		if constexpr (std::convertible_to<Handle, Mesh::VertexHandle>)
			property = mesh.has_vertex_normals();
		else if constexpr (std::convertible_to<Handle, Mesh::FaceHandle>)
			property = mesh.has_face_normals();
		else
			property = mesh.has_halfedge_normals();
		return property && mesh.data(handle).hasNormal;
	}

	/// Query color availability; explicitly requested corner colors use dense storage.
	///
	/// \tparam Handle An OpenMesh vertex, face or halfedge handle.
	/// \param mesh Borrowed source mesh.
	/// \param handle Queried element.
	/// \return Whether a color exists on this element.
	template<class Handle> static auto hasColor (const Mesh &mesh, Handle handle) -> bool {
		if constexpr (std::convertible_to<Handle, Mesh::VertexHandle>)
			return mesh.has_vertex_colors() && mesh.data(handle).hasColor;
		else if constexpr (std::convertible_to<Handle, Mesh::FaceHandle>)
			return mesh.has_face_colors() && mesh.data(handle).hasColor;
		else return mesh.has_halfedge_colors();
	}

	/// Query the UV property and the element's availability flag.
	///
	/// \tparam Handle An OpenMesh vertex or halfedge handle.
	/// \param mesh Borrowed source mesh.
	/// \param handle Queried element.
	///
	/// \return Whether UVs exist on this element.
	template<class Handle> static auto hasTexCoord (const Mesh &mesh, Handle handle) -> bool {
		if constexpr (std::convertible_to<Handle, Mesh::VertexHandle>)
			return mesh.has_vertex_texcoords2D() && mesh.data(handle).hasTexCoord;
		else return mesh.has_halfedge_texcoords2D() && mesh.data(handle).hasTexCoord;
	}

	/// Mark a generated vertex normal available.
	///
	/// \param mesh Modified mesh.
	/// \param vertex Vertex whose normal was written.
	static void markVertexNormal (Mesh &mesh, Mesh::VertexHandle vertex) { mesh.data(vertex).hasNormal = true; }
};



//////
//
// Structs and enums
//

/// Independent optional preprocessing callables, owning their captures.
///
/// \tparam Triangulate Callable returning corner-index triangles for a borrowed \c MeshFaceView.
/// \tparam WeighNormals Callable filling a weight span from incident triangles.
template<class Triangulate=EarCutTriangulator, class WeighNormals=AngleNormalWeighter>
struct MeshPreprocessingPolicy {
	/// Optional triangulation; successful results must triangulate the cyclic corner indices without new vertices.
	std::optional<Triangulate> triangulate;

	/// Optional missing-normal generation; fills every output weight with a finite nonnegative value.
	std::optional<WeighNormals> vertexNormals;
};



//////
//
// Functions
//

////
// Sub-namespaces

/// Internal geometry and callback helpers shared by CPU and GPU preparation.
namespace detail {

/// Copy a three-component vector into double-precision geometry.
/// \tparam V Source vector type.
/// \param value Borrowed source value.
/// \return The converted coordinates.
template<class V> auto meshPoint (const V &value) -> OpenMesh::Vec3d {
	return {double(value[0]), double(value[1]), double(value[2])};
}

/// Normalize a direction, retaining zero when no direction exists.
/// \param value Geometric direction.
/// \return A unit direction or zero.
inline auto meshUnit (OpenMesh::Vec3d value) -> OpenMesh::Vec3d {
	const auto length = value.norm();
	return length > 0 ? value / length : OpenMesh::Vec3d(0, 0, 0);
}

/// Sum weighted geometric unit normals without allocating.
/// \param triangles Incident triangle geometry.
/// \param weights One weight per triangle.
/// \return The normalized sum, or zero for an empty or cancelling sum.
inline auto weightedMeshNormal (std::span<const MeshIncidentTriangle> triangles, std::span<const double> weights)
	-> OpenMesh::Vec3d
{
	OpenMesh::Vec3d sum(0, 0, 0);
	for (std::size_t i = 0; i < triangles.size(); ++i) {
		const auto &p = triangles[i].positions;
		sum += meshUnit(OpenMesh::cross(p[1] - p[0], p[2] - p[0])) * weights[i];
	}
	return meshUnit(sum);
}

/// Plan ear removals from an unchecked combinatorial triangulation; the final triangle needs no split.
/// \param count Original corner count.
/// \param triangles Contract-compliant replacement triangles.
/// \return Ordered ear triples, with their removable corner in the middle.
[[nodiscard]] FCG_MESH_EXPORT auto meshSplitPlan (std::size_t count, std::span<const MeshTriangle> triangles)
	-> std::vector<MeshTriangle>;

/// Choose a default callable type for an absent policy slot.
/// \tparam T Supplied callable or absence marker.
/// \tparam Default Concrete type used for an absent slot.
template<class T, class Default>
using MeshPolicyCallable = std::conditional_t<std::same_as<std::decay_t<T>, std::nullopt_t>, Default, std::decay_t<T>>;

// Sub-namespace close
} // namespace detail


/// Construct a policy while deducing callable types and retaining move-only captures.
///
/// \tparam T Triangulation callable or \c std::nullopt_t.
/// \tparam N Normal weighting callable or \c std::nullopt_t.
/// \param triangulate Triangulator, or \c std::nullopt to disable it.
/// \param normals Weighter, or \c std::nullopt to disable it.
/// \return An owning policy; allocation and capture-construction exceptions propagate.
template<class T, class N> [[nodiscard]] auto makeMeshPreprocessingPolicy (T &&triangulate, N &&normals)
	-> MeshPreprocessingPolicy<detail::MeshPolicyCallable<T, EarCutTriangulator>,
		detail::MeshPolicyCallable<N, AngleNormalWeighter>>
{
	MeshPreprocessingPolicy<detail::MeshPolicyCallable<T, EarCutTriangulator>,
		detail::MeshPolicyCallable<N, AngleNormalWeighter>> result;
	if constexpr (!std::same_as<std::decay_t<T>, std::nullopt_t>) result.triangulate.emplace(std::forward<T>(triangulate));
	if constexpr (!std::same_as<std::decay_t<N>, std::nullopt_t>) result.vertexNormals.emplace(std::forward<N>(normals));
	return result;
}

/// Triangulate original polygon faces in place, retaining source face and incoming-corner properties.
///
/// \pre The callback returns a valid combinatorial triangulation using the existing corners; this is unchecked.
/// \tparam MeshT Mutable OpenMesh polygon or triangle mesh type.
/// \tparam Fn Concrete triangulation callable type.
/// \param mesh Mesh to modify; handles are retained and garbage collection is not performed.
/// \param fn Callback borrowed for the call; captures may be mutable and move-only.
/// \return Success or a face-specific processing error; completed earlier faces remain changed on failure.
/// \throws ... Allocation, property-copy and callback exceptions propagate.
template<class MeshT, class Fn=EarCutTriangulator>
[[nodiscard]] auto triangulateMesh (MeshT &mesh, Fn &&fn={}) -> std::expected<void, MeshError>
{
	// Snapshot original face handles to avoid processing newly inserted faces
	std::vector<typename MeshT::FaceHandle> faces;
	for (auto face : mesh.faces()) faces.push_back(face);
	std::vector<OpenMesh::Vec3d> positions;
	std::vector<typename MeshT::HalfedgeHandle> corners;
	for (auto face : faces)
	{
		positions.clear();
		corners.clear();
		for (auto halfedge : mesh.fh_range(face)) {
			corners.push_back(halfedge);
			positions.push_back(detail::meshPoint(mesh.point(mesh.to_vertex_handle(halfedge))));
		}
		if (corners.size() == 3) continue;
		auto triangles = std::invoke(fn, MeshFaceView{positions});
		if (!triangles) return std::unexpected(MeshError{MeshErrorCode::ProcessingFailure,
			"Face " + std::to_string(face.idx()) + ": " + triangles.error().message});

		// Apply the planned diagonals through the concrete mesh API
		if constexpr (!MeshT::IsTriMesh)
			for (const auto &ear : detail::meshSplitPlan(corners.size(), *triangles))
			{
				const auto diagonal = mesh.insert_edge(corners[ear[0]], mesh.next_halfedge_handle(corners[ear[2]]));
				const auto opposite = mesh.opposite_halfedge_handle(diagonal);
				const auto newFace = mesh.face_handle(diagonal);
				const auto oldFace = mesh.face_handle(opposite);
				mesh.copy_all_properties(face, newFace, true);
				mesh.copy_all_properties(face, oldFace, true);
				mesh.copy_all_properties(corners[ear[2]], diagonal, true);
				mesh.copy_all_properties(corners[ear[0]], opposite, true);
				corners[ear[2]] = diagonal;
				face = newFace;
			}
	}
	return {};
}

/// Compute geometric smooth vertex normals on a triangular mesh, requesting vertex-normal storage as needed.
///
/// \pre The callback fills every weight with a finite nonnegative value; spans must not be retained.
/// \tparam MeshT Mutable OpenMesh mesh type.
/// \tparam Fn Concrete weighting callable type.
/// \param mesh Mesh whose vertex normals are written; face and corner normals are retained.
/// \param fn Borrowed mutable or move-only callback filling one weight per incident triangle.
/// \param mode Whether to replace every vertex normal or only absent vertex normals.
/// \return Success or an error upon encountering a non-triangle; previously completed vertices retain their normals.
/// \throws ... Allocation and callback exceptions propagate.
template<class MeshT, class Fn=AngleNormalWeighter>
[[nodiscard]] auto computeVertexNormals (MeshT &mesh, Fn &&fn={}, VertexNormalMode mode=VertexNormalMode::ReplaceAll)
	-> std::expected<void, MeshError>
{
	// Reuse scratch arrays across vertices; callbacks see the target corner first
	using Traits = MeshAttributeTraits<MeshT>;
	std::vector<MeshIncidentTriangle> incident;
	std::vector<double> weights;
	const bool hadNormals = mesh.has_vertex_normals();
	if (!hadNormals) mesh.request_vertex_normals();
	for (auto vertex : mesh.vertices())
	{
		if (mode == VertexNormalMode::MissingOnly && hadNormals && Traits::hasNormal(mesh, vertex)) continue;
		incident.clear();
		for (auto face : mesh.vf_range(vertex))
		{
			MeshIncidentTriangle triangle;
			std::size_t count = 0, target = 0;
			for (auto corner : mesh.fv_range(face)) {
				if (count == 3) return std::unexpected(MeshError{MeshErrorCode::ProcessingFailure,
					"Non-triangle face " + std::to_string(face.idx())});
				if (corner == vertex) target = count;
				triangle.positions[count++] = detail::meshPoint(mesh.point(corner));
			}
			if (count != 3) return std::unexpected(MeshError{MeshErrorCode::ProcessingFailure, "Non-triangle face"});
			std::rotate(triangle.positions.begin(), triangle.positions.begin() + target, triangle.positions.end());
			incident.push_back(triangle);
		}
		weights.resize(incident.size());
		std::invoke(fn, std::span<const MeshIncidentTriangle>(incident), std::span<double>(weights));
		const auto normal = detail::weightedMeshNormal(incident, weights);
		mesh.set_normal(vertex, typename MeshT::Normal(normal[0], normal[1], normal[2]));
		Traits::markVertexNormal(mesh, vertex);
	}
	return {};
}



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // __FCG_MESH_MESH_PROCESSING_H__
