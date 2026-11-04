
#ifndef __FCG_MESH_PLY_H__
#define __FCG_MESH_PLY_H__


//////
//
// Module documentation
//

/// \defgroup fcg_ply_mesh PLY meshes
/// \ingroup fcg_modules
/// \brief Loading of ASCII and binary PLY geometry with vertex, face and corner attributes.
///
/// Use \ref fcg::PlyMeshFormatHandler "<tt>PlyMeshFormatHandler</tt>" alone or through
/// \ref fcg::MeshLoader "<tt>MeshLoader</tt>" to obtain an owning \ref fcg::Mesh "<tt>Mesh</tt>".
///
/// \section fcg_ply_mesh_usage Getting started
/// \snippet mesh_examples.cpp ply
///
/// \section fcg_ply_mesh_workflows Common tasks
/// ASCII, little-endian binary and big-endian binary files are supported. Unknown properties are skipped.
/// Integer colors are normalized to their positive type maximum; floating colors retain their values.
/// Vertex UV names are u/v, s/t or texture_u/texture_v; corner UVs use the face texcoord list.



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

/// \addtogroup fcg_ply_mesh
/// @{



//////
//
// Classes
//

/// Decoder for ASCII and binary PLY geometry with vertex, face and corner attributes.
class FCG_MESH_EXPORT PlyMeshFormatHandler final : public MeshFormatHandler
{
public:

	////
	// Interface: MeshFormatHandler

	/// Advertises the PLY file extension.
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


#endif // __FCG_MESH_PLY_H__
