
#ifndef __FCG_MESH_OBJ_H__
#define __FCG_MESH_OBJ_H__


//////
//
// Module documentation
//

/// \defgroup fcg_obj_mesh OBJ meshes
/// \ingroup fcg_modules
/// \brief Loading of OBJ polygon geometry, including independently indexed corner normals and UVs.
///
/// Use \ref fcg::ObjMeshFormatHandler "<tt>ObjMeshFormatHandler</tt>" alone or through
/// \ref fcg::MeshLoader "<tt>MeshLoader</tt>" to obtain an owning \ref fcg::Mesh "<tt>Mesh</tt>".
///
/// \section fcg_obj_mesh_usage Getting started
/// \snippet mesh_examples.cpp obj
///
/// \section fcg_obj_mesh_workflows Common tasks
/// Supply an OBJ hint for memory loading. Material directives are ignored without opening other files.
/// Polygon faces and source winding are retained; point and line primitive records are ignored.



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
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_obj_mesh
/// @{



//////
//
// Classes
//

/// Decoder for OBJ polygon geometry, including independently indexed corner normals and UVs.
class FCG_MESH_EXPORT ObjMeshFormatHandler final : public MeshFormatHandler
{
public:

	////
	// Interface: MeshFormatHandler

	/// Advertises the OBJ file extension.
	[[nodiscard]] auto fileFormats () const -> std::vector<MeshFileFormat> override;

	/// Checks the signature or extension hint to decide whether decoding should be attempted.
	///
	/// \param bytes Encoded contents borrowed for this call.
	/// \param hint Extension hint; ASCII case and a leading dot are ignored.
	/// \return Whether the input is a candidate for this decoder.
	[[nodiscard]] auto accepts (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<bool, MeshError> override;


protected:

	////
	// Interface: MeshFormatHandler

	/// Decodes source geometry and applies the requested construction policy.
	///
	/// \param bytes Nonempty encoded contents borrowed for this call.
	/// \param hint Normalized lowercase extension without a dot.
	/// \param options Construction policy.
	/// \return An owned mesh or a format, decoding or topology error.
	[[nodiscard]] auto decode (
		std::span<const std::byte> bytes, std::string_view hint, MeshLoaderOptions options
	) const -> std::expected<Mesh, MeshError> override;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // __FCG_MESH_OBJ_H__
