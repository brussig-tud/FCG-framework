
#ifndef __FCG_HELLO_SHAPES_H__
#define __FCG_HELLO_SHAPES_H__


//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <cstdint>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include <FCG/device.h>



//////
//
// Forward declarations
//

/// Opaque SDL GPU buffer type (defined in SDL headers, deliberately not included here).
struct SDL_GPUBuffer;



//////
//
// Namespaces open
//

// Forward declarations
namespace fcg {
	class Player;
}



//////
//
// Enums
//

/// The list of implemented simple shapes
enum class SimpleShapes {
	ConvexPolygon,
	NUM
};

/// Shortcut alias for \ref SimpleShapes.
using SS = SimpleShapes;



//////
//
// Interfaces
//

/// Abstract interface of all simple shapes.
///
/// Shape implementations generate procedural geometry in CPU-side buffers and call
/// \ref uploadGeometry to move the data to the GPU. Concrete shapes only need to worry
/// about the geometry; the actual SDL GPU buffer creation/upload code is centralized
/// in the base class so it can be reused across all shapes.
class SimpleShape
{
public:

	////
	// Types

	/// Vertex layout used by all simple shapes for now.
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
	};


	////
	// Construction / destruction

	/// Virtual destructor. Releases any GPU buffers that were created via \ref uploadGeometry.
	virtual ~SimpleShape();


	////
	// Methods

	/// Human-readable name of this shape.
	[[nodiscard]] virtual auto name() const -> const char* = 0;

	/// Show the shape-specific GUI widgets in the current *Dear ImGui* window.
	/// Implementations should call \ref markDirty whenever a parameter changes.
	virtual void gui(fcg::Player &player) = 0;

	/// Re-generate the geometry and upload it to the GPU.
	/// Called by the applet whenever \ref dirty returns `true`.
	virtual void rebuild(fcg::Device &device) = 0;


	////
	// Accessors

	/// Whether the shape's parameters have changed since the last \ref rebuild.
	[[nodiscard]] auto dirty() const -> bool {
		return m_dirty;
	}

	/// The GPU vertex buffer, or `nullptr` if \ref rebuild has not been called successfully yet.
	[[nodiscard]] auto vertexBuffer() const -> SDL_GPUBuffer* {
		return m_vertexBuffer;
	}

	/// The GPU index buffer, or `nullptr` if \ref rebuild has not been called successfully yet.
	[[nodiscard]] auto indexBuffer() const -> SDL_GPUBuffer* {
		return m_indexBuffer;
	}

	/// Number of indices currently stored in \ref indexBuffer.
	[[nodiscard]] auto numIndices() const -> std::size_t {
		return m_numIndices;
	}


protected:

	////
	// State management

	/// Mark the shape as needing a rebuild.
	void markDirty () {
		m_dirty = true;
	}

	/// Clear the dirty flag. Implementations should call this after a successful \ref rebuild.
	void clearDirty () {
		m_dirty = false;
	}

	/// Upload vertex/index data to the GPU, replacing any buffers that were previously created.
	///
	/// This helper centralizes all SDL GPU buffer management so that concrete shapes do not need
	/// to include SDL headers or deal with transfer buffers directly.
	///
	/// \param device        The active FCG device wrapper.
	/// \param vertexData    Raw pointer to the vertex data.
	/// \param vertexDataSize Size of the vertex data in bytes.
	/// \param indexData     Pointer to an array of 32-bit indices.
	/// \param numIndices    Number of indices in \p indexData.
	void uploadGeometry (
		fcg::Device &device, const void *vertexData, std::size_t vertexDataSize, const std::uint32_t *indexData,
		std::size_t numIndices
	);


private:

	////
	// Fields

	/// Dirty flag: parameters changed, geometry needs to be regenerated.
	bool m_dirty = true;

	/// Cached GPU device handle, captured on first \ref uploadGeometry so the destructor can release
	/// buffers without requiring an explicit device reference.
	SDL_GPUDevice *m_device = nullptr;

	/// GPU vertex buffer handle.
	SDL_GPUBuffer *m_vertexBuffer = nullptr;

	/// GPU index buffer handle.
	SDL_GPUBuffer *m_indexBuffer = nullptr;

	/// Number of uploaded indices.
	std::size_t m_numIndices = 0;
};



//////
//
// Inlined class defintions
//

// The convex polygon shape.
#include "shapes/convex_poly.h"


#endif  // __FCG_HELLO_SHAPES_H__
