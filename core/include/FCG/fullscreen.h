
#ifndef __FCG_FULLSCREEN_H__
#define __FCG_FULLSCREEN_H__


//////
//
// Module documentation
//

/// \defgroup fcg_fullscreen Fullscreen passes
/// \ingroup fcg_modules
/// \brief Shading every pixel of a texture or the window with a custom fragment shader.
///
/// A <tt>\ref fcg::FullscreenPass "FullscreenPass"</tt> runs one fragment shader over a rectangular target: use it to
/// apply a post-processing effect to the rendered scene, convert color spaces, or present the scene to the window. It
/// owns a pipeline for the given shader, and records its draws into command buffers you provide; it never submits or
/// waits. The shader receives a \c vec2 texture coordinate at input location 0, ranging from (0, 0) at the top-left to
/// (1, 1) at the bottom-right corner of the output, and writes one single-sample color output. There is no depth
/// testing or blending. Bindings must match the shader's declared resource counts exactly and are rebound on every
/// draw. Fragment bindings follow SDL's ordering: sampled texture/sampler pairs, read-only storage textures, then
/// storage buffers in set 2; uniforms use set 3, consecutively. Read-only storage textures are read with GLSL
/// \c texture2D and samplerless <tt>texelFetch()</tt>.
///
/// \section fcg_fullscreen_usage Getting started
/// The built-in \c passthrough shader copies one sampled texture to the output unchanged. Create such a pass and
/// record one draw into a command buffer:
/// \snippet texture_examples.cpp present
/// \snippet texture_examples.cpp chain
/// For a custom effect, create the pass from a fragment shader with
/// <tt>\ref fcg::FullscreenPass::create "create"</tt>. Inputs may differ in size from the output, including
/// floating-point intermediates, and the target texture must not also be bound as an input.
///
/// \section fcg_fullscreen_workflows Common tasks
/// \subsection fcg_fullscreen_workflows_present Presenting the scene
/// The built-in <tt>\ref fcg::FullscreenPass::linearToSRGB "linearToSRGB"</tt> shader converts sampled linear colors
/// to sRGB encoding. The standard application loop performs this conversion by default after all applets draw the
/// scene and before Dear ImGui rendering. Applets can replace that step with
/// <tt>\ref fcg::Player::setPresentationRecorder "Player::setPresentationRecorder"</tt>; see
/// \ref fcg_runtime_workflows_presentation for the compiled installation and reset example. A replacement must supply
/// the final display encoding itself.
///
/// In a caller-controlled frame loop, record directly into a target of your choice with
/// <tt>\ref fcg::FullscreenPass::record "record"</tt>, which begins and ends its own render pass, or into an active
/// pass with <tt>\ref fcg::FullscreenPass::draw "draw"</tt>. For a window frame, supply a presentation recorder to
/// <tt>\ref fcg::Frame::present "Frame::present"</tt> before drawing overlays. The recorder selects the conversion;
/// see \ref fcg_windows_usage for a complete sRGB presenter and frame loop.
///
/// \subsection fcg_fullscreen_workflows_chaining Effect chains
/// Chain effects by rendering each pass into an intermediate texture that the next pass samples. The compose example
/// above records two passes into one command buffer without submitting in between.
///
/// \section fcg_fullscreen_lifetime Ownership and lifetime
/// The device must outlive the pass, and target and input textures must outlive the draws that use them. The pass is
/// move-only; there is no default constructor. Destroying it never waits for the GPU. None of these objects
/// synchronizes internally.
///
/// \section fcg_fullscreen_errors Handling errors
/// Factories and recording functions return <tt>std::expected&lt;T, E&gt;</tt> with
/// <tt>E = \ref fcg::FullscreenError "FullscreenError"</tt>.
/// Invalid bindings, mismatched resource counts, extents outside 1 to 16384 pixels, uniform blocks outside 1 to 4096
/// bytes are rejected as <tt>\ref fcg::FullscreenErrorCode::InvalidArgument "InvalidArgument"</tt> without recording
/// anything. <tt>\ref fcg::FullscreenPass::record "record"</tt> also rejects target textures bound as inputs.
/// When using <tt>\ref fcg::FullscreenPass::draw "draw"</tt>, the caller must ensure that input textures differ
/// from the attachment and that the command buffer and render pass meet its compatibility preconditions. A failed
/// <tt>\ref fcg::FullscreenPass::record "record"</tt> leaves no pass open.
///
/// \see \ref fcg_textures, \ref fcg_buffers



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

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_fullscreen
/// @{



//////
//
// Structs and enums
//

