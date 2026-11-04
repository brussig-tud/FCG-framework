
//////
//
// Includes
//

// C++ STL
#include <filesystem>
#include <span>

// Framework
#include <FCG/Mesh/obj.h>
#include <FCG/Mesh/ply.h>
#include <FCG/Mesh/stl.h>



//////
//
// Functions
//

/// Compiled examples for mesh users.
void meshExamples (const std::filesystem::path &path, std::span<const std::byte> bytes)
{
	//! [load]
	auto result = fcg::MeshLoader::global().load(path);
	if (!result)
		return; // Inspect result.error().message in the application's error UI
	//! [load]

	//! [inspect]
	fcg::Mesh &mesh = *result;
	for (auto vertex : mesh.vertices()) {
		const auto position = mesh.point(vertex);
		(void)position;
	}
	for (auto face : mesh.faces())
		for (auto corner : mesh.fh_range(face))
			if (mesh.has_halfedge_texcoords2D() && mesh.data(corner).hasTexCoord) {
				const auto uv = mesh.texcoord2D(corner);
				(void)uv;
			}
	//! [inspect]

	//! [repair]
	fcg::MeshLoaderOptions options;
	options.topology = fcg::MeshTopologyPolicy::Repair;
	auto repaired = fcg::MeshLoader::global().load(path, options);
	//! [repair]

	//! [custom]
	fcg::MeshLoader loader;
	auto registered = loader.registerHandler("my_obj", std::make_unique<fcg::ObjMeshFormatHandler>());
	if (registered) {
		auto decoded = loader.load(bytes, "obj");
		(void)decoded;
	}
	//! [custom]

	//! [obj]
	fcg::ObjMeshFormatHandler obj;
	auto objMesh = obj.load(bytes, "obj");
	//! [obj]

	//! [ply]
	fcg::PlyMeshFormatHandler ply;
	auto plyMesh = ply.load(bytes, "ply");
	//! [ply]

	//! [stl]
	fcg::StlMeshFormatHandler stl;
	auto stlMesh = stl.load(bytes, "stl");
	//! [stl]
}
