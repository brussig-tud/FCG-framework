
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstddef>
#include <expected>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Dear ImGui library
#include <imgui.h>

// FCG Framework
#include <FCG/Extras/file_filters.h>
#include <FCG/Extras/gui.h>
#include <FCG/Image/image_loader.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Fail a contract check in release builds as well as debug builds.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Check ordering, wildcard preservation and group deduplication for both format domains.
template<class Format, class Filter> void filters (Filter filter, const char *aggregate)
{
	// Advertise redundant extensions and groups, retaining the first group's name and order
	const std::vector<Format> formats{
		{"First", {"a", "b", "a"}}, {"Duplicate", {"b", "a"}}, {"Second", {"b", "c"}},
		{"Empty", {}}, {"Named wildcard", {"*"}}, {"Duplicate wildcard", {"*"}}
	};
	const auto result = filter(formats, true);
	require(result.size() == 5, "Filter count");
	require(result[0].name == aggregate && result[0].extensions == std::vector<std::string>{"a", "b", "c"},
		"Aggregate order and wildcard exclusion");
	require(result[1].name == "First" && result[1].extensions == std::vector<std::string>{"a", "b"},
		"First advertised extension group wins");
	require(result[2].name == "Second" && result[3].name == "Named wildcard" && result[4].name == "All files",
		"Individual filters and unrestricted suffix");
	require(filter(formats, false).size() == 4, "Named wildcard survives includeAllFiles=false");
	require(filter({}, false).empty(), "Empty restricted formats");
	require(filter({}, true).size() == 1, "Empty unrestricted formats");
	const std::vector<Format> wildcard{{"Any", {"*"}}};
	require(filter(wildcard, false).size() == 1, "Wildcard-only input omits aggregate");
}

/// Advertise a custom format without requiring a concrete decoder implementation.
class CustomFormat : public fcg::MeshFormatHandler
{
public:
	/// Advertise the custom extension to native file pickers.
	[[nodiscard]] auto fileFormats () const -> std::vector<fcg::MeshFileFormat> override {
		return {{"Custom geometry", {"custom"}}};
	}

	/// Decline every decoding attempt in this registry-only fixture.
	[[nodiscard]] auto accepts (std::span<const std::byte>, std::string_view) const
		-> std::expected<bool, fcg::MeshError> override { return false; }

protected:
	/// Return a diagnostic if decoding is invoked directly.
	[[nodiscard]] auto decode (std::span<const std::byte>, std::string_view, fcg::MeshLoaderOptions) const
		-> std::expected<fcg::Mesh, fcg::MeshError> override {
		return std::unexpected(fcg::MeshError{fcg::MeshErrorCode::UnsupportedFormat, "Fixture has no decoder"});
	}
};

