
#ifndef __FCG_HELLO_PLATONIC_SOLID_H__
#define __FCG_HELLO_PLATONIC_SOLID_H__


//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <stdexcept>
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

/// \brief A flat-shaded regular convex polyhedron (platonic solid) centered at the origin.
///
/// All five platonic solids are supported: tetrahedron, cube, octahedron, dodecahedron and icosahedron. The size is
/// configurable between 0.001 and 100 and can be interpreted as circumradius, inradius or edge length (see
/// <tt>\ref SizeSemantics "SizeSemantics"</tt>).
///
/// Every face has its own copy of its corner vertices, all carrying the outward-pointing face normal. Faces are
/// triangulated as fans and drawn as a triangle list, with counter-clockwise winding when viewed from outside.
class PlatonicSolid : public SimpleShape
{
public:

	////
	// Types

	/// Identifies one of the five platonic solids.
	enum class Solid
	{
		/// The regular tetrahedron, with 4 triangular faces.
		Tetrahedron,

		/// The cube, with 6 square faces.
		Cube,

		/// The regular octahedron, with 8 triangular faces.
		Octahedron,

		/// The regular dodecahedron, with 12 pentagonal faces.
		Dodecahedron,

		/// The regular icosahedron, with 20 triangular faces.
		Icosahedron,

		/// The number of solids. Not a valid solid.
		NUM
	};

	/// Specifies which measure of the solid the configured size refers to.
	enum class SizeSemantics
	{
		/// The size is the radius of the circumscribed sphere, i.e. the distance from the center to each vertex.
		Circumradius,

		/// The size is the radius of the inscribed sphere, i.e. the distance from the center to each face.
		Inradius,

		/// The size is the length of each edge.
		EdgeLength,

		/// The number of size semantics. Not a valid value.
		NUM
	};


	////
	// Object construction

	/// Constructs a dodecahedron with circumradius 1.
	PlatonicSolid() = default;


	////
	// Interface: SimpleShape

	/// \brief Returns the display name of this shape.
	///
	/// \return The string <tt>"Platonic Solid"</tt>.
	[[nodiscard]] auto name () const -> const char* override {
		return "Platonic Solid";
	}

