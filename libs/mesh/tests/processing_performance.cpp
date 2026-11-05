
//////
//
// Includes
//

// C++ STL
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <span>
#include <vector>

// Framework
#include <FCG/Mesh/mesh_processing.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Whether to count allocations during the processing phase only.
bool counting = false;

/// Number of allocations observed during the processing phase.
std::size_t allocations = 0;

/// Incident faces at a vertex of the regular triangular grid.
struct VertexFaces
{
	/// Face handles for the grid's maximum of six incident triangles.
	std::array<OpenMesh::FaceHandle, 6> handles;

	/// Number of occupied entries in \c handles.
	std::size_t count = 0;
};

/// Array-backed grid exposing the mesh operations used by \c fcg::computeVertexNormals.
class AllocationMesh
{
public:

	////
	// Types

	/// Index of a vertex in the grid's position and normal arrays.
	using VertexHandle = OpenMesh::VertexHandle;

	/// Index of a triangle in the grid's face array.
	using FaceHandle = OpenMesh::FaceHandle;

	/// Double-precision geometric direction.
	using Normal = OpenMesh::Vec3d;


	////
	// Object construction

	/// \brief Construct a planar grid with counterclockwise triangles and no normal storage.
	///
	/// \pre \p side is at least two.
	///
	/// \param side Number of vertices along each grid edge.
	explicit AllocationMesh(unsigned side)
		: positions_(std::size_t(side) * side), incident_(positions_.size())
	{
		// Populate vertex handles and positions before measuring processing allocations
		vertices_.reserve(positions_.size());
		for (unsigned y = 0; y < side; ++y)
			for (unsigned x = 0; x < side; ++x) {
				vertices_.emplace_back(int(vertices_.size()));
				positions_[vertices_.back().idx()] = {double(x), double(y), 0};
			}

		// Record each triangle and its incidence at all three corners
		faces_.reserve(2 * std::size_t(side - 1) * (side - 1));
		for (unsigned y = 0; y + 1 < side; ++y)
			for (unsigned x = 0; x + 1 < side; ++x) {
				const auto i = y * side + x;
				addFace(vertices_[i], vertices_[i + 1], vertices_[i + side + 1]);
				addFace(vertices_[i], vertices_[i + side + 1], vertices_[i + side]);
			}
	}


	////
	// Accessors

	/// Return the number of grid vertices.
	[[nodiscard]] auto n_vertices () const -> std::size_t { return vertices_.size(); }

	/// Return whether dense vertex-normal storage has been requested.
	[[nodiscard]] auto has_vertex_normals () const -> bool { return !normals_.empty(); }

	/// Borrow all grid vertex handles for the lifetime of this mesh.
	[[nodiscard]] auto vertices () const -> std::span<const VertexHandle> { return vertices_; }

	/// \brief Borrow the triangles incident to a vertex for the lifetime of this mesh.
	///
	/// \param vertex Valid grid vertex handle.
	///
	/// \return The vertex's incident face handles.
	[[nodiscard]] auto vf_range (VertexHandle vertex) const -> std::span<const FaceHandle> {
		const auto &faces = incident_[vertex.idx()];
		return {faces.handles.data(), faces.count};
	}

	/// \brief Borrow a triangle's corners for the lifetime of this mesh.
	///
	/// \param face Valid grid face handle.
	///
	/// \return Three vertex handles in counterclockwise order.
	[[nodiscard]] auto fv_range (FaceHandle face) const -> std::span<const VertexHandle> {
		return faces_[face.idx()];
	}

	/// \brief Borrow a vertex position in grid coordinates for the lifetime of this mesh.
	///
	/// \param vertex Valid grid vertex handle.
	///
	/// \return The position, with unit spacing between adjacent grid vertices.
	[[nodiscard]] auto point (VertexHandle vertex) const -> const OpenMesh::Vec3d & {
		return positions_[vertex.idx()];
	}

	/// \brief Borrow a generated vertex normal until normal storage is resized or the mesh is destroyed.
	///
	/// \pre Vertex-normal storage has been requested.
	///
	/// \param vertex Valid grid vertex handle.
	///
	/// \return The stored geometric direction.
	[[nodiscard]] auto normal (VertexHandle vertex) const -> const Normal & { return normals_[vertex.idx()]; }


	////
	// Methods

