
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Framework
#include <FCG/Mesh/mesh_loader.h>



//////
//
// Functions
//

/// Exercise registration from an independent translation unit.
auto main () -> int {
	return fcg::MeshLoader::global().removeHandler("registration_test") ? 0 : 1;
}
