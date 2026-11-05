
#ifndef __FCG_RENDER_RENDERER_OPTIONS_H__
#define __FCG_RENDER_RENDERER_OPTIONS_H__


//////
//
// Includes
//

// C++ STL
#include <optional>

// SDL3 library
#include <SDL3/SDL_gpu.h>

// GLM library
#include <glm/glm.hpp>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_primitives
/// @{



//////
//
// Structs and enums
//

/// \brief Pipeline settings shared by primitive and mesh renderers.
///
/// The settings are fixed when the renderer is created. To change them, create a new renderer.
struct RendererOptions
{
	/// Whether depth testing is enabled. Has no effect if the target has no depth attachment.
	bool depthTest = true;

	/// Whether depth writing is enabled. Has no effect if the target has no depth attachment.
	bool depthWrite = true;

	/// The comparison used for depth testing.
	SDL_GPUCompareOp depthCompare = SDL_GPU_COMPAREOP_LESS;

	/// \brief The face culling mode.
	///
	/// If empty, \c MeshRenderer, \c BoxRenderer and \c EllipsoidRenderer cull back faces; \c QuadRenderer, \c LineRenderer and
	/// \c WireBoxRenderer cull nothing. The outward sides of boxes and ellipsoids and the +Z sides of quads are
	/// their front faces. Explicit culling on lines and wire boxes applies to their stroke triangles.
	std::optional<SDL_GPUCullMode> cullMode;

	/// \brief Whether colors are blended using straight (not premultiplied) alpha.
	///
	/// When enabled, the output RGB is <tt>src.rgb * src.a + dst.rgb * (1 - src.a)</tt> and the output alpha is
	/// <tt>src.a + dst.a * (1 - src.a)</tt>. Instances are drawn in the order of the collection and are not sorted by
	/// depth. Ellipsoids contribute at most one surface point per instance at each sample.
	bool alphaBlending = false;
};

/// \brief A borrowed texture and sampler for primitive or mesh draw options.
///
/// You create both resources, and they must stay alive at least until the command buffer containing the draw has
/// been submitted.
struct PrimitiveTexture
{
	/// The texture, sampled at the texture coordinates of the primitive. Must be a 2D texture with sampler usage,
	/// created on the renderer's device. Must not be null.
	SDL_GPUTexture *texture = nullptr;

	/// The sampler used to sample <tt>texture</tt>. Must be created on the renderer's device. Must not be null.
	SDL_GPUSampler *sampler = nullptr;
};

/// \brief A directional light for simple diffuse lighting in primitive and mesh draw options.
///
/// With lighting enabled, the RGB part of each fragment color is multiplied by
/// <tt>ambient + diffuse * max(dot(n, l), 0)</tt>, where \c n is the unit normal of the surface in eye space and
/// \c l is the normalized <tt>direction</tt>. The alpha is unchanged. Boxes use the flat outward normal of each face.
/// Quads use their +Z normal on the front side and the flipped normal on the back side. Ellipsoids use their
/// outward surface normal. Meshes use their supplied or generated normals, or geometric triangle normals.
struct DirectionalLight
{
	/// Whether lighting is applied. If <tt>false</tt>, the other fields are ignored and colors are drawn unlit.
	bool enabled = false;

	/// Direction from the surface toward the light, in eye space. Must be finite and nonzero when lighting is enabled;
	/// its length does not matter. The default (0, 0, 1) points toward the viewer.
	glm::vec3 direction{0.f, 0.f, 1.f};

	/// RGB intensity of the ambient term. Must be finite when lighting is enabled.
	glm::vec3 ambient{.2f};

	/// RGB intensity of the diffuse term. Must be finite when lighting is enabled.
	glm::vec3 diffuse{.8f};
};

/// \brief Texture and lighting settings for a single <tt>PrimitiveRenderer::draw</tt> call.
struct DrawOptions {
	/// Optional texture. If set, the instance color is multiplied by the texture color.
	std::optional<PrimitiveTexture> texture;

	/// Lighting settings. Lighting is disabled by default.
	DirectionalLight lighting;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // __FCG_RENDER_RENDERER_OPTIONS_H__