	/// Allocate dense storage for one normal per vertex.
	void request_vertex_normals () { normals_.resize(vertices_.size(), Normal(0, 0, 0)); }

	/// \brief Store a generated vertex normal.
	///
	/// \pre Vertex-normal storage has been requested.
	///
	/// \param vertex Valid grid vertex handle.
	/// \param value Geometric direction to store.
	void set_normal (VertexHandle vertex, const Normal &value) { normals_[vertex.idx()] = value; }


private:

	////
	// Methods

	/// \brief Append a counterclockwise triangle and record it at each of its three vertices.
	///
	/// \param a First corner's vertex handle.
	/// \param b Second corner's vertex handle.
	/// \param c Third corner's vertex handle.
	void addFace (VertexHandle a, VertexHandle b, VertexHandle c)
	{
		const FaceHandle face(int(faces_.size()));
		faces_.push_back({a, b, c});
		for (auto vertex : faces_.back()) {
			auto &faces = incident_[vertex.idx()];
			faces.handles.at(faces.count++) = face;
		}
	}


	////
	// Fields

	/// Vertex handles in grid insertion order.
	std::vector<VertexHandle> vertices_;

	/// Vertex positions with unit grid spacing in the XY plane.
	std::vector<OpenMesh::Vec3d> positions_;

	/// Counterclockwise vertex triples for every grid triangle.
	std::vector<std::array<VertexHandle, 3>> faces_;

	/// Incident face handles at each grid vertex.
	std::vector<VertexFaces> incident_;

	/// Dense normals allocated by the processing call.
	std::vector<Normal> normals_;
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Count ordinary allocations without changing their alignment or failure behavior.
auto operator new (std::size_t size) -> void* {
	if (counting) ++allocations;
	if (auto *p = std::malloc(size ? size : 1)) return p;
	throw std::bad_alloc();
}

/// Release ordinary allocations.
void operator delete (void *p) noexcept { std::free(p); }

/// Release sized ordinary allocations.
void operator delete (void *p, std::size_t) noexcept { std::free(p); }

/// Compare concrete and explicitly erased callbacks without timing assertions.
auto main () -> int
{
	// Build a regular triangular grid before counting processing allocations
	constexpr unsigned side = 48;
	AllocationMesh mesh(side);
	allocations = 0;
	counting = true;
	const auto result = fcg::computeVertexNormals(mesh);
	counting = false;
	if (!result) {
		std::cerr << "Normal computation failed: " << result.error().message << '\n';
		return 1;
	}
	if (allocations >= 64) {
		std::cerr << "Normal-processing allocation budget exceeded: " << allocations
			<< " allocations for " << mesh.n_vertices() << " vertices (expected fewer than 64)\n";
		return 1;
	}
	std::cout << "Processing " << mesh.n_vertices() << " vertices required " << allocations << " allocations\n";

	// Verify that the measured traversal generated a finite positive-Z normal at every vertex
	for (auto vertex : mesh.vertices())
		for (unsigned axis = 0; axis < 3; ++axis)
		{
			const auto component = mesh.normal(vertex)[axis];
			if (!std::isfinite(component) || std::abs(component - (axis == 2 ? 1.0 : 0.0)) > 1e-6) {
				std::cerr << "Incorrect grid normal at vertex " << vertex.idx() << '\n';
				return 1;
			}
		}

	// Reuse identical geometry and scratch in both timing paths
	std::array<fcg::MeshIncidentTriangle, 6> triangles;
	for (auto &triangle : triangles)
		triangle.positions = {OpenMesh::Vec3d(0,0,0), {1,0,0}, {0,1,0}};
	std::array<double, 6> weights;
	const auto measure = [&] (auto &callable) {
		const auto start = std::chrono::steady_clock::now();
		for (unsigned i = 0; i < 200000; ++i) {
			triangles[0].positions[1][1] = double(i % 97) * .0001;
			std::invoke(callable, std::span<const fcg::MeshIncidentTriangle>(triangles), std::span<double>(weights));
			// Keep repeated writes observable without an extra dynamic call
			std::atomic_signal_fence(std::memory_order_seq_cst);
		}
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	};
	auto concrete = fcg::vertexNormalAngleWeights;
	std::function<void(std::span<const fcg::MeshIncidentTriangle>, std::span<double>)> erased = concrete;
	std::cout << "Concrete callback: " << measure(concrete) << " ms; erased callback: " << measure(erased) << " ms\n";
	return 0;
}
