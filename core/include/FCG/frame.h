
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
#include "FCG/util.h"



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
// Structs and enums
//

/// \brief Borrowed resources for recording a frame's presentation.
///
/// All resources are valid only during the recorder invocation. Obtain scene and window-image handles anew on each
/// invocation, including after a resize; never retain them across frames.
struct PresentationContext
{
	/// The current scene texture, whose sampled colors are linear. Borrowed for this invocation only.
	const Texture &scene;

	/// The default scene sampler. Borrowed for this invocation only.
	const Sampler &sceneSampler;

	/// The command buffer to record into. Borrowed for this invocation; leave submission and cancellation to the
	/// caller.
	SDL_GPUCommandBuffer *cmdBuffer;

	/// The borrowed window image with its actual format, device and extent in pixels, discard/load and store
	/// operations. Its texture handle is valid only for this invocation.
	FullscreenTarget target;
};

/// \brief An owning, move-only recorder of GPU commands for presenting a frame.
///
/// The callable accepts <tt>const PresentationContext &</tt> and returns <tt>std::expected<void, FullscreenError></tt>.
/// Mutable and move-only captures are supported. Owned captures live until replacement, clearing or destruction;
/// captured references must remain valid while used. Default-constructed and moved-from recorders are empty.
/// Each invocation must finish every GPU pass, even on error or exception, and write display-encoded window colors.
/// Scene and target resources are borrowed only for the invocation. Leave frame completion and acquisition to the
/// caller. Pass a non-empty recorder to \c Frame::present; direct invocation of an empty recorder throws
/// \c std::bad_function_call. Callable construction and invocation exceptions propagate unchanged.
using PresentationRecorder = MoveOnlyFunction<std::expected<void, FullscreenError>(const PresentationContext &)>;



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
/// 2. Call <tt>\ref present</tt> with a recorder that writes display-encoded colors into the window image.
/// 3. Optionally draw overlays such as the GUI on top of the window image in a pass begun with
///    <tt>\ref beginOverlayRenderPass</tt> and ended with <tt>\ref endRenderPass</tt>.
/// 4. Call <tt>\ref Window::endFrame</tt>.
///
/// Overlays require successful presentation. Frame completion submits the recorded commands; without successful
/// presentation, the displayed contents are unspecified. A failed recorder permits only frame completion.
/// When using \c fcg::run, the main loop performs all these steps, and \c Applet::render records into the scene pass.
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
	/// \param device
	/// 	The device that owns the frame resources.
	/// \param presentationFormat
	/// 	The format of the acquired window image.
	/// \param sampler
	/// 	The sampler used to read the presented texture.
	/// \param extent
	/// 	The size of the window image in pixels.
	explicit Frame(
		PrivateConstructorKey, SDL_GPUCommandBuffer *commandBuffer, SDL_GPUTexture *targetTexture,
		SDL_GPUTexture *depthTexture, const Texture &scene, Device &device, SDL_GPUTextureFormat presentationFormat,
		const Sampler &sampler, glm::uvec2 extent
	)
		: m_commandBuffer(commandBuffer), presentationColorTarget(targetTexture), sceneDepthTarget(depthTexture),
		  sceneTex(scene), device(device), presentationFormat(presentationFormat), sampler(sampler), m_extent(extent)
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
		return sceneTex.handle();
	}

	/// \brief
	/// 	The scene texture that the scene pass renders into.
	///
	/// The texture stores 8 bits per channel and can be used both as a render target and for sampling. The GPU
	/// converts colors automatically, so shaders write and read linear color values in the range [0, 1]. Its format
	/// is the color format reported by \c Window::renderTargetInfo.
	///
	/// \return The scene texture, owned by the window. The reference stays valid until \c Window::endFrame.
	[[nodiscard]] auto sceneTarget () const -> const Texture& { return sceneTex; }

	/// \brief
	/// 	The window image that is shown in the window when the frame is finished.
	///
	/// Values written into it are displayed as they are, without conversion from linear colors. Its format is
	/// reported by \c Window::swapchainFormat.
	///
	/// \return The window image, owned by the window. The handle stays valid until \c Window::endFrame.
	[[nodiscard]] auto presentationTarget () const -> SDL_GPUTexture* { return presentationColorTarget; }

	/// \brief
	/// 	The depth texture used by the scene pass.
	///
	/// Its format is \c SDL_GPU_TEXTUREFORMAT_D32_FLOAT and its size equals <tt>\ref extent</tt>.
	///
	/// \return The depth texture, never \c nullptr. The handle stays valid until \c Window::endFrame.
	[[nodiscard]] auto depthTexture () const -> SDL_GPUTexture* {
		return sceneDepthTarget;
	}

	/// The size, in pixels, of the window image and of the scene and depth textures of this frame.
	[[nodiscard]] auto extent () const -> glm::uvec2 {
		return m_extent;
	}


	////
	// Methods

	/// \brief Record presentation after scene rendering and before overlays.
	///
	/// The recorder owns every pass it records and must end all passes even on error or exception. It writes final
	/// display-encoded window colors. Leave command-buffer submission, frame completion and acquisition to the caller.
	/// The recorder and context are borrowed only during this call. Obtain fresh scene and target resources each
	/// invocation, including after resize.
	///
	/// \pre Call after ending the scene pass, before presentation or overlays, with a non-empty recorder.
	///
	/// \param recorder The presentation recorder; it is invoked once and is not retained.
	///
	/// \return
	/// 	Nothing on success, the recorder's error on failure, or \c FullscreenErrorCode::InvalidState for invalid
	/// 	frame state and \c FullscreenErrorCode::InvalidArgument for an empty recorder. Invalid preconditions record
	/// 	nothing. Recorder failure rejects further scene, presentation and overlay work; finish the frame with
	/// 	\c Window::endFrame to submit without retrying presentation. Failed window contents are unspecified.
	///
	/// \throws ... Exceptions from the recorder propagate unchanged; complete the failed frame as for returned errors.
	[[nodiscard]] auto present (PresentationRecorder &recorder) -> std::expected<void, FullscreenError>;

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
	/// 	If another render pass is still active, the frame is finished, presentation has begun, or an overlay pass was begun.
	auto beginRenderPass (const glm::fvec4 &clearColor) -> SDL_GPURenderPass*;

	/// \brief
	/// 	Begin a render pass for drawing overlays, such as the GUI, on top of the window image.
	///
	/// Call after successful presentation with <tt>\ref present</tt>. The pass draws into the window image, keeping
	/// its presented contents, and has no depth texture. Pipelines used in it must therefore
	/// be created for the format reported by \c Window::swapchainFormat and without a depth attachment. End the pass
	/// with <tt>\ref endRenderPass</tt> before finishing the frame. No scene pass can be begun afterwards.
	///
	/// \return
	/// 	The render pass, or \c nullptr if SDL failed to begin it. Failures are logged to the SDL log. Do not call
	/// 	\c endRenderPass for a \c nullptr result.
	///
	/// \throws std::logic_error
	/// 	If another render pass is still active, the frame is finished, or presentation has not completed successfully.
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
	// Types

	/// Tracks presentation recording and prevents retries after recording has begun.
	enum class PresentationState
	{
		/// No presentation has been recorded.
		Pending,

		/// A recorder is currently recording.
		Recording,

		/// Presentation completed successfully.
		Presented,

		/// Presentation recording failed; only completion is permitted.
		Failed
	};


	////
	// Methods

	/// Submit the command buffer and mark the frame as finished. Throws \c std::logic_error if a render pass or
	/// presentation recorder is active. Called by \c Window::endFrame, including for unpresented and failed frames.
	void end ();


	////
	// Fields

	/// The command buffer handling render command submissions for this frame.
	SDL_GPUCommandBuffer *m_commandBuffer = nullptr;

	/// The swapchain texture of this frame.
	SDL_GPUTexture *presentationColorTarget = nullptr;

	/// The depth buffer to use for this frame's render passes.
	SDL_GPUTexture *sceneDepthTarget = nullptr;

	/// The scene texture, owned by the creating window.
	const Texture &sceneTex;

	/// The device owning the frame resources.
	Device &device;

	/// The acquired window image format.
	SDL_GPUTextureFormat presentationFormat;

	/// The window-owned sampler used for presentation.
	const Sampler &sampler;

	/// The dimensions of the acquired swapchain texture, in pixels.
	glm::uvec2 m_extent{0};

	/// Current presentation lifecycle state.
	PresentationState presentationState = PresentationState::Pending;

	/// The ongoing render pass, if any.
	SDL_GPURenderPass *renderPass = nullptr;
};




//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif  // ifndef __FCG_FRAME_H__
