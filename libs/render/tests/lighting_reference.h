
#ifndef __FCG_TEST_LIGHTING_REFERENCE_H__
#define __FCG_TEST_LIGHTING_REFERENCE_H__


//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>

// FCG Framework
#include <FCG/Render/renderer_options.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Independent mathematical references for rendering tests.
namespace test {



//////
//
// Functions
//

/// Evaluate the specified lighting equation in double precision using half-angle dot-product identities.
inline auto referenceLighting (
	glm::dvec3 base, glm::dvec3 normal, glm::dvec3 view, const DirectionalLight &light
) -> glm::dvec3
{
	// Normalize the reference geometry independently of shader interpolants
	const auto n = glm::length(normal) > 0 ? glm::normalize(normal) : glm::dvec3(0);
	const auto l = glm::normalize(glm::dvec3(light.direction));
	const double viewLength = glm::length(view);
	const auto v = viewLength > 0 ? view / viewLength : glm::dvec3(0);
	const double lambert = std::max(0.0, glm::dot(n, l));

	// Derive the normal/half-vector cosine without constructing the shader's half-vector
	const double halfLength = std::sqrt(std::max(0.0, 2.0 + 2.0 * glm::dot(l, v)));
	double highlight = 0;
	if (lambert > 0 && viewLength > 0 && halfLength > 0)
		highlight = std::pow(std::clamp((glm::dot(n, l) + glm::dot(n, v)) / halfLength, 0.0, 1.0), light.shininess);
	return base * (glm::dvec3(light.ambient) + glm::dvec3(light.diffuse) * lambert)
		+ .25 * highlight * glm::dvec3(light.specularColor);
}

/// Quantize independently calculated linear RGB and unmodified alpha for an RGBA8 render target.
inline auto referencePixel (
	glm::vec4 base, glm::dvec3 normal, glm::dvec3 view, const DirectionalLight &light
) -> std::array<int, 4> {
	const auto rgb = referenceLighting(glm::dvec3(base), normal, view, light);
	return {int(std::lround(255 * std::clamp(rgb.r, 0.0, 1.0))),
		int(std::lround(255 * std::clamp(rgb.g, 0.0, 1.0))),
		int(std::lround(255 * std::clamp(rgb.b, 0.0, 1.0))), int(std::lround(255 * base.a))};
}



//////
//
// Namespaces close
//

// namespace test
}

// namespace fcg
}


#endif // ifndef __FCG_TEST_LIGHTING_REFERENCE_H__
