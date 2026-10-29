
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstring>
#include <string>
#include <utility>

// FCG Framework
#include <FCG/player.h>
#include <FCG/res.h>
#include <FCG/Render/line_set_builder.h>

// Implemented header
#include "FCG/Render/line_renderer.h"

// Embedded shaders
#include <fcg-render-shaders.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Node attributes in shader storage-slot order.
constexpr std::array consumed{fcg::Attribute::Position, fcg::Attribute::Radius, fcg::Attribute::Color};

/// Viewing state matching the line shader's Viewing uniform block.
struct alignas(16) LineViewingUniforms {
	/// Object-to-clip transformation.
	glm::mat4 matrix;

	/// Physical viewport dimensions in XY, with ZW unused.
	glm::vec4 viewport;
};
static_assert(sizeof(LineViewingUniforms) == 80);

/// Attribute addressing matching the line shader's Attributes uniform block.
struct alignas(16) AttributeUniforms
{
	/// Per attribute: word offset, word stride, buffer-enabled flag and unused component.
	glm::uvec4 layouts[3]{};

	/// Component offsets in words, supporting padded vector types.
	glm::uvec4 components[3]{};

	/// Fallback position, radius and color components.
	glm::vec4 constants[3]{{0, 0, 0, 1}, {.5f, 0, 0, 0}, {1, 1, 1, 1}};

	/// Index-buffer word offset, first segment, round-cap flag and unused component.
	glm::uvec4 connectivity{};
};
static_assert(sizeof(AttributeUniforms) == 160);
static_assert(offsetof(AttributeUniforms, connectivity) == 144);

/// Coverage options matching the fragment shader's Appearance uniform block.
struct alignas(16) AppearanceUniforms {
	/// Smooth-filter and blending flags in XY, with ZW unused.
	glm::uvec4 flags;
};
static_assert(sizeof(AppearanceUniforms) == 16);

/// Capture an operational SDL failure.
auto sdlFailure (const char *operation) -> std::unexpected<fcg::RenderError> {
	return std::unexpected(fcg::RenderError{fcg::RenderErrorCode::SDLFailure,
		std::string(operation) + ": " + SDL_GetError()});
}

