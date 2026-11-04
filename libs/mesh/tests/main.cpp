
//////
//
// Includes
//

// C++ STL
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

// Framework
#include <FCG/Mesh/obj.h>
#include <FCG/Mesh/ply.h>
#include <FCG/Mesh/stl.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Fail a smoke check independently of build-mode assertions.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Return a borrowed encoded byte view.
auto bytes (std::string_view text) -> std::span<const std::byte> {
	return std::as_bytes(std::span(text));
}

/// Append an integer word in the requested byte order.
void word (std::string &text, std::uint32_t value, bool big=false) {
	for (unsigned i = 0; i < 4; ++i)
		text.push_back(static_cast<char>(value >> (8 * (big ? 3 - i : i))));
}

/// Append a floating-point word in the requested byte order.
void number (std::string &text, float value, bool big=false) {
	word(text, std::bit_cast<std::uint32_t>(value), big);
}

/// Triangle geometry shared by OBJ checks.
const std::string triangle = "v 0 0 0\nv 1 0 0\nv 0 1 0\n";

/// Construct an ASCII PLY containing mixed polygon sizes, attributes, and unrelated list properties.
auto asciiPly () -> std::string {
	return "ply\nformat ascii 1.0\nelement vertex 5\nproperty float z\nproperty double x\nproperty float y\n"
		"property float red\nproperty uchar green\nproperty uchar blue\nproperty float nx\nproperty float ny\nproperty float nz\n"
		"property float u\nproperty float v\nproperty list uchar int ignored\n"
		"element face 2\nproperty list uchar float texcoord\nproperty uchar red\nproperty uchar green\nproperty uchar blue\n"
		"property float nx\nproperty float ny\nproperty float nz\nproperty list uchar int vertex_indices\nend_header\n"
		"0 0 0 0.123456 128 255 0 0 1 0 0 2 7 8\n0 1 0 0.2 0 0 0 0 1 1 0 1 9\n"
		"0 1 1 0.3 0 0 0 0 1 1 1 0\n0 0 1 0.4 0 0 0 0 1 0 1 0\n0 -1 1 0.5 0 0 0 0 1 -1 1 0\n"
		"8 0 0 1 0 1 1 0 1 255 0 0 0 0 1 4 0 1 2 3\n"
		"6 0 0 0 1 -1 1 0 255 0 0 0 1 3 0 3 4\n";
}

/// Exercise a test handler's fallback and exception contracts.
class ProbeHandler final : public fcg::MeshFormatHandler
{
public:

	////
	// Object construction

	/// Capture the dispatch trace and the probe behavior for this test handler.
	explicit ProbeHandler(std::vector<int> &order, int id, bool probeError=false, bool raises=false)
		: m_order(order), m_id(id), m_probeError(probeError), m_raises(raises) {}


	////
	// Interface: MeshFormatHandler

	/// Record dispatch and either accept, fail probing or throw as configured.
	auto accepts (std::span<const std::byte>, std::string_view) const -> std::expected<bool, fcg::MeshError> override {
		m_order.push_back(m_id);
		if (m_raises) throw std::runtime_error("propagated");
		if (m_probeError) return std::unexpected(fcg::MeshError{fcg::MeshErrorCode::DecodeFailure, "probe"});
		return true;
	}

	/// Fail decoding so that the registry must continue to another handler.
	auto decode (std::span<const std::byte>, std::string_view, fcg::MeshLoaderOptions) const
		-> std::expected<fcg::Mesh, fcg::MeshError> override {
		return std::unexpected(fcg::MeshError{fcg::MeshErrorCode::DecodeFailure, "decode"});
	}


private:

	////
	// Fields

	/// Dispatch trace owned by the test.
	std::vector<int> &m_order;

	/// Value recorded when probing.
	int m_id;

	/// Whether probing returns an expected error.
	bool m_probeError;