	/// Shows widgets for selecting the solid, the size semantics and the size, plus buttons for each solid.
	void gui () override
	{
		bool changed = false;

		static constexpr const char *solidNames[] = {
			"Tetrahedron", "Cube", "Octahedron", "Dodecahedron", "Icosahedron"
		};
		if (ImGui::BeginCombo("Solid", solidNames[solid]))
		{
			for (int i=0; i<(int)Solid::NUM; ++i)
			{
				const bool isSelected = (i == solid);
				if (ImGui::Selectable(solidNames[i], isSelected)) {
					solid = i;
					changed = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		static constexpr const char *sizeSemanticsNames[] = {
			"Circumradius", "Inradius", "Edge length"
		};
		if (ImGui::BeginCombo("Size as", sizeSemanticsNames[sizeSemantics]))
		{
			for (int i=0; i<(int)SizeSemantics::NUM; ++i)
			{
				const bool isSelected = (i == sizeSemantics);
				if (ImGui::Selectable(sizeSemanticsNames[i], isSelected)) {
					sizeSemantics = i;
					changed = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (ImGui::DragFloat("Size", &size, 0.01f, minSize, maxSize, "%.3f"))
			changed = true;

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

		if (changed)
			markDirty();
	}

	/// Generates the solid's unique corners, flat-shaded vertices and triangle-list indices from the current parameters.
	void regenerate () override
	{
		// Prepare storage
		m_vertices.clear();
		m_indices.clear();
		m_corners.clear();

		const SolidData &data = solidData((Solid)solid);

		// Determine the scale factor so that the chosen size measure equals 'size'.
		float measurePerUnitCircumradius = 1.0f;
		switch ((SizeSemantics)sizeSemantics) {
			case SizeSemantics::Circumradius: measurePerUnitCircumradius = 1.0f; break;
			case SizeSemantics::Inradius: measurePerUnitCircumradius = data.inradiusOverCircumradius; break;
			case SizeSemantics::EdgeLength: measurePerUnitCircumradius = data.edgeOverCircumradius; break;
			default: break;
		}
		const float scale = size / measurePerUnitCircumradius;

		// Scale the canonical corners once, before duplicating them for the flat-shaded faces
		m_corners.reserve(data.vertices.size());
		for (const auto &position : data.vertices)
			m_corners.emplace_back(position * scale, 1.f);

		// Emit each face as its own set of vertices (flat shading via duplication), fan-triangulated.
		for (const auto &face : data.faces)
		{
			// Scaled corner positions.
			std::vector<glm::vec3> corners;
			corners.reserve(face.size());
			for (auto vi : face)
				corners.push_back(glm::vec3(m_corners[vi]));

			// Compute the face normal and orient it outward (away from the origin), flipping the winding along with it.
			glm::vec3 normal = glm::normalize(glm::cross(corners[1]-corners[0], corners[2]-corners[0]));
			glm::vec3 centroid(0.0f);
			for (const auto &c : corners)
				centroid += c;
			centroid /= float(corners.size());
			if (glm::dot(normal, centroid) < 0.0f) {
				normal = -normal;
				std::reverse(corners.begin(), corners.end());
			}

			// Emit duplicated vertices for this face.
			const auto baseIndex = uint32_t(m_vertices.size());
			for (const auto &c : corners)
				m_vertices.push_back(Vertex{
					.position = glm::vec4(c, 1.0f),
					.normal = glm::vec4(normal, 0.0f)
				});

			// Fan-triangulate: (0, i, i+1) for i = 1..n-2.
			for (uint32_t i=1; i+1<face.size(); ++i) {
				m_indices.push_back(baseIndex);
				m_indices.push_back(baseIndex+i);
				m_indices.push_back(baseIndex+i+1);
			}
		}
	}

	/// \brief Returns the primitive type of the solid's index data.
	///
	/// \return Always \c SDL_GPU_PRIMITIVETYPE_TRIANGLELIST.
	[[nodiscard]] auto primitiveType () const -> SDL_GPUPrimitiveType override {
		return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	}

	/// \brief Returns the vertices generated by the last call to \c regenerate.
	///
	/// \return A view that is valid until the next call to \c regenerate or destruction of the shape.
	[[nodiscard]] auto vertices () const -> std::span<const Vertex> override {
		return m_vertices;
	}

	/// \brief Returns the triangle-list indices generated by the last call to \c regenerate.
	///
	/// \return A view that is valid until the next call to \c regenerate or destruction of the shape.
	[[nodiscard]] auto indices () const -> std::span<const uint32_t> override {
		return m_indices;
	}

	/// \brief Returns the solid's unique corners generated by the last call to <tt>regenerate</tt>.
	///
	/// \return A borrowed view valid until the next regeneration or destruction of the shape.
	[[nodiscard]] auto corners () const -> std::span<const glm::vec4> override {
		return m_corners;
	}


private:

	////
	// Types

	/// Canonical geometry of one platonic solid with circumradius 1, plus the ratios needed to rescale it.
	struct SolidData
	{
		/// The corner positions, all at distance 1 from the origin.
		std::vector<glm::vec3> vertices;

		/// The faces, each given as a list of indices into \c vertices in circular order.
		std::vector<std::vector<uint32_t>> faces;

		/// The ratio of inradius to circumradius.
		float inradiusOverCircumradius;

		/// The ratio of edge length to circumradius.
		float edgeOverCircumradius;
	};


	////
	// Constants

	/// The smallest size offered by the GUI.
	static constexpr float minSize = 0.001f;

	/// The largest size offered by the GUI.
	static constexpr float maxSize = 100.0f;


	////
	// Geometry tables

	/// \brief Projects raw vertex positions onto the unit sphere, giving them circumradius 1.
	///
	/// \param raw The positions to normalize. None of them may be the zero vector.
	///
	/// \return The normalized positions.
	static auto normalized (std::vector<glm::vec3> raw) -> std::vector<glm::vec3> {
		for (auto &v : raw)
			v = glm::normalize(v);
		return raw;
	}

	/// \brief
	/// 	Returns the canonical geometry of the given solid. The tables are built on first use and kept until program
	/// 	exit.
	///
	/// \param s The solid to look up.
	///
	/// \return A reference to the static geometry table of \p s.
	///
	/// \throws std::logic_error if \p s is not a valid solid, e.g. <tt>Solid::NUM</tt>.
	static auto solidData (Solid s) -> const SolidData&
	{
		static const SolidData tetrahedron {
			.vertices = normalized({
				{ 1,  1,  1}, { 1, -1, -1}, {-1,  1, -1}, {-1, -1,  1}
			}),
			.faces = {{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}},
			.inradiusOverCircumradius = 1/3.f,
			.edgeOverCircumradius = 2*std::sqrt(6.f)/3.f
		};

		static const SolidData cube {
			.vertices = normalized({
				{ 1,  1,  1}, { 1,  1, -1}, { 1, -1,  1}, { 1, -1, -1},
				{-1,  1,  1}, {-1,  1, -1}, {-1, -1,  1}, {-1, -1, -1}
			}),
			.faces = {
				{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 2, 6, 4}, {1, 5, 7, 3}, {0, 4, 5, 1}, {2, 3, 7, 6}
			},
			.inradiusOverCircumradius = 1/std::sqrt(3.f),
			.edgeOverCircumradius = 2/std::sqrt(3.f)
		};

		static const SolidData octahedron {
			.vertices = normalized({
				{ 1,  0,  0}, {-1,  0,  0}, { 0,  1,  0}, { 0, -1,  0},
				{ 0,  0,  1}, { 0,  0, -1}
			}),
			.faces = {
				{0, 2, 4}, {0, 4, 3}, {0, 3, 5}, {0, 5, 2}, {1, 4, 2}, {1, 3, 4}, {1, 5, 3},
				{1, 2, 5}
			},
			.inradiusOverCircumradius = 1/std::sqrt(3.f),
			.edgeOverCircumradius = std::sqrt(2.f)
		};

		static const float phi = .5f*(1 + std::sqrt(5.f));

		static const SolidData dodecahedron {
			.vertices = normalized({
				{ 1,  1,  1}, { 1,  1, -1}, { 1, -1,  1}, { 1, -1, -1},
				{-1,  1,  1}, {-1,  1, -1}, {-1, -1,  1}, {-1, -1, -1},
				{0, 1/phi, phi}, {0, 1/phi, -phi}, {0, -1/phi, phi},
				{0, -1/phi, -phi}, {1/phi, phi, 0}, {1/phi, -phi, 0},
				{-1/phi, phi, 0}, {-1/phi, -phi, 0}, {phi, 0, 1/phi},
				{phi, 0, -1/phi}, {-phi, 0, 1/phi}, {-phi, 0, -1/phi}
			}),
			.faces = {
				{0, 8, 4, 14, 12}, {0, 12, 1, 17, 16}, {0, 16, 2, 10, 8}, {8, 10, 6, 18, 4},
				{12, 14, 5, 9, 1}, {16, 17, 3, 13, 2}, {1, 9, 11, 3, 17}, {2, 13, 15, 6, 10},
				{3, 11, 7, 15, 13}, {4, 18, 19, 5, 14}, {5, 19, 7, 11, 9}, {6, 15, 7, 19, 18}
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
			default:                  throw std::logic_error("Received invalid enum value");
		}
	}


	////
	// Fields

	/// The currently selected <tt>\ref Solid "Solid"</tt>, stored as an \c int for use with Dear ImGui.
	int solid = (int)Solid::Dodecahedron;

	/// The currently selected <tt>\ref SizeSemantics "SizeSemantics"</tt>, stored as an \c int for use with Dear ImGui.
	int sizeSemantics = (int)SizeSemantics::Circumradius;

	/// The configured size, interpreted according to <tt>\ref sizeSemantics "sizeSemantics"</tt>.
	float size = 1;

	/// The CPU-side vertex data.
	std::vector<Vertex> m_vertices;

	/// The CPU-side triangle-list index data.
	std::vector<uint32_t> m_indices;

	/// One homogeneous object-space position per canonical corner, scaled to the configured size.
	std::vector<glm::vec4> m_corners;
};


#endif  // __FCG_HELLO_PLATONIC_SOLID_H__
