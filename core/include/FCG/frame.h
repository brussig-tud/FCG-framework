
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

/// \addtogroup fcg_windows
/// @{



//////
//
// Classes
//

/// \brief
/// 	One frame of a <tt>\ref Window</tt> that is being rendered.
///
/// Frames are obtained from <tt>\ref Window::beginFrame</tt> and are owned by the window; never delete them. A frame
/// stays valid until <tt>\ref Window::endFrame</tt> is called and must not be used afterwards.
///
/// Every frame has three textures of the same size (see <tt>\ref extent</tt>):
/// - the scene texture (<tt>\ref sceneTarget</tt>), into which the 3D scene is rendered with linear colors;
/// - a depth texture (<tt>\ref depthTexture</tt>) used together with the scene texture;
/// - the window image (<tt>\ref presentationTarget</tt>), which is shown in the window.
///
/// A frame is typically recorded in this order:
/// 1. Render the scene in a pass begun with <tt>\ref beginRenderPass</tt> and ended with <tt>\ref endRenderPass</tt>.
/// 2. Optionally call <tt>\ref present</tt> to copy the scene, or another texture, into the window image.
/// 3. Optionally draw overlays such as the GUI on top of the window image in a pass begun with
///    <tt>\ref beginOverlayRenderPass</tt> and ended with <tt>\ref endRenderPass</tt>.
/// 4. Call <tt>\ref Window::endFrame</tt>.
///
/// Steps 3 and 4 present the scene automatically if \c present was not called before. When using \c fcg::run, the
/// main loop performs all these steps, and \c Applet::render records into the scene pass.
class FCG_FRAMEWORK_EXPORT Frame
{
	////
	// Friend declarations

	/// Needs to finish the frame in <tt>\ref Window::endFrame</tt>.
	friend class Window;

	/// Key restricting the public constructor to framework use; only <tt>\ref Window</tt> and \c Frame can create it.
	class PrivateConstructorKey final {
		friend Frame; friend Window;
		constexpr PrivateConstructorKey() noexcept = default;
	};


public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Create a frame from the resources of a window.
	///
	/// Called by \c Window::beginFrame; applications cannot call this constructor, since only the window can create
	/// the key. Use \c Window::beginFrame to obtain frames.
	///
	/// \pre
	/// 	All referenced resources outlive the frame.
	///
	/// \param commandBuffer
	/// 	The command buffer to record the frame's commands into.
	/// \param targetTexture
	/// 	The window image to present into.
	/// \param depthTexture
	/// 	The depth texture used by the scene pass.
	/// \param scene
	/// 	The scene texture.
	/// \param encoder
	/// 	The fullscreen pass that converts linear scene colors for display in the window image.
	/// \param sampler
	/// 	The sampler used to read the presented texture.
	/// \param extent
	/// 	The size of the window image in pixels.
	explicit Frame(
		PrivateConstructorKey, SDL_GPUCommandBuffer *commandBuffer, SDL_GPUTexture *targetTexture,
		SDL_GPUTexture *depthTexture, const Texture &scene, const FullscreenPass &encoder, const Sampler &sampler,
		glm::uvec2 extent
	)
		: m_commandBuffer(commandBuffer), m_targetTexture(targetTexture), m_depthTexture(depthTexture),
		  m_scene(scene), m_encoder(encoder), m_sampler(sampler), m_extent(extent)
	{}

	/// \brief
	/// 	Destroy the frame.
	///
	/// \pre
	/// 	The frame has been finished with \c Window::endFrame. Destroying an unfinished frame logs a critical error
	/// 	and terminates the program.
	~Frame();

	/// Not copyable.
	Frame(const Frame&) = delete;

	/// Not copy-assignable.
	auto operator= (const Frame&) -> Frame& = delete;


	////
	// Accessors

