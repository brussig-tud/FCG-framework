
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
/// interleaved position + normal, both as `glm::vec4`:
///
///   struct Vertex { glm::vec4 position; glm::vec4 normal; };
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

	void gui (fcg::Device &device) override
	{
		bool changed = false;

		if (ImGui::SliderInt("Vertices", &numVertices, minVertices, maxVertices)) {
			changed = true;
		}

		if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.001f, 100.0f, "%.3f")) {
			changed = true;
		}

		if (ImGui::Button("Triangle")) {
			numVertices = 3;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Quad")) {
			numVertices = 4;
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Hexagon")) {
			numVertices = 6;
			changed = true;
		}

		if (changed) {
			rebuild(device);
		}
	}

	void rebuild (fcg::Device &device)
	{
		// Prepare storage
		vertices.clear();
		indices.clear();
		const auto n = (std::size_t)numVertices;
		vertices.reserve(n);

		// Generate vertices
		constexpr float twoPi = 2*3.14159265358979f;
		for (std::size_t i=0; i<n; ++i)
		{
			const float angle = twoPi * static_cast<float>(i) / static_cast<float>(n);
			vertices.push_back(Vertex{
				.position = glm::vec4(
					radius * std::cos(angle),
					radius * std::sin(angle),
					.0f, 1.f
				),
				.normal = glm::vec4(.0f, .0f, 1.f, .0f)
			});
		}

		// Generate indices for triangle fan: (0, i, i+1) for i = 1..n-2
		for (uint32_t i=1; i+1<n; ++i) {
			indices.push_back(0);
			indices.push_back(i);
			indices.push_back(i+1);
		}

		// Upload
		uploadGeometry(
			device, vertices.data(), vertices.size() * sizeof(Vertex),
			indices.data(), indices.size()
		);
	}


private:

	////
	// Constants

	static constexpr unsigned minVertices = 3;
	static constexpr unsigned maxVertices = 64;


	////
	// Fields

	/// Number of vertices on the polygon perimeter.
	int numVertices = 3;

	/// Radius of the circle the vertices lie on.
	float radius = 1.0f;

	/// CPU-side vertex buffer.
	std::vector<Vertex> vertices;

	/// CPU-side index buffer.
	std::vector<std::uint32_t> indices;
};


#endif  // __FCG_HELLO_CONVEX_POLY_H__
