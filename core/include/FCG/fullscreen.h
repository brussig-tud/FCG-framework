
#ifndef __FCG_FULLSCREEN_H__
#define __FCG_FULLSCREEN_H__


//////
//
// Includes
//

// C++ STL
#include <expected>
#include <span>
#include <string>

// Framework
#include <FCG/res.h>
#include <FCG/texture.h>



//////
//
// Namespaces open
//

namespace fcg {

/// \defgroup fcg_fullscreen Fullscreen passes
/// \ingroup fcg_components
/// \brief Reusable color-only fullscreen encoders for presentation and custom dependent passes.
///
/// <tt>\ref fcg::FullscreenPass</tt> retains a pipeline, borrows its device, and writes an oversized three-vertex
/// triangle with top-left normalized UV at fragment location zero. One single-sample output, no depth, and no blending
/// are supported. Bindings match shader resource counts exactly and are rebound on every draw. Fragment bindings use
/// SDL ordering: sampled pairs, read-only storage textures, then buffers in set 2; uniforms use set 3, consecutively.
/// Read-only storage textures use GLSL \c texture2D and samplerless <tt>texelFetch()</tt> (not writable image bindings).
///
/// <tt>\ref fcg::FullscreenPass::record</tt> validates an explicit target and rejects input/output feedback before
/// beginning a pass. <tt>\ref fcg::FullscreenPass::draw</tt> uses an opaque active SDL pass whose compatibility and
/// absence of feedback remain caller preconditions. Neither allocates frame resources, submits, or waits. Inputs may
/// differ in size from outputs, including floating-point intermediates. Generic shaders apply no color conversion.
/// \snippet texture_examples.cpp chain
///
/// Window scenes use sRGB8 attachments: scene outputs, clears, and blending are linear; storage encodes RGB and
/// sampling decodes it. <tt>\ref fcg::Frame::present</tt> separately encodes linear SDR RGB onto the UNORM swapchain
/// while preserving alpha. The main loop presents before GUI rendering. Overlay creation and frame completion supply
/// omitted presentation; explicit duplicates and presentation after overlays fail. Scene pass completion stays separate
/// so custom effects can be inserted before presentation.
///
/// Future HDR effects must begin with floating-point scene storage, keep floating-point intermediates, then tone-map
/// and encode for output. They cannot recover HDR values already clipped or quantized in this version's sRGB8 scene.
/// HDR output, production effects, and a chain manager remain future work.
///
/// \addtogroup fcg_fullscreen
/// @{




//////
//
// Structs and enums
//

/// Fullscreen encoder error categories.
enum class FullscreenErrorCode {

	/// Invalid counts, bindings, feedback, or target geometry.
	InvalidArgument,

	/// Encoder or bound resource has been moved from.
	InvalidState,

	/// Unsupported output or shader resource configuration.
	UnsupportedConfiguration,

	/// SDL shader, pipeline, or pass creation failed.
	SDLFailure
};

/// Owned fullscreen diagnostic.
struct FullscreenError {

	/// Failure category.
	FullscreenErrorCode code;

	/// Context and backend diagnostic.
	std::string message;
};

/// Ordered borrowed sampled texture and sampler.
struct FullscreenTextureBinding {

	/// Single-sample texture with sampler usage on the encoder's device.
	const Texture *texture = nullptr;

	/// Sampler on the encoder's device.
	const Sampler *sampler = nullptr;
};

/// Borrowed fragment resources; all counts must exactly match the shader declaration.
struct FullscreenBindings {

	/// Ordered texture/sampler pairs, starting at slot zero.
	std::span<const FullscreenTextureBinding> samplers;

	/// Ordered read-only graphics storage textures.
	std::span<const Texture *const> storageTextures;

	/// Ordered read-only graphics storage buffers.
	std::span<const Buffer *const> storageBuffers;

	/// Uniform blocks pushed into consecutive fragment slots. Each follows \c std140.
	std::span<const std::span<const std::byte>> uniforms;
};




//////
//
// Classes
//

/// Explicit single-sample color attachment. Raw handle metadata is a caller precondition.
struct FCG_FRAMEWORK_EXPORT FullscreenTarget
{
public:

	////
	// Object construction

	/// Describe an owned texture subresource, validating its color-target usage and geometry.
	[[nodiscard]] static auto fromTexture (
		const Texture &texture,
		Uint32 mipLevel=0,
		Uint32 layer=0,
		SDL_GPULoadOp load=SDL_GPU_LOADOP_DONT_CARE,
		SDL_GPUStoreOp store=SDL_GPU_STOREOP_STORE
	)
		-> std::expected<FullscreenTarget, FullscreenError>;


	////
	// Fields

