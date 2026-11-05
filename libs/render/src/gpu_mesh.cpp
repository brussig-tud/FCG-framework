
//////
//
// Includes
//

// C++ STL
#include <bit>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

// Framework
#include <FCG/Render/gpu_mesh.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Fixed-width packed key; unused components are zero and original vertex identity is retained.
struct VertexKey
{
	/// Original vertex index.
	std::size_t source;

	/// Canonical floating-point component bit patterns.
	std::array<std::uint32_t, 12> components{};

	/// Compare both source identity and all selected components.
	auto operator== (const VertexKey&) const -> bool = default;
};

/// Hash interleaved vertex components without inspecting struct padding.
struct VertexHash {
	/// Combine source identity and canonical component words.
	auto operator() (const VertexKey &key) const -> std::size_t {
		auto hash = key.source;
		for (const auto word : key.components) hash ^= word + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		return hash;
	}
};

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Function implementations
//

auto detail::meshIncidence (const GPUMeshData &data) -> MeshIncidence
{
	// Count and prefix-sum all incidences into one contiguous allocation
	MeshIncidence result;
	result.offsets.resize(data.vertices.size() + 1);
	for (const auto &corner : data.corners) ++result.offsets[corner.vertex + 1];
	std::partial_sum(result.offsets.begin(), result.offsets.end(), result.offsets.begin());
	result.triangles.resize(data.corners.size());
	auto cursor = result.offsets;
	for (std::size_t i = 0; i < data.corners.size(); i += 3)
		for (std::size_t j = 0; j < 3; ++j)
		{
			MeshIncidentTriangle triangle;
			for (std::size_t k = 0; k < 3; ++k)
				triangle.positions[k] = data.vertices[data.corners[i + (j + k) % 3].vertex].position;
			result.triangles[cursor[data.corners[i + j].vertex]++] = triangle;
		}
	return result;
}

auto detail::packMesh (const GPUMeshData &data, std::optional<GPUMeshLayout> requested) -> PackedMesh
{
	// Infer channels from drawable corners rather than merely allocated OpenMesh properties
	PackedMesh result;
	for (const auto &corner : data.corners) {
		result.layout.normals |= corner.normal.has_value() || data.vertices[corner.vertex].generated.has_value();
		result.layout.colors |= corner.color.has_value();
		result.layout.texCoords |= corner.uv.has_value();
	}
	if (requested) result.layout = *requested;
	const auto components = result.layout.stride() / sizeof(float);
	const auto limit = std::numeric_limits<std::uint32_t>::max();
	if (data.corners.size() > limit / sizeof(std::uint32_t)) throw std::length_error("Mesh index buffer exceeds SDL limits");
	result.indices.reserve(data.corners.size());

	// Reuse the first representation by direct mapping; hash only vertices with differing corner attributes
	std::vector<std::optional<std::uint32_t>> first(data.vertices.size());
	std::unordered_map<VertexKey, std::uint32_t, VertexHash> split;
	for (std::size_t i = 0; i < data.corners.size(); i += 3)
	{
		const auto &a = data.vertices[data.corners[i].vertex].position;
		const auto &b = data.vertices[data.corners[i + 1].vertex].position;
		const auto &c = data.vertices[data.corners[i + 2].vertex].position;
		const auto flat = meshUnit(OpenMesh::cross(b - a, c - a));
		for (std::size_t j = 0; j < 3; ++j)
		{
			const auto &corner = data.corners[i + j];
			const auto &vertex = data.vertices[corner.vertex];
			std::array<float, 12> values{};
			std::size_t offset = 0;
			for (int k = 0; k < 3; ++k) values[offset++] = float(vertex.position[k]);
			if (result.layout.normals) {
				const auto normal = corner.normal.value_or(vertex.generated.value_or(flat));
				for (int k = 0; k < 3; ++k) values[offset++] = float(normal[k]);
			}
			if (result.layout.colors)
				for (float value : corner.color.value_or(std::array<float, 4>{1, 1, 1, 1})) values[offset++] = value;
			if (result.layout.texCoords)
				for (float value : corner.uv.value_or(std::array<float, 2>{0, 0})) values[offset++] = value;
			VertexKey key{corner.vertex};
			for (std::size_t k = 0; k < components; ++k) key.components[k] = std::bit_cast<std::uint32_t>(values[k]);
			auto &initial = first[corner.vertex];
			if (initial && std::memcmp(result.vertices.data() + *initial * components, values.data(),
				components * sizeof(float)) == 0) {
				result.indices.push_back(*initial);
				continue;
			}
			if (initial) {
				const auto found = split.find(key);
				if (found != split.end()) { result.indices.push_back(found->second); continue; }
			}
			if (result.vertices.size() > limit / sizeof(float) - components)
				throw std::length_error("Mesh vertex buffer exceeds SDL limits");
			const auto index = std::uint32_t(result.vertices.size() / components);
			if (initial) split.emplace(key, index);
			else initial = index;
			result.vertices.insert(result.vertices.end(), values.begin(), values.begin() + components);
			result.indices.push_back(index);
		}
	}
	return result;
}



//////
//
// Class implementations
//

////
// GPUMesh

auto GPUMesh::upload (Device &device, const detail::GPUMeshData &data, const GPUMeshOptions &options)
	-> std::expected<GPUMesh, RenderError>
{
	// Pack before allocating device resources; arithmetic limits are operational errors
	detail::PackedMesh packed;
	try { packed = detail::packMesh(data, options.layout); }
	catch (const std::length_error &error) {
		return std::unexpected(RenderError{RenderErrorCode::InvalidArgument, error.what()});
	}
	GPUMesh result(device);
	result.m_layout = packed.layout;
	result.m_indexCount = packed.indices.size();
	result.m_vertexCount = packed.vertices.size() * sizeof(float) / packed.layout.stride();
	if (packed.indices.empty()) return result;

	// Own all buffers locally until the single upload transaction succeeds
	const auto fail = [] (const BufferError &error) {
		return std::unexpected(RenderError{error.code == BufferErrorCode::SDLFailure
			? RenderErrorCode::SDLFailure : RenderErrorCode::InvalidArgument, "GPU mesh: " + error.message});
	};
	auto vertices = Buffer::create(device, packed.vertices.size() * sizeof(float), SDL_GPU_BUFFERUSAGE_VERTEX);
	if (!vertices) return fail(vertices.error());
	auto indices = Buffer::create(device, packed.indices.size() * sizeof(std::uint32_t), SDL_GPU_BUFFERUSAGE_INDEX);
	if (!indices) return fail(indices.error());
	UploadBatch uploads(device);
	auto uploaded = uploads.upload(*vertices, std::span(packed.vertices));
	if (uploaded) uploaded = uploads.upload(*indices, std::span(packed.indices));
	if (uploaded) uploaded = uploads.submit();
	if (!uploaded) return fail(uploaded.error());
	result.m_vertices.emplace(std::move(*vertices));
	result.m_indices.emplace(std::move(*indices));
	return result;
}



//////
//
// Module namespace close
//

// namespace fcg
}
