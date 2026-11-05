
//////
//
// Includes
//

// C++ STL
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <stdexcept>

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
	fcg::Mesh mesh;
	constexpr unsigned side = 48;
	std::vector<fcg::Mesh::VertexHandle> vertices;
	for (unsigned y = 0; y < side; ++y)
		for (unsigned x = 0; x < side; ++x) vertices.push_back(mesh.add_vertex({float(x), float(y), 0}));
	for (unsigned y = 0; y + 1 < side; ++y)
		for (unsigned x = 0; x + 1 < side; ++x) {
			const auto i = y * side + x;
			mesh.add_face(vertices[i], vertices[i + 1], vertices[i + side + 1]);
			mesh.add_face(vertices[i], vertices[i + side + 1], vertices[i + side]);
		}
	counting = true;
	const auto result = fcg::computeVertexNormals(mesh);
	counting = false;
	if (!result || allocations >= 64) {
		std::cerr << "Per-vertex scratch allocation regression: " << allocations << '\n';
		return 1;
	}
	std::cout << "Processing " << mesh.n_vertices() << " vertices required " << allocations << " allocations\n";

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
