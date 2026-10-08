
#ifndef __FCG_FRAME_H__
#define __FCG_FRAME_H__


//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/device.h"
#include "FCG/fullscreen.h"



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

/// One window frame with distinct linear scene recording, SDR presentation, and GUI overlays.
/// Finish through \c Window::endFrame before destruction; all borrowed window resources outlive this frame.
class FCG_FRAMEWORK_EXPORT Frame
{
	////
	// Friend declarations

	/// The window supplies scene and swapchain resources and controls submission.
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
	explicit Frame(
		PrivateConstructorKey, SDL_GPUCommandBuffer *commandBuffer, SDL_GPUTexture *targetTexture,
		SDL_GPUTexture *depthTexture, const Texture &scene, const FullscreenPass &encoder, const Sampler &sampler,
		glm::uvec2 extent
	)
		: m_commandBuffer(commandBuffer), m_targetTexture(targetTexture), m_depthTexture(depthTexture),
		  m_scene(scene), m_encoder(encoder), m_sampler(sampler), m_extent(extent)
	{}

	/// Destroy a completed frame. An unfinished frame is a fatal lifetime error; finish via \c Window::endFrame.
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

	/// Borrow the scene color target; scene shader outputs and clear colors are linear.
	[[nodiscard]] auto colorTarget () const -> SDL_GPUTexture* {
		return m_scene.handle();
	}

	/// Borrow the canonical sRGB scene texture, sampled as linear SDR.
	[[nodiscard]] auto sceneTarget () const -> const Texture& { return m_scene; }

	/// Borrow the UNORM swapchain image used for encoded presentation and GUI.
	[[nodiscard]] auto presentationTarget () const -> SDL_GPUTexture* { return m_targetTexture; }

	/// The frame's depth texture, if any.
	[[nodiscard]] auto depthTexture () const -> SDL_GPUTexture* {
		return m_depthTexture;
	}


	////
	// Methods

	/// Encode the scene onto the swapchain. Call after scene completion and before overlays.
	/// Duplicate presentation, active passes, and presentation after overlays return errors.
	[[nodiscard]] auto present () -> std::expected<void, FullscreenError>;

	/// Present a sampled 2D texture representing linear SDR; dimensions may differ from the window.
	[[nodiscard]] auto present (const Texture &source) -> std::expected<void, FullscreenError>;

	/// Obtain a render pass targeting this frame's scene texture. Render calls that should appear in this frame must
	/// be recorded into a pass obtained this way. The pass includes a depth buffer that gets cleared to the far
	/// plane (depth value 1), ready for standard depth testing.
	///
	/// Needs to be paired with a call to <code>\ref endRenderPass</code> before another pass can be begun.
	///
	/// \param clearColor The color to clear the color target of the render pass with.
	///
	/// \returns The render pass, or `nullptr` if pass creation failed.
	auto beginRenderPass (const glm::fvec4 &clearColor) -> SDL_GPURenderPass*;

	/// Present automatically if needed, then begin a depth-free overlay pass on the UNORM swapchain.
	/// Encoded scene contents are preserved with \c SDL_GPU_LOADOP_LOAD for GUI rendering.
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

	/// Borrowed scene allocation, owned by the creating window.
	const Texture &m_scene;

	/// Reusable window-owned SDR encoder.
	const FullscreenPass &m_encoder;

	/// Reusable window-owned presentation sampler.
	const Sampler &m_sampler;

	/// Acquired swapchain dimensions.
	glm::uvec2 m_extent;

	/// Whether explicit or automatic presentation has completed.
	bool m_presented = false;

	/// Whether overlay rendering has begun.
	bool m_overlay = false;

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
