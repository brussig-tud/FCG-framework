
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <memory>
#include <stdexcept>

// OpenMesh library
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>

// Framework
#include <FCG/Mesh/mesh_processing.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Fail independently of assertions.
void require (bool value, const char *message) {
	if (!value) throw std::runtime_error(message);
}

/// Construct an isolated polygon with source attributes identifying every corner.
auto polygon (std::vector<OpenMesh::Vec3d> points, bool reverse=false) -> fcg::Mesh
{
	fcg::Mesh mesh;
	std::vector<fcg::Mesh::VertexHandle> vertices;
	for (const auto &p : points) vertices.push_back(mesh.add_vertex(fcg::Mesh::Point(p[0], p[1], p[2])));
	if (reverse) std::reverse(vertices.begin(), vertices.end());
	const auto face = mesh.add_face(vertices);
	require(face.is_valid(), "Polygon insertion failed");
	mesh.request_halfedge_texcoords2D();
	mesh.request_face_colors();
	mesh.set_color(face, {0.2f, 0.3f, 0.4f, 1});
	mesh.data(face).hasColor = true;
	for (auto h : mesh.fh_range(face)) {
		mesh.set_texcoord2D(h, {float(mesh.to_vertex_handle(h).idx()), 0});
		mesh.data(h).hasTexCoord = true;
	}
	return mesh;
}

/// Verify geometry coverage, winding, properties and stable vertex identities after processing.
void triangulation ()
{
	const std::vector<OpenMesh::Vec3d> points{{0,0,0}, {2,0,0}, {2,2,0}, {1,1,0}, {0,2,0}};
	for (bool reverse : {false, true})
	{
		auto mesh = polygon(points, reverse);
		OpenMesh::FPropHandleT<int> region;
		mesh.add_property(region);
		mesh.property(region, *mesh.faces_begin()) = 37;
		require(bool(fcg::triangulateMesh(mesh)), "Concave ear cutting failed");
		require(mesh.n_faces() == 3 && mesh.n_vertices() == 5, "Triangulation changed source vertices");
		double area = 0;
		for (auto f : mesh.faces())
		{
			require(mesh.valence(f) == 3, "Non-triangle output");
			require(mesh.property(region, f) == 37 && mesh.data(f).hasColor, "Face properties lost");
			require(mesh.color(f)[1] == .3f, "Face color lost");
			std::vector<OpenMesh::Vec3d> p;
			for (auto h : mesh.fh_range(f)) {
				const auto v = mesh.to_vertex_handle(h);
				require(mesh.data(h).hasTexCoord && mesh.texcoord2D(h)[0] == v.idx(), "Corner properties lost");
				p.push_back(fcg::detail::meshPoint(mesh.point(v)));
			}
			const auto cross = OpenMesh::cross(p[1] - p[0], p[2] - p[0]);
			require(reverse ? cross[2] < 0 : cross[2] > 0, "Winding changed");
			area += cross.norm() / 2;
		}
		require(std::abs(area - 3) < 1e-8, "Concave polygon area changed");
	}

	// Preserve arbitrary callback triangle ordering and tilted polygon coordinates
	auto tilted = polygon({{0,0,0}, {1,0,1}, {2,0,2}, {2,2,2}, {0,2,0}});
	auto reversedOutput = [] (fcg::MeshFaceView face) {
		auto result = fcg::triangulateEarCut(face);
		if (result) std::reverse(result->begin(), result->end());
		return result;
	};
	require(bool(fcg::triangulateMesh(tilted, reversedOutput)), "Tilted/collinear triangulation failed");

	// Retain completed faces when a later polygon cannot be clipped
	fcg::Mesh mixed;
	for (int group = 0; group < 2; ++group) {
		std::vector<fcg::Mesh::VertexHandle> v;
		for (int i = 0; i < 4; ++i) v.push_back(mixed.add_vertex(group == 0
			? fcg::Mesh::Point(float(i == 1 || i == 2), float(i >= 2), 0)
			: fcg::Mesh::Point(float(i), 0, 0)));
		mixed.add_face(v);
	}
	const auto failed = fcg::triangulateMesh(mixed);
	require(!failed && mixed.n_faces() == 3 && mixed.valence(fcg::Mesh::FaceHandle(1)) == 4,
		"Incremental failure semantics changed");
	const auto fan = fcg::triangulateFan({points});
	require(fan && (*fan)[0] == fcg::MeshTriangle{0,1,2} && (*fan)[2] == fcg::MeshTriangle{0,3,4}, "Fan ordering");
}

