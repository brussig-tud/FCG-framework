
//////
//
// Includes
//

// C++ STL
#include <string_view>

// Framework
#include <FCG/Mesh/mesh_loader.h>



//////
//
// Functions
//

/// Exercise built-in retention without naming any concrete handler.
auto main () -> int
{
	const std::string_view inputs[] = {
		"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
		"ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\nproperty float z\nend_header\n0 0 0\n",
		"solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid test\n"
	};
	const std::string_view hints[] = {"obj", "", ""};
	for (unsigned i = 0; i < 3; ++i) {
		auto mesh = fcg::MeshLoader::global().load(std::as_bytes(std::span(inputs[i])), hints[i]);
		if (!mesh || !mesh->n_vertices())
			return 1;
	}
	return 0;
}