	/// SDL color attachment, including mip, layer/depth slice, load, store, and clear operations.
	SDL_GPUColorTargetInfo color{};

	/// Actual attachment format.
	SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;

	/// Output size; may differ from every input texture.
	glm::uvec2 extent{0};

	/// Borrowed device, required even for raw targets such as swapchain images.
	Device *device = nullptr;
};


/// Reusable oversized triangle encoder. UV at fragment location zero is normalized from the top left.
/// Generic shaders perform no implicit conversion. Devices outlive the move-only encoder and its resources.
/// Draw and record allocate no frame resources, submit nothing, and never wait.
class FCG_FRAMEWORK_EXPORT FullscreenPass
{
public:

	////
	// Object construction/destruction

	/// Create a custom fragment encoder with one single-sample color output, no depth, and no blending.
	[[nodiscard]] static auto create (
		Device &device,
		const res::ShaderStageData &fragment,
		const ShaderResources &resources,
		SDL_GPUTextureFormat outputFormat
	) -> std::expected<FullscreenPass, FullscreenError>;

	/// Sample one input and write it unchanged; texture sampling still obeys the texture format.
	[[nodiscard]] static auto passthrough (Device &device, SDL_GPUTextureFormat outputFormat)
		-> std::expected<FullscreenPass, FullscreenError>;

	/// Encode linear SDR RGB as sRGB into an UNORM output, preserving straight alpha.
	[[nodiscard]] static auto linearToSRGB (Device &device, SDL_GPUTextureFormat outputFormat)
		-> std::expected<FullscreenPass, FullscreenError>;

	/// Release the retained pipeline without waiting.
	~FullscreenPass();

	/// Encoders cannot be copied.
	FullscreenPass(const FullscreenPass&) = delete;

	/// Encoders cannot be copy-assigned.
	auto operator= (const FullscreenPass&) -> FullscreenPass& = delete;

	/// Transfer pipeline ownership and empty the source.
	FullscreenPass(FullscreenPass&&) noexcept;

	/// Release previous pipeline and transfer ownership.
	auto operator= (FullscreenPass&&) noexcept -> FullscreenPass&;


	////
	// Accessors

	/// Borrowed raw graphics pipeline; null after move.
	[[nodiscard]] auto handle () const -> SDL_GPUGraphicsPipeline* { return m_pipeline; }

	/// Borrowed creating device; null after move.
	[[nodiscard]] auto device () const -> Device* { return m_device; }

	/// Pipeline output format.
	[[nodiscard]] auto outputFormat () const -> SDL_GPUTextureFormat { return m_format; }


	////
	// Methods

	/// Bind every fragment resource, push every uniform, and draw three vertices in an active pass.
	///
	/// \pre Command and pass belong to this device. Attachment format matches, with no depth, one sample,
	/// and no input/output texture feedback. SDL render passes are opaque, so these cannot be inspected.
	/// \param command Caller-owned command buffer.
	/// \param pass Active caller-owned render pass.
	/// \param extent Output viewport and scissor dimensions.
	/// \param bindings Exactly the declared resource counts.
	/// \return Validation success or an owned diagnostic; failures encode nothing.
	[[nodiscard]] auto draw (
		SDL_GPUCommandBuffer *command,
		SDL_GPURenderPass *pass,
		glm::uvec2 extent,
		const FullscreenBindings &bindings
	) const -> std::expected<void, FullscreenError>;

	/// Validate all resources and target, reject feedback, then begin, draw, and end a color-only pass.
	///
	/// \pre Command belongs to this device and has no active pass. Raw target metadata is accurate.
	[[nodiscard]] auto record (
		SDL_GPUCommandBuffer *command,
		const FullscreenTarget &target,
		const FullscreenBindings &bindings
	) const -> std::expected<void, FullscreenError>;


private:

	////
	// Object construction/destruction

	/// Adopt a successfully created pipeline.
	FullscreenPass(Device &device, SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTextureFormat format, ShaderResources resources)
		: m_device(&device), m_pipeline(pipeline), m_format(format), m_resources(resources) {}


	////
	// Methods

	/// Validate all bindings before encoding any command.
	[[nodiscard]] auto validate (const FullscreenBindings &bindings) const -> std::expected<void, FullscreenError>;


	////
	// Fields

	/// Borrowed creating device.
	Device *m_device = nullptr;

	/// Owned immutable pipeline.
	SDL_GPUGraphicsPipeline *m_pipeline = nullptr;

	/// Output format.
	SDL_GPUTextureFormat m_format;

	/// Exact fragment resource counts.
	ShaderResources m_resources;
};



/// @}



//////
//
// Namespaces close
//

} // namespace fcg


#endif // ifndef __FCG_FULLSCREEN_H__
