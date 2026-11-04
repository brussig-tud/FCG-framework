
#ifndef __FCG_MESH_MESH_H__
#define __FCG_MESH_MESH_H__


//////
//
// Library documentation
//

/// \page fcg_mesh_overview "Mesh library overview"
///
/// The Mesh library loads polygon geometry into OpenMesh objects for inspection and processing.
/// Link the CMake target <tt>FCG-Framework::Mesh</tt> and include <tt>\#include &lt;FCG/Mesh/mesh_loader.h&gt;</tt>.
/// Loading requires no window or GPU device.
///
/// Start with \ref fcg_mesh_loading for file and memory loading, or \ref fcg_meshes to inspect attributes
/// on a loaded \ref fcg::Mesh "<tt>Mesh</tt>". The \ref fcg_obj_mesh, \ref fcg_ply_mesh and \ref fcg_stl_mesh guides
/// describe the supported file formats.



//////
//
// Module documentation
//

/// \defgroup fcg_meshes Mesh data
/// \ingroup fcg_modules
/// \brief Polygon meshes with vertex, face and corner attributes.
///
/// A \ref fcg::Mesh "<tt>Mesh</tt>" is an owning OpenMesh polygon mesh. Faces retain their source winding and
/// polygon boundaries. Positions and normals use floating-point OpenMesh vectors; colors use floating-point RGBA.
/// Coordinates retain the source units and axes, and UV coordinates retain their source orientation.
///
/// \section fcg_meshes_usage Getting started
/// Load a mesh and inspect its vertices and face corners:
/// \snippet mesh_examples.cpp inspect
///
/// \section fcg_meshes_workflows Common tasks
/// Use OpenMesh ranges to visit vertices, faces and halfedges. Corner attributes belong to the destination vertex
/// of a halfedge. Check both the standard property availability and the corresponding flag in the element's
/// \c data before reading an attribute. Missing normals remain absent; choose flat or smooth normal generation
/// when preparing geometry for your application.
///
/// OpenMesh operations can modify the returned object directly. After editing attributes, update their availability
/// flags as well. Copying a mesh copies its geometry and attributes; handles belong to their originating mesh and
/// can be invalidated by topology edits or garbage collection.



//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// OpenMesh library
#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>

#if OM_VERSION < 0x090000
#error "FCG Mesh requires OpenMesh 9.0 or newer"
#endif



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_meshes
/// @{



//////
//
// Structs and enums
//

/// OpenMesh storage types and attribute-availability flags used by <tt>Mesh</tt>.
struct MeshTraits : OpenMesh::DefaultTraits
{
	/// RGBA components; integer file colors are normalized and floating-point file colors retain their values.
	using Color = OpenMesh::Vec4f;

	/// Availability of source attributes on a vertex; update these flags when editing attribute values.
	VertexTraits
	{
		/// Whether the vertex has a supplied normal.
		bool hasNormal = false;

		/// Whether the vertex has supplied 2D texture coordinates.
		bool hasTexCoord = false;

		/// Whether the vertex has a supplied color.
		bool hasColor = false;
	};

	/// Availability of source attributes on the destination corner of a halfedge.
	HalfedgeTraits {
		/// Whether the corner has a supplied normal.
		bool hasNormal = false;

		/// Whether the corner has supplied 2D texture coordinates.
		bool hasTexCoord = false;
	};

	/// Availability of source attributes on a face.
	FaceTraits {
		/// Whether the face has a supplied normal.
		bool hasNormal = false;

		/// Whether the face has a supplied color.
		bool hasColor = false;
	};

	/// Vertex status includes the OpenMesh repair marker.
	VertexAttributes(OpenMesh::Attributes::Status);

	/// Halfedges retain backward traversal links.
	HalfedgeAttributes(OpenMesh::Attributes::PrevHalfedge);

	/// Edge status includes the OpenMesh repair marker.
	EdgeAttributes(OpenMesh::Attributes::Status);

	/// Face status includes the OpenMesh repair marker.
	FaceAttributes(OpenMesh::Attributes::Status);
};

/// An owning OpenMesh polygon mesh with optional vertex, face and corner attributes.
using Mesh = OpenMesh::PolyMesh_ArrayKernelT<MeshTraits>;



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // __FCG_MESH_MESH_H__