/// Bind one attribute descriptor without validating its caller-owned layout or values.
template <fcg::Attribute A> void prepare (
	const fcg::PrimitiveAttributes &attributes, std::size_t slot, AttributeUniforms &uniforms,
	std::array<SDL_GPUBuffer*, 4> &bindings
){
	const auto source = attributes.source<A>();
	if (const auto *view = std::get_if<fcg::AttributeBufferView<A>>(&source)) {
		bindings[slot] = view->buffer->handle();
		uniforms.layouts[slot] = {(Uint32)(view->offset / 4), (Uint32)(view->stride / 4), 1, 0};
	}
	else if (const auto *value = std::get_if<typename fcg::AttributeTraits<A>::Value>(&source))
		for (std::size_t i = 0; i < fcg::AttributeTraits<A>::componentOffsets.size(); ++i)
			std::memcpy(&uniforms.constants[slot][(int)i],
				(const std::byte*)value + fcg::AttributeTraits<A>::componentOffsets[i], sizeof(float));
	for (std::size_t i = 0; i < fcg::AttributeTraits<A>::componentOffsets.size(); ++i)
		uniforms.components[slot][(int)i] = (Uint32)(fcg::AttributeTraits<A>::componentOffsets[i] / 4);
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Structs and enums
//

/// Resources independent of node or connectivity collection sizes.
struct LineRenderer::State
{
	////
	// Object construction/destruction

	/// Construct state from owned placeholder storage and a borrowed device.
	State(Device &device, Buffer dummy, bool blending)
		: device(device), dummy(std::move(dummy)), blending(blending)
	{}

	/// Release the line pipeline; submitted commands retain their resources.
	~State() {
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(device.handle(), pipeline);
	}


	////
	// Fields

	/// Device that must outlive this state.
	Device &device;

	/// Placeholder graphics storage for attributes supplied as constants.
	Buffer dummy;

	/// The line graphics pipeline.
	SDL_GPUGraphicsPipeline *pipeline = nullptr;

	/// Whether the fixed pipeline blends straight-alpha output.
	bool blending;

	/// Mutable style copied into uniforms for each draw.
	LineStyle style;
};



//////
//
// Class implementations
//

////
// LineRenderer

LineRenderer::LineRenderer(std::unique_ptr<State> state) noexcept : state(std::move(state)) {}

LineRenderer::~LineRenderer() = default;

LineRenderer::LineRenderer(LineRenderer&&) noexcept = default;

auto LineRenderer::operator= (LineRenderer&&) noexcept -> LineRenderer& = default;

auto LineRenderer::create (Device &device, const RenderTargetInfo &target, const RendererOptions &options)
	-> std::expected<LineRenderer, RenderError>
{
	// Create placeholder storage; disabled attribute slots never read its contents
	auto dummy = Buffer::create(device, 4, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
	if (!dummy)
		return std::unexpected(RenderError{RenderErrorCode::SDLFailure, dummy.error().message});
	auto state = std::make_unique<State>(device, std::move(*dummy), options.alphaBlending);

	// Create target-aware shaders, including depth-free fragment translation
	const auto shader = res::shader(render::embedded::FS, "line");
	if (!shader || !shader->stage(ShaderStage::VERTEX) || !shader->stage(ShaderStage::FRAGMENT))
		return std::unexpected(RenderError{RenderErrorCode::InvalidState, "Embedded line shader is missing"});
	const auto release = [&] (SDL_GPUShader *value) { SDL_ReleaseGPUShader(device.handle(), value); };
	using ShaderOwner = std::unique_ptr<SDL_GPUShader, decltype(release)>;
	ShaderOwner vertex(device.createShader(ShaderStage::VERTEX, shader->stage(ShaderStage::VERTEX)->spirv,
		ShaderResources{.uniformBuffers = 2, .storageBuffers = 4}), release);
	if (!vertex)
		return sdlFailure("Creating line vertex shader");
	ShaderOwner fragment(device.createShader(ShaderStage::FRAGMENT, shader->stage(ShaderStage::FRAGMENT)->spirv,
		ShaderResources{.uniformBuffers = 1}, target), release);
	if (!fragment)
		return sdlFailure("Creating line fragment shader");

	// Configure the common depth and straight-alpha options without capability queries
	SDL_GPUColorTargetDescription color{};
	color.format = target.colorFormat;
	color.blend_state.enable_blend = options.alphaBlending;
	color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
	color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
	color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
	const bool depth = target.depthStencilFormat != SDL_GPU_TEXTUREFORMAT_INVALID;
	SDL_GPUGraphicsPipelineCreateInfo info{};
	info.vertex_shader = vertex.get();
	info.fragment_shader = fragment.get();
	info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
	info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	info.rasterizer_state.cull_mode = options.cullMode.value_or(SDL_GPU_CULLMODE_NONE);
	info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
	info.rasterizer_state.enable_depth_clip = true;
	info.multisample_state.sample_count = target.sampleCount;
	info.depth_stencil_state.enable_depth_test = depth && options.depthTest;
	info.depth_stencil_state.enable_depth_write = depth && options.depthWrite;
	info.depth_stencil_state.compare_op = options.depthCompare;
	info.target_info = {&color, 1, target.depthStencilFormat, depth, {}};
	state->pipeline = SDL_CreateGPUGraphicsPipeline(device.handle(), &info);
	if (!state->pipeline)
		return sdlFailure("Creating line graphics pipeline");
	return LineRenderer(std::move(state));
}

auto LineRenderer::create (Player &player, const RendererOptions &options)
	-> std::expected<LineRenderer, RenderError> {
	return create(player.device(), *player.mainRenderTargetInfo(), options);
}

auto LineRenderer::supportedAttributes () const -> std::span<const Attribute> {
	return consumed;
}

auto LineRenderer::nodesPerPrimitive () const -> std::size_t {
	return 2;
}

auto LineRenderer::style () const -> const LineStyle& {
	return state->style;
}

void LineRenderer::setStyle (const LineStyle &style) {
	state->style = style;
}

auto LineRenderer::draw (
	const PrimitiveAttributes &attributes, const PrimitiveConnectivity &connectivity, RenderState &renderState,
	SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass, const DrawOptions &options, InstanceRange range
) const -> std::expected<void, RenderError>
{
	// Unsupported features are the only recoverable draw errors
	if (options.texture || options.lighting.enabled)
		return std::unexpected(RenderError{
			RenderErrorCode::InvalidArgument, "Lines support unlit colors without textures"
		});
	const auto count = range.count == std::dynamic_extent ?+
		connectivity.indexCount()/2 - range.first : range.count;
	if (!count)
		return {};

	// Prepare node addressing and style without scanning values or index contents
	AttributeUniforms uniforms;
	uniforms.constants[1].x = state->style.defaultWidth * .5f;
	const auto view = connectivity.source();
	uniforms.connectivity = {
		(Uint32)(view.offset / 4), (Uint32)range.first, state->style.cap == LineCap::Round, 0
	};
	std::array<SDL_GPUBuffer*, 4> bindings;
	bindings.fill(state->dummy.handle());
	prepare<Attribute::Position>(attributes, 0, uniforms, bindings);
	prepare<Attribute::Radius>(attributes, 1, uniforms, bindings);
	prepare<Attribute::Color>(attributes, 2, uniforms, bindings);
	bindings[3] = view.buffer->handle();
	const auto size = renderState.viewportSize();
	const LineViewingUniforms viewing{renderState.modelviewProjectionMatrix(), glm::vec4(size.x, size.y, 0, 0)};
	const AppearanceUniforms appearance{glm::uvec4(
		state->blending && state->style.filter == LineFilter::Smoothstep, state->blending, 0, 0
	)};

	// Draw four generated vertices per independent segment
	SDL_BindGPUGraphicsPipeline(pass, state->pipeline);
	SDL_PushGPUVertexUniformData(command, 0, &viewing, sizeof(viewing));
	SDL_PushGPUVertexUniformData(command, 1, &uniforms, sizeof(uniforms));
	SDL_PushGPUFragmentUniformData(command, 0, &appearance, sizeof(appearance));
	SDL_BindGPUVertexStorageBuffers(pass, 0, bindings.data(), (Uint32)bindings.size());
	SDL_DrawGPUPrimitives(pass, 4, (Uint32)count, 0, 0);
	return {};
}

auto LineRenderer::draw (
	const LineSet &lines, RenderState &state, SDL_GPUCommandBuffer *commandBuffer,
	SDL_GPURenderPass *renderPass, const DrawOptions &options, InstanceRange range
) const -> std::expected<void, RenderError> {
	return draw(lines.attributes, lines.connectivity, state, commandBuffer, renderPass, options, range);
}



//////
//
// Module namespace close
//

} // namespace fcg
