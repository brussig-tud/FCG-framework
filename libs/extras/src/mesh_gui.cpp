
//////
//
// Includes
//

// C++ STL
#include <format>
#include <string>

// FCG Framework
#include <FCG/Mesh/mesh_processing.h>

// Implemented header
#include "FCG/Extras/gui.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Insert a label and a wrapped value into the current two-column table.
void row (const char *label, const std::string &value) {
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::TextUnformatted(label);
	ImGui::TableNextColumn();
	ImGui::TextWrapped("%s", value.c_str());
}

/// Display available/total counts in one attribute domain.
void coverage (const fcg::extra::MeshAttributeCoverage &attributes, std::size_t total, bool texCoords=true) {
	row("Normals", std::format("{} / {}", attributes.normals, total));
	row("Colors", std::format("{} / {}", attributes.colors, total));
	if (texCoords)
		row("UV coordinates", std::format("{} / {}", attributes.texCoords, total));
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library's namespace for assorted extra utilities.
namespace fcg::extra {



//////
//
// Function implementations
//

auto collectMeshMetadata (const Mesh &mesh) -> MeshMetadata
{
	// Count live vertices and their sparse attributes, retaining source coordinates
	using Traits = MeshAttributeTraits<Mesh>;
	MeshMetadata result;
	for (auto it = mesh.vertices_sbegin(); it != mesh.vertices_end(); ++it)
	{
		const auto &point = mesh.point(*it);
		const glm::dvec3 position(point[0], point[1], point[2]);
		if (!result.bounds)
			result.bounds = MeshBounds{position, position};
		else {
			result.bounds->minimum = glm::min(result.bounds->minimum, position);
			result.bounds->maximum = glm::max(result.bounds->maximum, position);
		}
		++result.vertexCount;
		result.vertices.normals += Traits::hasNormal(mesh, *it);
		result.vertices.colors += Traits::hasColor(mesh, *it);
		result.vertices.texCoords += Traits::hasTexCoord(mesh, *it);
	}

	// Count live edges independently of storage slots pending garbage collection
	for (auto it = mesh.edges_sbegin(); it != mesh.edges_end(); ++it)
		++result.edgeCount;

	// Inspect only face corners, excluding the opposite boundary halfedges
	for (auto it = mesh.faces_sbegin(); it != mesh.faces_end(); ++it)
	{
		++result.faceCount;
		result.faces.normals += Traits::hasNormal(mesh, *it);
		result.faces.colors += Traits::hasColor(mesh, *it);
		for (auto corner : mesh.fh_range(*it)) {
			++result.cornerCount;
			result.corners.normals += Traits::hasNormal(mesh, corner);
			result.corners.colors += Traits::hasColor(mesh, corner);
			result.corners.texCoords += Traits::hasTexCoord(mesh, corner);
		}
	}
	return result;
}

void displayMeshMetadata (const MeshMetadata &metadata)
{
	// Keep geometry counts visible while details remain independently expandable
	ImGui::PushID("fcg::extra::displayMeshMetadata");
	ImGui::PushID(&metadata);
	if (ImGui::BeginTable("summary", 2, ImGuiTableFlags_SizingStretchProp)) {
		row("Vertices", std::format("{}", metadata.vertexCount));
		row("Edges", std::format("{}", metadata.edgeCount));
		row("Polygon faces", std::format("{}", metadata.faceCount));
		row("Face corners", std::format("{}", metadata.cornerCount));
		ImGui::EndTable();
	}

	// Present bounds in original coordinates without applying a display transform
	if (ImGui::CollapsingHeader("Source bounds"))
	{
		if (metadata.bounds && ImGui::BeginTable("bounds", 2, ImGuiTableFlags_SizingStretchProp)) {
			const auto &bounds = *metadata.bounds;
			row("Minimum", std::format("{:.6g}, {:.6g}, {:.6g}", bounds.minimum.x, bounds.minimum.y, bounds.minimum.z));
			row("Maximum", std::format("{:.6g}, {:.6g}, {:.6g}", bounds.maximum.x, bounds.maximum.y, bounds.maximum.z));
			ImGui::EndTable();
		}
		if (!metadata.bounds)
			ImGui::TextUnformatted("No live vertices");
	}

	// Separate source availability from attributes generated for GPU rendering
	if (ImGui::CollapsingHeader("Source attributes"))
	{
		ImGui::TextUnformatted("Available / total elements");
		if (ImGui::BeginTable("attributes", 2, ImGuiTableFlags_SizingStretchProp))
		{
			row("Domain", "Vertices");
			coverage(metadata.vertices, metadata.vertexCount);
			row("Domain", "Faces");
			coverage(metadata.faces, metadata.faceCount, false);
			row("Domain", "Corners");
			coverage(metadata.corners, metadata.cornerCount);
			ImGui::EndTable();
		}
	}
	ImGui::PopID();
	ImGui::PopID();
}

void showMeshMetadataWindow (const MeshMetadata &metadata, const GuiWindowOptions &options)
{
	// Keep a closed inspector dormant until the application reopens it
	if (options.open && !*options.open)
		return;

	// Seed window geometry while preserving saved settings and user adjustments
	if (options.initialPosition)
		ImGui::SetNextWindowPos(*options.initialPosition, ImGuiCond_FirstUseEver);
	if (options.initialSize)
		ImGui::SetNextWindowSize(*options.initialSize, ImGuiCond_FirstUseEver);
	const char *title = options.title ? options.title->c_str() : "Mesh information";
	if (ImGui::Begin(title, options.open, options.flags))
		displayMeshMetadata(metadata);
	ImGui::End();
}



//////
//
// Module namespace close
//

// namespace fcg::extra
}