/// The categories of errors reported by fullscreen pass operations.
enum class FullscreenErrorCode
{
	/// \brief
	/// 	A parameter is invalid: e.g. a null command buffer or pass, a mismatched target, an invalid or empty input
	/// 	resource binding, binding counts that differ from the shader declaration, an output extent outside 1–16384,
	/// 	a uniform block outside 1–4096 bytes, a non-fragment or empty shader, or a target texture that is also bound
	/// 	as an input.
	InvalidArgument,

	/// \brief
	/// 	The fullscreen pass is empty, the texture supplied to \c FullscreenTarget::fromTexture is empty,
	/// 	or a built-in shader is missing.
	InvalidState,

	/// The output format cannot be rendered to, or the shader declares more resources than supported.
	UnsupportedConfiguration,

	/// SDL failed to create a shader or pipeline, or to begin a render pass.
	SDLFailure
};

/// \brief
/// 	Error returned by fullscreen pass operations: a category plus a human-readable message (including SDL's message
/// 	where relevant).
struct FullscreenError
{
	/// The error category.
	FullscreenErrorCode code;

	/// A human-readable description of the failure.
	std::string message;
};

/// A texture and the sampler used to sample it, bound as one fragment shader \c sampler2D.
struct FullscreenTextureBinding
{
	/// The texture to sample. It must be single-sample, have \c SDL_GPU_TEXTUREUSAGE_SAMPLER usage and belong to the
	/// device of the fullscreen pass.
	const Texture *texture = nullptr;

	/// The sampler to use. It must belong to the device of the fullscreen pass.
	const Sampler *sampler = nullptr;
};

/// \brief
/// 	The fragment shader resources for one fullscreen draw.
///
/// The number of entries in each span must exactly match the corresponding count in the
/// <tt>\ref fcg::ShaderResources "ShaderResources"</tt> the pass was created with. Entries are bound in order,
/// starting at slot zero of their resource kind. The spans and the objects they reference only need to stay valid
/// for the duration of the <tt>\ref fcg::FullscreenPass::draw "draw"</tt> or
/// <tt>\ref fcg::FullscreenPass::record "record"</tt> call.
struct FullscreenBindings
{
	/// The texture/sampler pairs.
	std::span<const FullscreenTextureBinding> samplers;

	/// The read-only storage textures. Each must have \c SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ usage.
	std::span<const Texture *const> storageTextures;

	/// The read-only storage buffers. Each must have \c SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ usage.
	std::span<const Buffer *const> storageBuffers;

	/// The contents of the uniform blocks, pushed to consecutive fragment uniform slots starting at zero. Each block
	/// holds 1–4096 bytes laid out according to the \c std140 rules.
	std::span<const std::span<const std::byte>> uniforms;
};



//////
//
// Classes
//

/// \brief
/// 	A single-sample color attachment that a <tt>\ref fcg::FullscreenPass "FullscreenPass"</tt> can render into.
///
/// Create it from a <tt>\ref fcg::Texture "Texture"</tt> with <tt>\ref fromTexture</tt>, or fill in the fields
/// directly to render into another texture, such as a swapchain image. When filling in the fields directly, the
/// caller is responsible for their accuracy.
struct FCG_FRAMEWORK_EXPORT FullscreenTarget
{
public:

	////
	// Object construction

	/// \brief
	/// 	Describe one subresource of a texture as a render target.
	///
	/// The target references the texture without owning it. The texture must stay alive until all commands recorded
	/// with the target have completed on the GPU. When \p load is \c SDL_GPU_LOADOP_CLEAR, the clear color can be set
	/// in <tt>color.clear_color</tt> afterwards; it defaults to transparent black.
	///
	/// \param texture
	/// 	A single-sample texture with \c SDL_GPU_TEXTUREUSAGE_COLOR_TARGET usage.
	/// \param mipLevel
	/// 	The mip level to render into.
	/// \param layer
	/// 	The array layer or cube face to render into, or the depth slice for 3D textures.
	/// \param load
	/// 	What happens to the previous contents when the render pass begins.
	/// \param store
	/// 	What happens to the rendered contents when the render pass ends.
	///
	/// \return
	/// 	The target, with \c extent set to the size of the selected mip level. Returns \c InvalidState if the texture
	/// 	is empty, or \c InvalidArgument if it lacks color target usage, is multisampled, the mip level or layer is
	/// 	out of range, or the load or store operation is invalid.
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

	/// The SDL color attachment: texture, mip level, layer or depth slice, load and store operations and clear color.
	/// Resolve textures are not supported.
	SDL_GPUColorTargetInfo color{};

	/// The format of the attachment texture.
	SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID;

	/// The size of the attachment in pixels. It may differ from the sizes of the input textures.
	glm::uvec2 extent{0};

	/// The device the attachment texture belongs to.
	Device *device = nullptr;
};

