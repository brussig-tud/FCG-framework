
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Framework
#include <FCG/Render/gpu_mesh.h>
#include <FCG/Render/mesh_renderer.h>



//////
//
// Functions
//

/// Compiled GPU mesh and renderer workflow.
void meshRenderExamples (fcg::Device &device, const fcg::Mesh &source, fcg::RenderState &state,
	SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass, const fcg::RenderTargetInfo &target)
{
	//! [upload]
	auto policy = fcg::makeMeshPreprocessingPolicy(fcg::triangulateEarCut, fcg::vertexNormalAngleWeights);
	const fcg::GPUMeshLayout layout{.normals=true, .colors=true, .texCoords=true};
	auto mesh = fcg::GPUMesh::create(device, source, policy, {.layout=layout});
	if (!mesh) return; // Present mesh.error().message to the user
	//! [upload]

	//! [renderer]
	auto renderer = fcg::MeshRenderer::create(device, target, layout);
	if (!renderer) return; // Present renderer.error().message to the user
	fcg::MeshDrawOptions options;
	options.lighting.enabled = true;
	renderer->draw(*mesh, state, command, pass, options);
	//! [renderer]
}
