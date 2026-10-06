
//////
//
// Includes
//

// Local includes
#include "shapes.h"



//////
//
// SimpleShape
//

SimpleShape::~SimpleShape () = default;

auto SimpleShape::uploadGeometry (
	fcg::Device &device, std::span<const Vertex> vertices, std::span<const std::uint32_t> indices
) -> bool
{
	// Empty geometry is a successful replacement and must not allocate zero-byte SDL resources.
	if (vertices.empty() || indices.empty()) {
		m_vertexBuffer = {};
		m_indexBuffer = {};
		m_numIndices = 0;
		return true;
	}
	if (!uploads)
		uploads.emplace(device);
	if (auto *previous = m_vertexBuffer.device(); previous && previous != &device) {
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
	m_vertexBuffer = std::move(*vertex);
	m_indexBuffer = std::move(*index);
	m_numIndices = indices.size();
	return true;
}