	/// Whether probing throws an exception.
	bool m_raises;
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run format, attribute, construction and registry regression checks.
auto main (int argc, char **argv) -> int
{
	try
	{
		// Preserve polygon and corner attributes, including seams and absent values
		fcg::ObjMeshFormatHandler obj;
		const auto source = triangle + "v -1 1 0 0.25 0.5 0.75\nvt 0 0\nvt 1 0\nvt 0 1\nvt 0.75 0.25\nvn 0 0 1\nvn 0 0 -1\n"
			"f 1/1/1 2/2/1 3/3/1\nf -4/4/2 -2//2 -1\nmtllib must-not-open.mtl\nusemtl ignored\n";
		auto mesh = obj.load(bytes(source), ".OBJ");
		require(mesh && mesh->n_vertices() == 4 && mesh->n_faces() == 2, "OBJ geometry");
		require(mesh->has_halfedge_normals() && mesh->has_halfedge_texcoords2D(), "OBJ corner channels");
		const auto first = mesh->find_halfedge(fcg::Mesh::VertexHandle(2), fcg::Mesh::VertexHandle(0));
		const auto second = mesh->find_halfedge(fcg::Mesh::VertexHandle(3), fcg::Mesh::VertexHandle(0));
		require(mesh->normal(first)[2] == 1 && mesh->normal(second)[2] == -1, "OBJ normal seam");
		require(mesh->texcoord2D(second)[0] == 0.75f && mesh->texcoord2D(first)[0] == 0, "OBJ UV seam");
		require(mesh->data(fcg::Mesh::VertexHandle(3)).hasColor && !mesh->data(fcg::Mesh::VertexHandle(0)).hasColor, "OBJ partial colors");
		const auto absent = mesh->find_halfedge(fcg::Mesh::VertexHandle(2), fcg::Mesh::VertexHandle(3));
		require(!mesh->data(absent).hasNormal && !mesh->data(absent).hasTexCoord, "OBJ partial corner availability");
		auto quad = obj.load(bytes(triangle + "v -1 0 0\nf 1 2 3 4\n"), "obj");
		require(quad && quad->n_faces() == 1 && quad->valence(fcg::Mesh::FaceHandle(0)) == 4, "OBJ polygon retention");
		auto plain = obj.load(bytes(triangle + "f 1 2 3\n"), "obj");
		require(plain && !plain->has_vertex_normals() && !plain->has_halfedge_normals() && !plain->has_face_normals(), "Missing normals stay absent");
		for (const auto &bad : {"f 1 2 9\n", "f 1 1 2\n", "f 0 1 2\n", "f -9 1 2\n", "f 1/4 2 3\n", "v nan 0 0\n", "v broken 0 0\n", "f 1 2\n"})
			require(!obj.load(bytes(triangle + bad), "obj"), "Malformed OBJ rejected");

		// Repair only failed insertions and retain supported bow-tie connectivity unchanged
		const fcg::MeshLoaderOptions repair{fcg::MeshTopologyPolicy::Repair};
		const auto conflicting = source + "f 1/4/2 2/2/1 3/3/1\n";
		auto rejected = obj.load(bytes(conflicting), "obj");
		require(!rejected && rejected.error().code == fcg::MeshErrorCode::InvalidTopology, "Conflicting face rejected");
		auto repaired = obj.load(bytes(conflicting), "obj", repair);
		require(repaired && repaired->n_faces() == 3 && repaired->n_vertices() == 7, "Conflicting face repaired");
		const fcg::Mesh::FaceHandle repairedFace(2);
		require(repaired->status(repairedFace).fixed_nonmanifold(), "Repair face marker");
		for (auto corner : repaired->fh_range(repairedFace)) {
			require(repaired->status(repaired->to_vertex_handle(corner)).fixed_nonmanifold(), "Repair vertex marker");
			require(repaired->data(corner).hasNormal && repaired->data(corner).hasTexCoord, "Repair corner attributes");
		}
		const auto bowtie = triangle + "v -1 0 0\nv 0 -1 0\nf 1 2 3\nf 1 4 5\n";
		for (auto policy : {fcg::MeshTopologyPolicy::Reject, fcg::MeshTopologyPolicy::Repair}) {
			auto result = obj.load(bytes(bowtie), "obj", {policy});
			require(result && result->n_vertices() == 5 && !result->is_manifold(fcg::Mesh::VertexHandle(0)), "Bow-tie accepted unchanged");
		}
		const auto closed = triangle + "v 0 0 1\nv -1 0 0\nv 0 -1 0\nf 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\nf 1 5 6\n";
		require(!obj.load(bytes(closed), "obj"), "Closed vertex attachment rejected");
		auto split = obj.load(bytes(closed), "obj", repair);
		require(split && split->n_faces() == 5 && split->n_vertices() == 9, "Closed vertex attachment repaired");
		const auto edge = triangle + "v 0 -1 0\nv 0 0 1\nf 1 2 3\nf 2 1 4\nf 1 2 5\n";
		require(!obj.load(bytes(edge), "obj") && obj.load(bytes(edge), "obj", repair).has_value(), "Three incident faces policy");
		require(!obj.load(bytes(triangle + "f 1 1 2\n"), "obj", repair), "Repair does not accept malformed loops");
		auto degenerate = obj.load(bytes("v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n"), "obj");
		require(degenerate.has_value(), "Zero-area geometry accepted");

		// PLY retains mixed polygon sizes, colors and both vertex and corner attributes
		fcg::PlyMeshFormatHandler ply;
		const auto ascii = asciiPly();
		auto polygon = ply.load(bytes(ascii), "ply");
		if (!polygon) std::cerr << polygon.error().message << '\n';
		require(polygon && polygon->n_faces() == 2 && polygon->valence(fcg::Mesh::FaceHandle(0)) == 4, "PLY mixed polygons");
		require(std::abs(polygon->color(fcg::Mesh::VertexHandle(0))[0] - 0.123456f) < 1e-7f, "PLY float color precision");
		require(std::abs(polygon->color(fcg::Mesh::VertexHandle(0))[1] - 128.f / 255.f) < 1e-6f, "PLY integer color normalization");
		require(polygon->has_face_colors() && polygon->has_face_normals() && polygon->has_vertex_texcoords2D()
			&& polygon->has_halfedge_texcoords2D(), "PLY attribute channels");
		require(!ply.load(bytes(ascii.substr(0, ascii.size() - 8)), "ply"), "Truncated ASCII PLY");
		// Malformed declarations, incomplete attributes and unsupported numeric ranges must fail cleanly
		for (const auto &bad : {
			"ply\nformat unknown 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\nend_header\n0 0 0\n",
			"ply\nformat ascii 1.0\nelement vertex 999999999999\nproperty float x\nproperty float y\nproperty float z\nend_header\n0 0 0\n",
			"ply\nformat ascii 1.0\nelement vertex 1\nproperty double x\nproperty float y\nproperty float z\nend_header\n1e300 0 0\n",
			"ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\nproperty float nx\nend_header\n0 0 0 1\n",
			"ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\nelement face 1\nproperty list int int vertex_indices\nend_header\n0 0 0\n-1 0\n"})
			require(!ply.load(bytes(bad), "ply"), "Malformed PLY rejected");
		for (bool big : {false, true})
		{
			std::string binary = std::string("ply\nformat ") + (big ? "binary_big_endian" : "binary_little_endian")
				+ " 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n";
			for (float value : {0, 0, 0, 1, 0, 0, 0, 1, 0}) number(binary, value, big);
			binary.push_back(3);
			for (unsigned value : {0, 1, 2}) word(binary, value, big);
			auto decoded = fcg::MeshLoader::global().load(bytes(binary));
			require(decoded && decoded->n_faces() == 1, "Binary PLY endian support");
			binary.pop_back();
			require(!ply.load(bytes(binary), "ply"), "Truncated binary PLY");
		}

		// STL detects binary headers beginning with solid and retains supplied facet normals
		fcg::StlMeshFormatHandler stl;
		std::string binary(80, '\0');
		binary.replace(0, 5, "solid");
		word(binary, 1);
		for (float value : {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0}) number(binary, value);
		binary.append(2, '\0');
		auto binaryMesh = fcg::MeshLoader::global().load(bytes(binary));
		require(binaryMesh && binaryMesh->n_faces() == 1 && binaryMesh->normal(fcg::Mesh::FaceHandle(0))[2] == 1, "Binary STL detection and normal");
		binary.pop_back();
		require(!stl.load(bytes(binary), "stl"), "Truncated binary STL");
		const std::string asciiStl = "solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid test\n";
		auto asciiMesh = stl.load(bytes(asciiStl));
		require(asciiMesh && asciiMesh->n_faces() == 1, "ASCII STL");
		require(!stl.load(bytes(asciiStl.substr(0, asciiStl.find("endfacet")))), "Incomplete STL facet");

		// Registry dispatch, error aggregation, exception propagation, and public argument checks
		fcg::MeshLoader loader;
		require(loader.handlerCount() == 0 && !loader.load(bytes(triangle), "obj"), "Empty local registry");
		std::vector<int> order;
		require(loader.registerHandler("z", std::make_unique<ProbeHandler>(order, 3)).has_value(), "Register z");
		require(loader.registerHandler("a", std::make_unique<ProbeHandler>(order, 2, true)).has_value(), "Register a");
		require(loader.registerHandler("first", std::make_unique<ProbeHandler>(order, 1), 10).has_value(), "Register priority");
		auto failure = loader.load(bytes(triangle), "obj");
		require(!failure && order == std::vector{1, 2, 3}, "Dispatch ordering");
		require(failure.error().message == "first: decode\na (probe): probe\nz: decode", "Failure aggregation");
		require(!loader.registerHandler("z", std::make_unique<fcg::ObjMeshFormatHandler>()), "Duplicate registration");
		require(!loader.registerHandler("", std::make_unique<fcg::ObjMeshFormatHandler>()), "Empty handler ID");
		require(loader.registerHandler("obj", std::make_unique<fcg::ObjMeshFormatHandler>(), -10).has_value(), "Register fallback");
		require(loader.load(bytes(triangle + "f 1 2 3\n"), ".OBJ").has_value(), "Successful fallback");
		require(loader.fileFormats().size() == 1 && loader.removeHandler("z") && !loader.removeHandler("z"), "Registry accessors");
		require(loader.registerHandler("throw", std::make_unique<ProbeHandler>(order, 0, false, true), 20).has_value(), "Register exception");
		bool propagated = false;
		try { (void)loader.load(bytes(triangle), "obj"); } catch (const std::runtime_error&) { propagated = true; }
		require(propagated, "Custom exception propagation");
		require(!obj.load({}, "obj") && !obj.load(bytes(triangle), std::string_view("obj\0x", 5)), "Invalid bytes and hint");
		require(!obj.load(bytes(triangle), "obj", {static_cast<fcg::MeshTopologyPolicy>(99)}), "Invalid topology policy");

		// File paths retain their native encoding; loaded results outlive the encoded buffer
		require(argc == 2, "Scratch path argument");
		const std::filesystem::path scratch(argv[1]);
		std::filesystem::create_directories(scratch);
		const auto path = scratch / "mesh-\xc3\xa4.OBJ";
		/* Write an encoded file for the path overload */ {
			std::ofstream file(path, std::ios::binary);
			file << source;
		}
		auto fromFile = fcg::MeshLoader::global().load(path);
		require(fromFile && fromFile->n_faces() == 2, "File loading");
		require(!fcg::MeshLoader::global().load(scratch / "missing.obj"), "Missing file");
		require(!fcg::MeshLoader::global().load(std::filesystem::path{}), "Empty path");
		std::cout << "Mesh checks passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