	/// \brief
	/// 	The command buffer that all commands of this frame are recorded into.
	///
	/// Use it for GPU work that belongs to this frame, such as uploads in copy passes recorded outside of a render
	/// pass. It is submitted by \c Window::endFrame; never submit or cancel it yourself.
	///
	/// \return The command buffer, valid until \c Window::endFrame.
	[[nodiscard]] auto commandBuffer () const -> SDL_GPUCommandBuffer* {
		return m_commandBuffer;
	}

	/// \brief
	/// 	The raw SDL handle of the scene texture.
	///
	/// Equivalent to <tt>sceneTarget().handle()</tt>. Shaders write linear colors into it, and clear colors are
	/// linear as well.
	///
	/// \return The scene texture handle, valid until \c Window::endFrame.
	[[nodiscard]] auto colorTarget () const -> SDL_GPUTexture* {
		return m_scene.handle();
	}

	/// \brief
	/// 	The scene texture that the scene pass renders into.
	///
	/// The texture stores 8 bits per channel and can be used both as a render target and for sampling. The GPU
	/// converts colors automatically, so shaders write and read linear color values in the range [0, 1]. Its format
	/// is the color format reported by \c Window::renderTargetInfo.
	///
	/// \return The scene texture, owned by the window. The reference stays valid until \c Window::endFrame.
	[[nodiscard]] auto sceneTarget () const -> const Texture& { return m_scene; }

	/// \brief
	/// 	The window image that is shown in the window when the frame is finished.
	///
	/// Values written into it are displayed as they are, without conversion from linear colors. Its format is
	/// reported by \c Window::swapchainFormat.
	///
	/// \return The window image, owned by the window. The handle stays valid until \c Window::endFrame.
	[[nodiscard]] auto presentationTarget () const -> SDL_GPUTexture* { return m_targetTexture; }

	/// \brief
	/// 	The depth texture used by the scene pass.
	///
	/// Its format is \c SDL_GPU_TEXTUREFORMAT_D32_FLOAT and its size equals <tt>\ref extent</tt>.
	///
	/// \return The depth texture, never \c nullptr. The handle stays valid until \c Window::endFrame.
	[[nodiscard]] auto depthTexture () const -> SDL_GPUTexture* {
		return m_depthTexture;
	}

	/// The size, in pixels, of the window image and of the scene and depth textures of this frame.
	[[nodiscard]] auto extent () const -> glm::uvec2 {
		return m_extent;
	}


	////
	// Methods

	/// \brief
	/// 	Present the scene texture in the window image.
	///
	/// Equivalent to <tt>present(sceneTarget())</tt>. Calling this is optional, since
	/// <tt>\ref beginOverlayRenderPass</tt> and \c Window::endFrame present the scene automatically if no texture was
	/// presented before.
	///
	/// \return Nothing on success, or an error as described for the overload taking a source texture.
	[[nodiscard]] auto present () -> std::expected<void, FullscreenError>;

	/// \brief
	/// 	Present the given texture in the window image.
	///
	/// Overwrites the whole window image with \c source, converted from linear colors for display. Color values are
	/// clamped to [0, 1] and alpha is copied unchanged. The texture is stretched to the window image if the sizes
	/// differ, using nearest-neighbor filtering. Call this after the scene pass has ended and before
	/// <tt>\ref beginOverlayRenderPass</tt>. Each frame can be presented only once; after a successful call, the
	/// automatic presentation by \c beginOverlayRenderPass and \c Window::endFrame is skipped. On failure, nothing is
	/// recorded.
	///
	/// \param source
	/// 	A 2D texture created with \c SDL_GPU_TEXTUREUSAGE_SAMPLER on the same device, without multisampling. Its
	/// 	sampled values are interpreted as linear colors.
	///
	/// \return
	/// 	Nothing on success, or an error with one of these codes:
	/// 	- \c FullscreenErrorCode::InvalidState if a render pass is active, the frame was already presented, or an
	/// 	  overlay pass was begun;
	/// 	- \c FullscreenErrorCode::InvalidArgument if \c source does not meet the requirements above;
	/// 	- \c FullscreenErrorCode::SDLFailure if SDL fails to begin the presentation pass.
	[[nodiscard]] auto present (const Texture &source) -> std::expected<void, FullscreenError>;

