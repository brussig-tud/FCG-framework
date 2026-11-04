
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

// Framework
#include "mesh_internal.h"



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

// Private mesh decoding helpers.
namespace detail {

auto meshHint (std::string_view hint) -> std::string
{
	if (hint.starts_with('.'))
		hint.remove_prefix(1);
	std::string result(hint);
	for (auto &c : result)
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	return result;
}

auto constructMesh (const MeshSource &source, MeshLoaderOptions options) -> std::expected<Mesh, MeshError>
{
	// Validate the complete input before calling OpenMesh functions that assume valid handles
	const auto finite = [] (const auto &v) {
		for (const auto value : v)
			if (!std::isfinite(value))
				return false;
		return true;
	};
	const auto valid = [&] (const auto &v) { return !v || finite(*v); };
	std::size_t corners = 0;
	constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
	if (source.vertices.empty() || source.vertices.size() > limit || source.polygons.size() > limit)
		return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, "Mesh vertex or face count is invalid"});
	for (const auto &v : source.vertices)
		if (!finite(v.point) || !valid(v.normal) || !valid(v.uv) || !valid(v.color))
			return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, "Mesh contains non-finite vertex attributes"});
	for (const auto &face : source.polygons)
	{
		if (face.corners.size() < 3 || face.corners.size() > limit - corners || !valid(face.normal) || !valid(face.color))
			return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, "Invalid polygon size or attributes"});
		corners += face.corners.size();
		std::unordered_set<std::size_t> vertices;
		for (const auto &corner : face.corners)
			if (corner.vertex >= source.vertices.size() || !vertices.insert(corner.vertex).second
				|| !valid(corner.normal) || !valid(corner.uv))
				return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, "Invalid polygon index, loop or corner attributes"});
	}
	if (corners > limit / 2 || source.vertices.size() > limit - corners)
		return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, "Mesh exceeds OpenMesh handle capacity"});

	// Request only channels supplied by the source, retaining independent corner channels
	Mesh mesh;
	bool vn = false, vu = false, vc = false, hn = false, hu = false, fn = false, fc = false;
	for (const auto &v : source.vertices) {
		vn |= v.normal.has_value();
		vu |= v.uv.has_value();
		vc |= v.color.has_value();
	}
	for (const auto &f : source.polygons) {
		fn |= f.normal.has_value();
		fc |= f.color.has_value();
		for (const auto &c : f.corners) {
			hn |= c.normal.has_value();
			hu |= c.uv.has_value();
		}
	}
	if (vn) mesh.request_vertex_normals();
	if (vu) mesh.request_vertex_texcoords2D();
	if (vc) mesh.request_vertex_colors();
	if (hn) mesh.request_halfedge_normals();
	if (hu) mesh.request_halfedge_texcoords2D();
	if (fn) mesh.request_face_normals();
	if (fc) mesh.request_face_colors();

	// Create source vertices and initialize optional storage, including absent values
	const auto addVertex = [&] (const MeshVertex &v, bool repaired) {
		auto handle = mesh.add_vertex(v.point);
		mesh.data(handle).hasNormal = v.normal.has_value();
		mesh.data(handle).hasTexCoord = v.uv.has_value();
		mesh.data(handle).hasColor = v.color.has_value();
		if (vn) mesh.set_normal(handle, v.normal.value_or(Mesh::Normal(0, 0, 0)));
		if (vu) mesh.set_texcoord2D(handle, v.uv.value_or(Mesh::TexCoord2D(0, 0)));
		if (vc) mesh.set_color(handle, v.color.value_or(Mesh::Color(0, 0, 0, 0)));
		mesh.status(handle).set_fixed_nonmanifold(repaired);
		return handle;
	};
	std::vector<Mesh::VertexHandle> handles;
	for (const auto &v : source.vertices)
		handles.push_back(addVertex(v, false));

	// OpenMesh insertion determines compatibility; supported bow-tie vertices need no special treatment
	for (std::size_t index = 0; index < source.polygons.size(); ++index)
	{
		const auto &polygon = source.polygons[index];
		std::vector<Mesh::VertexHandle> loop;
		for (const auto &corner : polygon.corners)
			loop.push_back(handles[corner.vertex]);
		auto face = mesh.add_face(loop);
		bool repaired = false;
		if (!face.is_valid() && options.topology == MeshTopologyPolicy::Repair) {
			loop.clear();
			for (const auto &corner : polygon.corners)
				loop.push_back(addVertex(source.vertices[corner.vertex], true));
			face = mesh.add_face(loop);
			repaired = true;
		}
		if (!face.is_valid())
			return std::unexpected(MeshError{MeshErrorCode::InvalidTopology,
				"OpenMesh cannot insert source face " + std::to_string(index + 1)});
		mesh.status(face).set_fixed_nonmanifold(repaired);
		mesh.data(face).hasNormal = polygon.normal.has_value();
		mesh.data(face).hasColor = polygon.color.has_value();
		if (fn) mesh.set_normal(face, polygon.normal.value_or(Mesh::Normal(0, 0, 0)));
		if (fc) mesh.set_color(face, polygon.color.value_or(Mesh::Color(0, 0, 0, 0)));

		// Match each source corner to its incoming halfedge, including after vertex duplication
		for (std::size_t i = 0; i < loop.size(); ++i)
		{
			auto halfedge = mesh.find_halfedge(loop[(i + loop.size() - 1) % loop.size()], loop[i]);
			const auto &corner = polygon.corners[i];
			mesh.data(halfedge).hasNormal = corner.normal.has_value();
			mesh.data(halfedge).hasTexCoord = corner.uv.has_value();
			if (hn) mesh.set_normal(halfedge, corner.normal.value_or(Mesh::Normal(0, 0, 0)));
			if (hu) mesh.set_texcoord2D(halfedge, corner.uv.value_or(Mesh::TexCoord2D(0, 0)));
			if (repaired) mesh.status(mesh.edge_handle(halfedge)).set_fixed_nonmanifold(true);
		}
	}

	// Boundary halfedges have no supplied corner data but must have deterministic optional storage
	for (auto halfedge : mesh.halfedges())
		if (mesh.is_boundary(halfedge)) {
			if (hn) mesh.set_normal(halfedge, Mesh::Normal(0, 0, 0));
			if (hu) mesh.set_texcoord2D(halfedge, Mesh::TexCoord2D(0, 0));
		}
	return mesh;
}

// namespace detail
}



//////
//
// Module namespace close
//

// namespace fcg
}