/// Inspect sparse properties, polygon corners and deleted storage without mutating the source.
auto metadata () -> fcg::extra::MeshMetadata
{
	// Empty geometry has no bounds, even when attribute storage has been requested
	fcg::Mesh mesh;
	mesh.request_vertex_normals();
	mesh.request_vertex_colors();
	mesh.request_vertex_texcoords2D();
	mesh.request_halfedge_normals();
	mesh.request_halfedge_colors();
	mesh.request_halfedge_texcoords2D();
	mesh.request_face_normals();
	mesh.request_face_colors();
	const auto empty = fcg::extra::collectMeshMetadata(mesh);
	require(!empty.bounds && !empty.vertexCount && !empty.edgeCount && !empty.faceCount && !empty.cornerCount,
		"Empty summary");

	// Retain one quad and an isolated live vertex while removing another isolated vertex
	std::vector<fcg::Mesh::VertexHandle> vertices;
	for (const auto &point : std::array<OpenMesh::Vec3f, 4>{{{1,2,3}, {5,2,3}, {5,6,3}, {1,6,3}}})
		vertices.push_back(mesh.add_vertex(point));
	const auto face = mesh.add_face(vertices);
	require(face.is_valid(), "Fixture face");
	mesh.add_vertex({-1, 8, -2});
	const auto removed = mesh.add_vertex({1000, 1000, 1000});
	mesh.delete_vertex(removed, false);
	mesh.data(vertices[0]).hasNormal = true;
	mesh.data(vertices[1]).hasColor = true;
	mesh.data(vertices[2]).hasTexCoord = true;
	mesh.data(face).hasColor = true;
	const auto firstCorner = *mesh.fh_range(face).begin();
	mesh.data(firstCorner).hasNormal = true;
	mesh.data(firstCorner).hasTexCoord = true;
	const auto info = fcg::extra::collectMeshMetadata(mesh);
	require(info.vertexCount == 5 && info.edgeCount == 4 && info.faceCount == 1 && info.cornerCount == 4,
		"Live polygon and isolated vertex counts");
	require(info.bounds && info.bounds->minimum == glm::dvec3(-1,2,-2)
		&& info.bounds->maximum == glm::dvec3(5,8,3), "Source bounds exclude deleted vertex");
	require(info.vertices.normals == 1 && info.vertices.colors == 1 && info.vertices.texCoords == 1,
		"Sparse vertex availability");
	require(info.faces.normals == 0 && info.faces.colors == 1 && info.faces.texCoords == 0,
		"Allocated face normals remain absent");
	require(info.corners.normals == 1 && info.corners.colors == 4 && info.corners.texCoords == 1,
		"Sparse corners and dense requested corner colors");

	// Fully populated flags report complete coverage without changing the earlier sparse summary
	for (auto vertex : mesh.vertices()) {
		mesh.data(vertex).hasNormal = true;
		mesh.data(vertex).hasColor = true;
		mesh.data(vertex).hasTexCoord = true;
	}
	mesh.data(face).hasNormal = true;
	for (auto corner : mesh.fh_range(face)) {
		mesh.data(corner).hasNormal = true;
		mesh.data(corner).hasTexCoord = true;
	}
	const auto full = fcg::extra::collectMeshMetadata(mesh);
	require(full.vertices.normals == full.vertexCount && full.vertices.colors == full.vertexCount
		&& full.vertices.texCoords == full.vertexCount && full.faces.normals == full.faceCount
		&& full.corners.normals == full.cornerCount && full.corners.texCoords == full.cornerCount,
		"Full attribute coverage");

	// Deleted faces and edges do not survive collection; an earlier value remains independently owned
	mesh.delete_face(face, false);
	const auto deleted = fcg::extra::collectMeshMetadata(mesh);
	require(!deleted.faceCount && !deleted.cornerCount && !deleted.edgeCount, "Deleted topology excluded");
	mesh.set_point(vertices[0], {-100, -100, -100});
	require(info.bounds->minimum == glm::dvec3(-1,2,-2), "Summary owns its bounds");
	return info;
}

/// Render embedded and separate inspectors using an ImGui context without an SDL window or GPU.
void inspectors (const fcg::extra::MeshMetadata &info)
{
	// Build a software font atlas and disable persistence for this headless frame
	ImGui::CreateContext();
	auto &io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.DisplaySize = {1000, 800};
	unsigned char *pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	ImGui::NewFrame();
	ImGui::SetNextWindowSize({450, 700});
	ImGui::Begin("Embedded");
	ImGui::PushID("fcg::extra::displayMeshMetadata");
	ImGui::PushID(&info);
	ImGui::GetStateStorage()->SetInt(ImGui::GetID("Source attributes"), 1);
	ImGui::PopID();
	ImGui::PopID();
	ImGui::SetNextItemOpen(true);
	fcg::extra::displayMeshMetadata(info);
	ImGui::PushID("empty");
	fcg::extra::displayMeshMetadata({});
	ImGui::PopID();
	ImGui::End();

	// Exercise default, customized and closed window variants
	fcg::extra::showMeshMetadataWindow(info);
	bool open = true;
	fcg::extra::showMeshMetadataWindow(info, {.title="Custom###mesh", .open=&open,
		.initialPosition=ImVec2(450,0), .initialSize=ImVec2(450,700)});
	open = false;
	fcg::extra::showMeshMetadataWindow(info, {.title="Closed", .open=&open});
	ImGui::Render();
	require(ImGui::GetDrawData()->Valid, "Balanced ImGui frame");
	ImGui::DestroyContext();
}

// Anonymous namespace end
}



//////
//
// Function implementations
//

/// Exercise the public filter and metadata exports, including the bundled startup mesh when supplied.
auto main (int argc, char **argv) -> int
{
	try
	{
		filters<fcg::MeshFileFormat>(fcg::extra::meshFileFilters, "All supported meshes");
		filters<fcg::ImageFileFormat>(fcg::extra::imageFileFilters, "All supported images");
		fcg::MeshLoader loader;
		require(bool(loader.registerHandler("custom", std::make_unique<CustomFormat>())), "Register custom format");
		require(fcg::extra::meshFileFilters(loader.fileFormats())[1].extensions[0] == "custom", "Custom registry filter");
		inspectors(metadata());
		if (argc == 2) {
			auto mesh = fcg::MeshLoader::global().load(argv[1]);
			require(bool(mesh), "Bundled startup mesh loads with the default topology policy");
			const auto info = fcg::extra::collectMeshMetadata(*mesh);
			require(info.vertexCount == 1894 && info.faceCount == 3762 && info.bounds.has_value(), "Bundled source summary");
		}
		return 0;
	}
	catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
