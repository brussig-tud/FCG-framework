
#ifndef __FCG_HELLO_PLATONIC_SOLID_H__
#define __FCG_HELLO_PLATONIC_SOLID_H__


//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Dear ImGui
#include <imgui.h>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "shapes.h"



//////
//
// Classes
//

/// A regular convex polyhedron (platonic solid), flat-shaded.
///
/// Supports all five platonic solids (tetrahedron, cube, octahedron, dodecahedron, icosahedron), with a
/// configurable size that can be interpreted as circumradius, inradius or edge length.
class PlatonicSolid : public SimpleShape {
public:

	////
	// Types

	/// The five platonic solids.
	enum class Solid { Tetrahedron, Cube, Octahedron, Dodecahedron, Icosahedron, NUM };

	/// The way the \ref size parameter is to be interpreted.
	enum class SizeSemantics { Circumradius, Inradius, EdgeLength, NUM };


	////
	// Construction / destruction

	/// Default constructor. Creates a tetrahedron with circumradius 1.
	PlatonicSolid() = default;


	////
	// Interface: SimpleShape

	[[nodiscard]] auto name () const -> const char* override {
		return "Platonic Solid";
	}

	void gui () override
	{
		bool changed = false;

		static constexpr const char *solidNames[] = {
			"Tetrahedron", "Cube", "Octahedron", "Dodecahedron", "Icosahedron"
		};
		if (ImGui::BeginCombo("Solid", solidNames[solid])) {
			for (int i=0; i<(int)Solid::NUM; ++i) {
				const bool isSelected = (i == solid);
				if (ImGui::Selectable(solidNames[i], isSelected)) {
					solid = i;
					changed = true;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		static constexpr const char *sizeSemanticsNames[] = {
			"Circumradius", "Inradius", "Edge length"
		};
		if (ImGui::BeginCombo("Size as", sizeSemanticsNames[sizeSemantics])) {
			for (int i=0; i<(int)SizeSemantics::NUM; ++i) {
				const bool isSelected = (i == sizeSemantics);
				if (ImGui::Selectable(sizeSemanticsNames[i], isSelected)) {
					sizeSemantics = i;
					changed = true;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (ImGui::DragFloat("Size", &size, 0.01f, minSize, maxSize, "%.3f")) {
			changed = true;
		}

		if (ImGui::Button("Tetra")) {
			solid = (int)Solid::Tetrahedron;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cube")) {
			solid = (int)Solid::Cube;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Octa")) {
			solid = (int)Solid::Octahedron;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Dodeca")) {
			solid = (int)Solid::Dodecahedron;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Icosa")) {
			solid = (int)Solid::Icosahedron;
			changed = true;
		}

		if (changed) {
			markDirty();
		}
	}

	void regenerate () override
	{
		// Prepare storage
		m_vertices.clear();
		m_indices.clear();

		const SolidData &data = solidData((Solid)solid);

		// Determine the scale factor so that the chosen size measure equals `size`.
		float measurePerUnitCircumradius = 1.0f;
		switch ((SizeSemantics)sizeSemantics) {
			case SizeSemantics::Circumradius: measurePerUnitCircumradius = 1.0f; break;
			case SizeSemantics::Inradius: measurePerUnitCircumradius = data.inradiusOverCircumradius; break;
			case SizeSemantics::EdgeLength: measurePerUnitCircumradius = data.edgeOverCircumradius; break;
			default: break;
		}
		const float scale = size / measurePerUnitCircumradius;

		// Emit each face as its own set of vertices (flat shading via duplication), fan-triangulated.
		for (const auto &face : data.faces)
		{
			// Scaled corner positions.
			std::vector<glm::vec3> corners;
			corners.reserve(face.size());
			for (auto vi : face) {
				corners.push_back(data.vertices[vi] * scale);
			}

			// Compute the face normal and orient it outward (away from the origin).
			glm::vec3 normal = glm::normalize(glm::cross(corners[1]-corners[0], corners[2]-corners[0]));
			glm::vec3 centroid(0.0f);
			for (const auto &c : corners) {
				centroid += c;
			}
			centroid /= float(corners.size());
			if (glm::dot(normal, centroid) < 0.0f) {
				normal = -normal;
				std::reverse(corners.begin(), corners.end());
			}

			// Emit duplicated vertices for this face.
			const auto baseIndex = uint32_t(m_vertices.size());
			for (const auto &c : corners) {
				m_vertices.push_back(Vertex{
					.position = glm::vec4(c, 1.0f),
					.normal = glm::vec4(normal, 0.0f)
				});
			}

			// Fan-triangulate: (0, i, i+1) for i = 1..n-2.
			for (uint32_t i=1; i+1<face.size(); ++i) {
				m_indices.push_back(baseIndex);
				m_indices.push_back(baseIndex+i);
				m_indices.push_back(baseIndex+i+1);
			}
		}
	}

	[[nodiscard]] auto primitiveType () const -> SDL_GPUPrimitiveType override {
		return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	}

	[[nodiscard]] auto vertices () const -> std::span<const Vertex> override {
		return m_vertices;
	}

	[[nodiscard]] auto indices () const -> std::span<const uint32_t> override {
		return m_indices;
	}


private:

	////
	// Types

	/// Canonical (unit circumradius) geometry data for one platonic solid.
	struct SolidData {
		std::vector<glm::vec3> vertices;
		std::vector<std::vector<uint32_t>> faces;
		float inradiusOverCircumradius;
		float edgeOverCircumradius;
	};


	////
	// Constants

	static constexpr float minSize = 0.001f, maxSize = 100.0f;


	////
	// Static geometry tables

	/// Normalize a list of raw vertex positions onto the unit sphere (circumradius 1).
	static auto normalized (std::vector<glm::vec3> raw) -> std::vector<glm::vec3> {
		for (auto &v : raw) {
			v = glm::normalize(v);
		}
		return raw;
	}

	/// Lazily-built, cached canonical geometry for each platonic solid.
	static auto solidData (Solid s) -> const SolidData&
	{
		static const SolidData tetrahedron {
			.vertices = normalized({
				{ 1,  1,  1}, { 1, -1, -1}, {-1,  1, -1}, {-1, -1,  1}
			}),
			.faces = {
				{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}
			},
			.inradiusOverCircumradius = 1.0f/3.0f,
			.edgeOverCircumradius = 2.0f*std::sqrt(6.0f)/3.0f
		};

		static const SolidData cube {
			.vertices = normalized({
				{ 1,  1,  1}, { 1,  1, -1}, { 1, -1,  1}, { 1, -1, -1},
				{-1,  1,  1}, {-1,  1, -1}, {-1, -1,  1}, {-1, -1, -1}
			}),
			.faces = {
				{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 2, 6, 4},
				{1, 5, 7, 3}, {0, 4, 5, 1}, {2, 3, 7, 6}
			},
			.inradiusOverCircumradius = 1.0f/std::sqrt(3.0f),
			.edgeOverCircumradius = 2.0f/std::sqrt(3.0f)
		};

		static const SolidData octahedron {
			.vertices = normalized({
				{ 1,  0,  0}, {-1,  0,  0}, { 0,  1,  0}, { 0, -1,  0}, { 0,  0,  1}, { 0,  0, -1}
			}),
			.faces = {
				{0, 2, 4}, {0, 4, 3}, {0, 3, 5}, {0, 5, 2},
				{1, 4, 2}, {1, 3, 4}, {1, 5, 3}, {1, 2, 5}
			},
			.inradiusOverCircumradius = 1.0f/std::sqrt(3.0f),
			.edgeOverCircumradius = std::sqrt(2.0f)
		};

		static const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;

		static const SolidData dodecahedron {
			.vertices = normalized({
				{ 1,  1,  1}, { 1,  1, -1}, { 1, -1,  1}, { 1, -1, -1},
				{-1,  1,  1}, {-1,  1, -1}, {-1, -1,  1}, {-1, -1, -1},
				{0, 1.0f/phi, phi}, {0, 1.0f/phi, -phi}, {0, -1.0f/phi, phi}, {0, -1.0f/phi, -phi},
				{1.0f/phi, phi, 0}, {1.0f/phi, -phi, 0}, {-1.0f/phi, phi, 0}, {-1.0f/phi, -phi, 0},
				{phi, 0, 1.0f/phi}, {phi, 0, -1.0f/phi}, {-phi, 0, 1.0f/phi}, {-phi, 0, -1.0f/phi}
			}),
			.faces = {
				{0, 8, 4, 14, 12}, {0, 12, 1, 17, 16}, {0, 16, 2, 10, 8},
				{8, 10, 6, 18, 4}, {12, 14, 5, 9, 1}, {16, 17, 3, 13, 2},
				{1, 9, 11, 3, 17}, {2, 13, 15, 6, 10}, {3, 11, 7, 15, 13},
				{4, 18, 19, 5, 14}, {5, 19, 7, 11, 9}, {6, 15, 7, 19, 18}
			},
			.inradiusOverCircumradius = 0.7946544723f,
			.edgeOverCircumradius = 0.7136440765f
		};

		static const float t = phi;
		static const SolidData icosahedron {
			.vertices = normalized({
				{-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
				{ 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
				{ t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
			}),
			.faces = {
				{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
				{1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
				{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
				{4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
			},
			.inradiusOverCircumradius = 0.7946544723f,
			.edgeOverCircumradius = 1.0514622242f
		};

		switch (s) {
			case Solid::Tetrahedron:  return tetrahedron;
			case Solid::Cube:         return cube;
			case Solid::Octahedron:   return octahedron;
			case Solid::Dodecahedron: return dodecahedron;
			case Solid::Icosahedron:  return icosahedron;
			default:                  return tetrahedron;
		}
	}


	////
	// Fields

	/// The currently selected solid, as an `int` for ImGui interop (see \ref Solid).
	int solid = (int)Solid::Tetrahedron;

	/// The currently selected size semantics, as an `int` for ImGui interop (see \ref SizeSemantics).
	int sizeSemantics = (int)SizeSemantics::Circumradius;

	/// The configured size, interpreted according to \ref sizeSemantics.
	float size = 1.0f;

	/// CPU-side vertex buffer.
	std::vector<Vertex> m_vertices;

	/// CPU-side index buffer.
	std::vector<uint32_t> m_indices;
};


#endif  // __FCG_HELLO_PLATONIC_SOLID_H__
