
#ifndef __FCG_FRAME_H__
#define __FCG_FRAME_H__


//////
//
// Includes
//

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/device.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUTexture;



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/** \addtogroup fcg_windows
 * @{
 */



//////
//
// Classes
//

/// State representing one frame of rendering. Basically scopes all render commands to this frame's lifetime. Everything
/// that has been recorded by the time the \c Frame is destroyed will get submitted to the <code>\ref fcg::Device</code>
/// this
/// \c Frame was created with.
class FCG_FRAMEWORK_EXPORT Frame
{
	////
	// Friend declarations

	// For creating a frame rendering to a window's swapchain texture
	friend class Window;

	/// Zero-overhead key to access our pseudo-private constructors. Pseudo-private because we don't want them used
	/// outside our own internals, but they have to be public because otherwise they can't be used by STL functions
	/// which we use internally (like \c std::make_optional). WHY C++??? WHYYYYYYY??????!?!?!!!11
	class PrivateConstructorKey final {
		friend Frame; friend Window;
		constexpr PrivateConstructorKey() noexcept = default;
	};


public:

	////
	// Object construction/destruction

	/// Construct with the provided command buffer and target/depth textures (pseudo-private, for internal use
	/// only).
	explicit Frame (PrivateConstructorKey, SDL_GPUCommandBuffer *commandBuffer, SDL_GPUTexture *targetTexture,
	                SDL_GPUTexture *depthTexture)
		: m_commandBuffer(commandBuffer), m_targetTexture(targetTexture), m_depthTexture(depthTexture)
	{}

	/// The destructor. Causes the associated command buffer to be cancelled. If the rendering commands are to be
	/// submitted, then this has to happen explicitly by calling the creating window's
	/// <code>\ref fcg::Window::endFrame</code> method.
	~Frame();

	/// A \c Frame is not copyable.
	Frame(const Frame&) = delete;

	/// A \c Frame is not copy-assignable.
	auto operator= (const Frame&) -> Frame& = delete;


	////
	// Accessors

	/// The command buffer of this frame. Needs to be passed to some GPU API functions that must be called outside
	/// of a render pass (e.g. the ImGui SDL GPU backend for uploading GUI vertex/index data).
	[[nodiscard]] auto commandBuffer () const -> SDL_GPUCommandBuffer* {
		return m_commandBuffer;
	}

	/// The frame's color target, if any.
	[[nodiscard]] auto colorTarget () const -> SDL_GPUTexture* {
		return m_targetTexture;
	}

	/// The frame's depth texture, if any.
	[[nodiscard]] auto depthTexture () const -> SDL_GPUTexture* {
		return m_depthTexture;
	}


	////
	// Methods

	/// Obtain a render pass targeting this frame's target texture. Render calls that should appear in this frame must
	/// be recorded into a pass obtained this way. The pass includes a depth buffer that gets cleared to the far
	/// plane (depth value 1), ready for standard depth testing.
	///
	/// Needs to be paired with a call to <code>\ref endRenderPass</code> before another pass can be begun.
	///
	/// \param clearColor The color to clear the color target of the render pass with.
	///
	/// \returns The render pass, or `nullptr` if pass creation failed.
	auto beginRenderPass (const glm::fvec4 &clearColor) -> SDL_GPURenderPass*;

	/// Obtain a render pass targeting this frame's target texture without a depth buffer. The existing color
	/// contents are preserved (load op `LOAD`), so this pass can be used to overlay the GUI on top of rendering
	/// performed in the primary <code>\ref beginRenderPass</code> pass.
	///
	/// Needs to be paired with a call to <code>\ref endRenderPass</code> before this frame is finished.
	///
	/// \returns The render pass, or `nullptr` if pass creation failed.
	auto beginOverlayRenderPass () -> SDL_GPURenderPass*;

	/// End the current render pass, marking this frame as not having a currently ongoing render pass being recorded
	/// (and thus making it safe to \link fcg::Window::endFrame end \endlink. The main reason for requiring a call to
	/// this function is to establish an explicit workflow, violations of which can be easily detected and thus avoid
	/// spurious or hard to track down bugs or crashes.
	void endRenderPass ();


private:

	////
	// Methods

	/// Marks the frame as finished. For internal use only.
	void end ();


	////
	// Fields

	/// The command buffer handling render command submissions for this frame.
	SDL_GPUCommandBuffer *m_commandBuffer = nullptr;

	/// The target texture of this frame.
	SDL_GPUTexture *m_targetTexture = nullptr;

	/// The depth buffer to use for this frame's render passes.
	SDL_GPUTexture *m_depthTexture = nullptr;

	/// The ongoing render pass, if any.
	SDL_GPURenderPass *m_renderPass = nullptr;
};



/** @} */

//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_FRAME_H__
