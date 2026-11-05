
//////
//
// Includes
//

// C++ STL
#include <memory>

// Framework
#include <FCG/Mesh/mesh_processing.h>



//////
//
// Functions
//

/// Compiled mesh processing examples.
void meshProcessingExamples (fcg::Mesh &mesh)
{
	//! [processing]
	auto triangulated = fcg::triangulateMesh(mesh);
	if (!triangulated) return; // Present triangulated.error().message to the user
	auto normals = fcg::computeVertexNormals(mesh, fcg::vertexNormalAngleWeights);
	//! [processing]

	//! [policy]
	auto policy = fcg::makeMeshPreprocessingPolicy(fcg::triangulateEarCut,
		[scale=std::make_unique<double>(1)] (std::span<const fcg::MeshIncidentTriangle> triangles,
			std::span<double> weights) {
			fcg::vertexNormalAreaWeights(triangles, weights);
			for (auto &weight : weights) weight *= *scale;
		});
	//! [policy]
	(void)normals;
	(void)policy;
}
