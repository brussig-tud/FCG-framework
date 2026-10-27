
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <set>
#include <utility>

// Local includes
#include "shapes.h"



//////
//
// Class implementations
//

////
// SimpleShape

SimpleShape::~SimpleShape() = default;

auto SimpleShape::triangulatedEdges () const -> std::vector<std::uint32_t>
{
	// Map the flat-shaded mesh vertices back to their exact, shared geometric corners
	const auto nodes = corners();
	std::vector<std::uint32_t> cornerForVertex;
	cornerForVertex.reserve(vertices().size());
	for (const auto &vertex : vertices())
		cornerForVertex.push_back((std::uint32_t)(
			std::find(nodes.begin(), nodes.end(), vertex.position) - nodes.begin()
		));

	// Read the same triangle list supplied to the mesh uploader, deduplicating shared edges
	std::set<std::pair<std::uint32_t, std::uint32_t>> unique;
	const auto triangles = indices();
	for (std::size_t i = 0; i < triangles.size(); i += 3)
		for (std::size_t side = 0; side < 3; ++side) {
			const auto a = cornerForVertex[triangles[i + side]];
			const auto b = cornerForVertex[triangles[i + (side + 1) % 3]];
			unique.emplace(std::min(a, b), std::max(a, b));
		}

	// Flatten pairs into the connectivity renderer's contiguous 32-bit representation
	std::vector<std::uint32_t> result;
	result.reserve(unique.size() * 2);
	for (const auto &[a, b] : unique) {
		result.push_back(a);
		result.push_back(b);
	}
	return result;
}

auto SimpleShape::uploadGeometry (
	fcg::Device &device, std::span<const Vertex> vertices, std::span<const std::uint32_t> indices
) -> bool
{
	// Empty geometry is a successful replacement and must not allocate zero-byte SDL resources.
	if (vertices.empty() || indices.empty()) {
		m_geometry.reset();
		return true;
	}
	if (!uploads)
		uploads.emplace(device);
	if (m_geometry && m_geometry->vertices.device() != &device) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot migrate a shape between GPU devices");
		return false;
	}

	// Build replacements first. Failure leaves the old GPU geometry and dirty flag intact.
	auto vertex = fcg::Buffer::create(device, vertices.size_bytes(), SDL_GPU_BUFFERUSAGE_VERTEX);
	auto index = fcg::Buffer::create(device, indices.size_bytes(), SDL_GPU_BUFFERUSAGE_INDEX);
	if (!vertex || !index) {
		const auto &error = !vertex ? vertex.error() : index.error();
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating geometry: %s", error.message.c_str());
		return false;
	}
	auto result = uploads->upload(*vertex, vertices);
	if (result)
		result = uploads->upload(*index, indices);
	if (result)
		result = uploads->submit();
	if (!result) {
		uploads->clear(); // Drop borrowed handles before the replacement owners leave scope.
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Uploading geometry: %s", result.error().message.c_str());
		return false;
	}
	m_geometry.emplace(Geometry{std::move(*vertex), std::move(*index), indices.size()});
	return true;
}
