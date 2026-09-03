
#ifndef __FCG_HELLO_CONVEX_POLY_H__
#define __FCG_HELLO_CONVEX_POLY_H__


//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <cstdint>
#include <vector>

// GLM library
#include <glm/glm.hpp>

// Dear ImGui
#include <imgui.h>

// Local includes
#include "shapes.h"



//////
//
// Classes
//

/// A flat convex polygon on the *xy*-plane.
///
/// Vertices are equally distributed on a circle of configurable radius. The vertex layout is
/// interleaved position + normal, both as `glm::vec3`:
///
///   struct Vertex { glm::vec3 position; glm::vec3 normal; };
///
/// This will be the vertex format used by the future pipeline/shader.
class ConvexPolygon : public SimpleShape {
public:

	////
	// Construction / destruction

	/// Default constructor. Creates an equilateral triangle with radius 1.
	ConvexPolygon() = default;


	////
	// Interface: SimpleShape

	[[nodiscard]] auto name () const -> const char* override {
		return "Convex Polygon";
	}

	void gui (fcg::Player &player) override
	{
		bool changed = false;

		if (ImGui::SliderInt("Vertices", &m_numVertices, minVertices, maxVertices)) {
			changed = true;
		}

		if (ImGui::DragFloat("Radius", &m_radius, 0.01f, 0.001f, 100.0f, "%.3f")) {
			changed = true;
		}

		if (ImGui::Button("Triangle")) {
			m_numVertices = 3;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Quad")) {
			m_numVertices = 4;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Hexagon")) {
			m_numVertices = 6;
			changed = true;
		}

		if (changed) {
			markDirty();
		}
	}

	void rebuild (fcg::Device &device) override
	{
		generate();
		uploadGeometry(
			device, m_vertices.data(), m_vertices.size() * sizeof(Vertex),
			m_indices.data(), m_indices.size()
		);
		clearDirty();
	}


private:

	////
	// Constants

	static constexpr int minVertices = 3;
	static constexpr int maxVertices = 64;


	////
	// Fields

	/// Number of vertices on the polygon perimeter.
	int m_numVertices = 3;

	/// Radius of the circle the vertices lie on.
	float m_radius = 1.0f;

	/// CPU-side vertex buffer.
	std::vector<Vertex> m_vertices;

	/// CPU-side index buffer.
	std::vector<std::uint32_t> m_indices;


	////
	// Methods

	/// Recompute `m_vertices` and `m_indices` from the current parameters.
	void generate()
	{
		// Prepare storage
		m_vertices.clear();
		m_indices.clear();
		const std::size_t n = static_cast<std::size_t>(m_numVertices);
		m_vertices.reserve(n);

		// Generate vertices
		const float twoPi = 2.0f * 3.14159265358979f;
		for (std::size_t i = 0; i < n; ++i)
		{
			const float angle = twoPi * static_cast<float>(i) / static_cast<float>(n);
			m_vertices.push_back(Vertex{
				.position = glm::vec3(
					m_radius * std::cos(angle),
					m_radius * std::sin(angle),
					0.0f
				),
				.normal = glm::vec3(0.0f, 0.0f, 1.0f)
			});
		}

		// Generate indices for triangle fan: (0, i, i+1) for i = 1 .. n-2
		for (std::size_t i = 1; i + 1 < n; ++i) {
			m_indices.push_back(0);
			m_indices.push_back(static_cast<std::uint32_t>(i));
			m_indices.push_back(static_cast<std::uint32_t>(i + 1));
		}
	}
};


#endif  // __FCG_HELLO_CONVEX_POLY_H__
