
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Framework
#include <FCG/Mesh/obj.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Register an independently linked decoder during static initialization.
fcg::MeshFormatHandlerRegistration registration("registration_test", std::make_unique<fcg::ObjMeshFormatHandler>());

// Anonymous namespace end
}