/// \brief
/// 	Draws a single primitive that covers the whole output, shaded by a fragment shader of your choice.
///
/// A fullscreen pass is a reusable graphics pipeline for image-processing steps such as copying, color conversion or
/// post-processing effects. Create a pass for a custom fragment shader with <tt>\ref create</tt>, or use one of the
/// built-in passes <tt>\ref passthrough</tt> and <tt>\ref linearToSRGB</tt>. Then either let
/// <tt>\ref record</tt> begin and end a render pass for a <tt>\ref fcg::FullscreenTarget "FullscreenTarget"</tt>, or
/// add the draw to a render pass of your own with <tt>\ref draw</tt>.
///
/// The fragment shader receives a \c vec2 texture coordinate at input location 0. It ranges from (0, 0) at the
/// top-left to (1, 1) at the bottom-right corner of the output. The shader writes one color to output location 0.
/// The pipeline renders into exactly one single-sample color attachment, without depth testing or blending. No color
/// conversion is applied other than what the shader does itself and what the formats of the textures imply.
///
/// Recording never allocates per-frame resources, never submits command buffers and never waits for the GPU. A
/// moved-from pass is empty. Only assignment and destruction are valid; recording returns \c InvalidState.
class FCG_FRAMEWORK_EXPORT FullscreenPass
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Create a fullscreen pass for a custom fragment shader.
	///
	/// \pre \p device outlives the returned pass.
	///
	/// \param device
	/// 	The device on which to create the pipeline.
	/// \param fragment
	/// 	The fragment shader stage. Its data only needs to stay valid for the duration of the call.
	/// \param resources
	/// 	The resources declared by the fragment shader: at most 16 texture/sampler pairs, 8 storage textures,
	/// 	8 storage buffers and 4 uniform blocks. Every later draw must supply exactly these counts.
	/// \param outputFormat
	/// 	The format of the color attachments the pass will render into.
	///
	/// \return
	/// 	The new pass. Returns \c InvalidArgument if \p fragment is not a non-empty fragment stage,
	/// 	\c UnsupportedConfiguration if the resource counts exceed the limits or \p outputFormat is not supported as
	/// 	a 2D color target, \c InvalidState if the built-in vertex shader is missing, or \c SDLFailure if creating a
	/// 	shader or the pipeline fails.
	[[nodiscard]] static auto create (
		Device &device,
		const res::ShaderStageData &fragment,
		const ShaderResources &resources,
		SDL_GPUTextureFormat outputFormat
	) -> std::expected<FullscreenPass, FullscreenError>;

	/// \brief
	/// 	Create a built-in pass that copies one sampled texture to the output.
	///
	/// The pass expects exactly one <tt>\ref fcg::FullscreenTextureBinding "FullscreenTextureBinding"</tt> and writes
	/// the sampled value unchanged. Sampling still applies the texture format, e.g. an sRGB texture is decoded to
	/// linear values, and the output format may encode the written value again.
	///
	/// \pre \p device outlives the returned pass.
	///
	/// \param device The device on which to create the pipeline.
	/// \param outputFormat The format of the color attachments the pass will render into.
	///
	/// \return
	/// 	The new pass, \c InvalidState if a built-in shader is missing, or any error of <tt>\ref create</tt>.
	[[nodiscard]] static auto passthrough (Device &device, SDL_GPUTextureFormat outputFormat)
		-> std::expected<FullscreenPass, FullscreenError>;

	/// \brief
	/// 	Create a built-in pass that encodes one sampled linear texture as sRGB.
	///
	/// The pass expects exactly one <tt>\ref fcg::FullscreenTextureBinding "FullscreenTextureBinding"</tt>. It clamps
	/// the sampled linear RGB values to [0, 1], applies the sRGB transfer function and writes the result together with
	/// the unchanged alpha value.
	///
	/// \pre \p device outlives the returned pass.
	///
	/// \param device
	/// 	The device on which to create the pipeline.
	/// \param outputFormat
	/// 	The format of the color attachments the pass will render into. It must be
	/// 	\c SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM or \c SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM.
	///
	/// \return
	/// 	The new pass, \c UnsupportedConfiguration for any other output format, \c InvalidState if a built-in shader
	/// 	is missing, or any error of <tt>\ref create</tt>.
	[[nodiscard]] static auto linearToSRGB (Device &device, SDL_GPUTextureFormat outputFormat)
		-> std::expected<FullscreenPass, FullscreenError>;

	/// Release the pipeline. Never waits for the GPU.
	~FullscreenPass();

	/// Not copyable.
	FullscreenPass(const FullscreenPass&) = delete;

	/// Not copy-assignable.
	auto operator= (const FullscreenPass&) -> FullscreenPass& = delete;

	/// Take over the pipeline of another pass, leaving it empty.
	///
	/// \param other The pass to move from.
	FullscreenPass(FullscreenPass &&other) noexcept;

	/// Release the current pipeline and take over the pipeline of another pass, leaving it empty.
	///
	/// \param other The pass to move from.
	///
	/// \return A reference to this pass.
	auto operator= (FullscreenPass &&other) noexcept -> FullscreenPass&;


	////
	// Accessors

	/// The SDL graphics pipeline, or \c nullptr if empty. The pass keeps ownership; never release it yourself.
	[[nodiscard]] auto handle () const -> SDL_GPUGraphicsPipeline* { return m_pipeline; }

	/// The device the pass was created on, or \c nullptr if empty.
	[[nodiscard]] auto device () const -> Device* { return m_device; }

	/// The color attachment format the pass renders into.
	[[nodiscard]] auto outputFormat () const -> SDL_GPUTextureFormat { return m_format; }


	////
	// Methods

	/// \brief
	/// 	Record the fullscreen draw into an active render pass.
	///
	/// Binds the pipeline of this pass, all resources in \p bindings and the uniform blocks, sets viewport and scissor
	/// to cover \p extent, and draws. The render pass keeps the pipeline and bindings afterwards, so rebind your own
	/// state before further draws. If an error is returned, nothing has been recorded.
	///
	/// \pre
	/// 	\p command and \p pass belong to the device of this pass. The render pass has exactly one single-sample
	/// 	color attachment of format <tt>\ref outputFormat</tt> and no depth attachment. None of the input textures
	/// 	is the attachment texture.
	///
	/// \param command
	/// 	The command buffer that owns \p pass.
	/// \param pass
	/// 	The active render pass to record into.
	/// \param extent
	/// 	The size of the output in pixels, each component in 1–16384.
	/// \param bindings
	/// 	The fragment shader resources, matching the counts given at creation.
	///
	/// \return
	/// 	Nothing on success. Returns \c InvalidState if this pass is empty, or \c InvalidArgument if \p command or
	/// 	\p pass is null, \p extent is out of range, or \p bindings is invalid (see
	/// 	<tt>\ref fcg::FullscreenBindings "FullscreenBindings"</tt>).
	[[nodiscard]] auto draw (
		SDL_GPUCommandBuffer *command,
		SDL_GPURenderPass *pass,
		glm::uvec2 extent,
		const FullscreenBindings &bindings
	) const -> std::expected<void, FullscreenError>;

	/// \brief
	/// 	Record a complete render pass that draws into the given target.
	///
	/// Validates the target and bindings, then begins a render pass on \p target, records <tt>\ref draw</tt> and ends
	/// the pass. The target texture must not also be bound as an input (feedback); this is checked for sampled and
	/// storage textures regardless of the subresource.
	///
	/// \pre
	/// 	\p command belongs to the device of this pass and has no active pass. If \p target was filled in manually,
	/// 	its fields describe the attachment texture accurately.
	///
	/// \param command
	/// 	The command buffer to record into.
	/// \param target
	/// 	The color attachment to render into. Its device and format must match this pass, its extent must be in
	/// 	1–16384, and it must not specify a resolve texture.
	/// \param bindings
	/// 	The fragment shader resources, matching the counts given at creation.
	///
	/// \return
	/// 	Nothing on success. Returns \c InvalidState if this pass is empty, \c InvalidArgument if \p command is null,
	/// 	\p target does not match, \p bindings is invalid or feedback is detected, or \c SDLFailure if the render
	/// 	pass cannot be begun. Nothing is recorded on failure.
	[[nodiscard]] auto record (
		SDL_GPUCommandBuffer *command,
		const FullscreenTarget &target,
		const FullscreenBindings &bindings
	) const -> std::expected<void, FullscreenError>;


private:

	////
	// Object construction/destruction

	/// Adopt a successfully created pipeline.
	FullscreenPass(
		Device &device, SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTextureFormat format, ShaderResources resources
	)
		: m_device(&device), m_pipeline(pipeline), m_format(format), m_resources(resources) {}


	////
	// Methods

	/// Validate the encoder state and all bindings before any command is encoded.
	[[nodiscard]] auto validate (const FullscreenBindings &bindings) const -> std::expected<void, FullscreenError>;


	////
	// Fields

	/// The borrowed creating device.
	Device *m_device = nullptr;

	/// The owned graphics pipeline.
	SDL_GPUGraphicsPipeline *m_pipeline = nullptr;

	/// The color attachment format of the pipeline.
	SDL_GPUTextureFormat m_format;

	/// The exact fragment resource counts that every draw must supply.
	ShaderResources m_resources;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_FULLSCREEN_H__