/// Verify known normal directions, missing-only behavior and move-only callbacks.
void normals ()
{
	fcg::Mesh mesh;
	const auto o = mesh.add_vertex({0,0,0}), a = mesh.add_vertex({2,0,0});
	const auto b = mesh.add_vertex({0,1,0}), c = mesh.add_vertex({0,0,1});
	mesh.add_face(o, a, b);
	mesh.add_face(o, c, a);
	require(bool(fcg::computeVertexNormals(mesh, fcg::vertexNormalUniformWeights)), "Uniform normals");
	const auto n = mesh.normal(o);
	require(std::abs(n[1] - std::sqrt(.5)) < 1e-6 && std::abs(n[2] - std::sqrt(.5)) < 1e-6, "Normal average");
	mesh.set_normal(o, {1,0,0});
	mesh.data(a).hasNormal = false;
	unsigned calls = 0;
	auto weighter = [capture=std::make_unique<int>(0), &calls] (
		std::span<const fcg::MeshIncidentTriangle> triangles, std::span<double> weights
	) mutable {
		++*capture;
		++calls;
		fcg::vertexNormalAngleWeights(triangles, weights);
	};
	static_assert(!std::copy_constructible<decltype(weighter)>);
	require(bool(fcg::computeVertexNormals(mesh, weighter, fcg::VertexNormalMode::MissingOnly)), "Missing-only normals");
	require(calls == 1 && mesh.normal(o)[0] == 1 && mesh.data(a).hasNormal, "Missing-only overwrote authored normal");

	// Dense custom OpenMesh traits need no framework availability fields
	OpenMesh::TriMesh_ArrayKernelT<> generic;
	auto x = generic.add_vertex({0,0,0}), y = generic.add_vertex({1,0,0}), z = generic.add_vertex({0,1,0});
	generic.add_face(x, y, z);
	require(bool(fcg::computeVertexNormals(generic, fcg::vertexNormalAreaWeights, fcg::VertexNormalMode::MissingOnly)),
		"Generic normals failed");
	require(generic.normal(x)[2] == 1, "New dense normal storage incorrectly treated as authored");
	require(bool(fcg::triangulateMesh(generic)), "Triangle mesh triangulation should succeed");

	// Isolated and degenerate geometry yields a finite zero normal
	OpenMesh::TriMesh_ArrayKernelT<> degenerate;
	const auto d0 = degenerate.add_vertex({0,0,0}), d1 = degenerate.add_vertex({1,0,0});
	const auto d2 = degenerate.add_vertex({2,0,0}), isolated = degenerate.add_vertex({3,0,0});
	degenerate.add_face(d0, d1, d2);
	require(bool(fcg::computeVertexNormals(degenerate)), "Degenerate normal processing failed");
	require(degenerate.normal(d0).sqrnorm() == 0 && degenerate.normal(isolated).sqrnorm() == 0,
		"Undefined geometric normals should be zero");

	// Weighting schemes differ predictably on unequal incident triangles
	std::array<fcg::MeshIncidentTriangle, 2> triangles;
	triangles[0].positions = {OpenMesh::Vec3d(0,0,0), {2,0,0}, {0,2,0}};
	triangles[1].positions = {OpenMesh::Vec3d(0,0,0), {0,0,1}, {1,0,0}};
	std::array<double, 2> weights;
	fcg::vertexNormalAreaWeights(triangles, weights);
	require(weights[0] == 2 && weights[1] == .5, "Area weights");
	const auto weighted = fcg::detail::weightedMeshNormal(triangles, weights);
	require(std::abs(weighted[2] / weighted[1] - 4) < 1e-8, "Area weighted sum");
	fcg::vertexNormalAngleWeights(triangles, weights);
	require(std::abs(weights[0] - weights[1]) < 1e-8, "Angle weights");
	const auto policy = fcg::makeMeshPreprocessingPolicy(std::nullopt, std::move(weighter));
	static_assert(!std::copy_constructible<decltype(policy)>);
	require(!policy.triangulate && policy.vertexNormals.has_value(), "Independent policy options");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run processing regression tests.
auto main () -> int {
	try { triangulation(); normals(); }
	catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
	return 0;
}
