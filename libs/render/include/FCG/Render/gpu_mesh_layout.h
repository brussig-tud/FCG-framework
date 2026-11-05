
#ifndef __FCG_RENDER_GPU_MESH_LAYOUT_H__
#define __FCG_RENDER_GPU_MESH_LAYOUT_H__


//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <functional>
#include <cstdint>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_gpu_meshes
/// @{



//////
//
// Classes
//

/// Canonical mesh vertex format; positions are always present as three floats.
class GPUMeshLayout
{
public:

	////
	// Accessors

	/// Return the canonical three-bit key, independent of object padding.
	[[nodiscard]] constexpr auto key () const -> std::uint8_t {
		return std::uint8_t(normals | (colors << 1) | (texCoords << 2));
	}

	/// Return the byte stride of the interleaved vertex format.
	[[nodiscard]] constexpr auto stride () const -> std::size_t {
		return sizeof(float) * (3 + 3 * normals + 4 * colors + 2 * texCoords);
	}

	/// Compare the complete vertex format.
	///
	/// \param other Format to compare.
	///
	/// \return Whether all channel selections match.
	[[nodiscard]] constexpr auto operator== (const GPUMeshLayout &other) const -> bool = default;


	////
	// Fields

	/// Whether three-float normals are stored after positions.
	bool normals = false;

	/// Whether four-float RGBA colors follow normals, when present.
	bool colors = false;

	/// Whether two-float UV coordinates follow colors, when present.
	bool texCoords = false;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}



//////
//
// C++ standard library specializations
//

namespace std {
	/// Hash a canonical GPU mesh layout by its channel bits.
	template <> struct hash<fcg::GPUMeshLayout>
	{
		/// Return a hash independent of padding.
		///
		/// \param layout Format to hash.
		///
		/// \return The canonical layout key.
		auto operator() (fcg::GPUMeshLayout layout) const noexcept -> std::size_t {
			return layout.key();
		}
	};
};


#endif // __FCG_RENDER_GPU_MESH_LAYOUT_H__