	/// \brief
	/// 	Begin a render pass that draws into the scene texture.
	///
	/// The pass clears the scene texture to \c clearColor and the depth texture to the far plane (depth value 1),
	/// ready for standard depth testing. Every call clears again, discarding what previous scene passes of this frame
	/// rendered. End the pass with <tt>\ref endRenderPass</tt> before beginning another pass.
	///
	/// \param clearColor
	/// 	The linear RGBA color to clear the scene texture with.
	///
	/// \return
	/// 	The render pass, or \c nullptr if SDL failed to begin it. Failures are logged to the SDL log. Do not call
	/// 	\c endRenderPass for a \c nullptr result.
	///
	/// \throws std::logic_error
	/// 	If another render pass is still active, or if the frame was already presented or an overlay pass was begun.
	auto beginRenderPass (const glm::fvec4 &clearColor) -> SDL_GPURenderPass*;

	/// \brief
	/// 	Begin a render pass for drawing overlays, such as the GUI, on top of the window image.
	///
	/// Presents the scene first unless a texture was presented before (see <tt>\ref present</tt>). The pass draws into
	/// the window image, keeping its presented contents, and has no depth texture. Pipelines used in it must therefore
	/// be created for the format reported by \c Window::swapchainFormat and without a depth attachment. End the pass
	/// with <tt>\ref endRenderPass</tt> before finishing the frame. No scene pass can be begun afterwards.
	///
	/// \return
	/// 	The render pass, or \c nullptr if SDL failed to begin it. Failures are logged to the SDL log. Do not call
	/// 	\c endRenderPass for a \c nullptr result.
	///
	/// \throws std::logic_error
	/// 	If another render pass is still active.
	/// \throws std::runtime_error
	/// 	If the automatic presentation of the scene fails.
	auto beginOverlayRenderPass () -> SDL_GPURenderPass*;

	/// \brief
	/// 	End the active render pass.
	///
	/// Every pass begun with <tt>\ref beginRenderPass</tt> or <tt>\ref beginOverlayRenderPass</tt> must be ended
	/// with this method before another pass can be begun or the frame can be finished with \c Window::endFrame.
	///
	/// \throws std::logic_error
	/// 	If no render pass is active.
	void endRenderPass ();


private:

	////
	// Methods

	/// Present the scene if necessary, then submit the command buffer and mark the frame as finished. Throws
	/// \c std::logic_error if a render pass is active and \c std::runtime_error if the presentation fails. Called by
	/// \c Window::endFrame.
	void end ();


	////
	// Fields

	/// The command buffer handling render command submissions for this frame.
	SDL_GPUCommandBuffer *m_commandBuffer = nullptr;

	/// The swapchain texture of this frame.
	SDL_GPUTexture *m_targetTexture = nullptr;

	/// The depth buffer to use for this frame's render passes.
	SDL_GPUTexture *m_depthTexture = nullptr;

	/// The scene texture, owned by the creating window.
	const Texture &m_scene;

	/// The window-owned fullscreen pass that converts the linear scene to the swapchain format.
	const FullscreenPass &m_encoder;

	/// The window-owned sampler used for presentation.
	const Sampler &m_sampler;

	/// The dimensions of the acquired swapchain texture, in pixels.
	glm::uvec2 m_extent{0};

	/// Whether explicit or automatic presentation has completed.
	bool m_presented = false;

	/// Whether overlay rendering has begun.
	bool m_overlay = false;

	/// The ongoing render pass, if any.
	SDL_GPURenderPass *m_renderPass = nullptr;
};



/// @}



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_FRAME_H__
