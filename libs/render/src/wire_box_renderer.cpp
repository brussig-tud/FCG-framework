
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
#include <FCG/buffer.h>

// Implemented header
#include "FCG/Render/wire_box_renderer.h"

// Embedded shaders
#include <fcg-render-shaders.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Box attributes in shader storage-slot order.
constexpr std::array consumed{
	fcg::Attribute::Position, fcg::Attribute::Extent, fcg::Attribute::Orientation,
	fcg::Attribute::Color, fcg::Attribute::Radius
};

/// Viewing state matching the wire-box shader's Viewing uniform block.
struct alignas(16) WireViewingUniforms {
	/// Object-to-clip transformation.
	glm::mat4 matrix;

	/// Physical viewport dimensions in XY, with ZW unused.
	glm::vec4 viewport;
};
static_assert(sizeof(WireViewingUniforms) == 80);
static_assert(offsetof(WireViewingUniforms, viewport) == 64);

/// Attribute addressing matching the wire-box shader's Attributes uniform block.
struct alignas(16) AttributeUniforms
{
	/// Per attribute: word offset, word stride, buffer-enabled flag and unused component.
	glm::uvec4 layouts[5]{};

	/// Component offsets in words, supporting padded vector types.
	glm::uvec4 components[5]{};

	/// Fallback position, extent, orientation, color and radius components.
	glm::vec4 constants[5]{{0, 0, 0, 1}, {1, 1, 1, 0}, {0, 0, 0, 1}, {1, 1, 1, 1}, {.5f, 0, 0, 0}};

	/// First box in X and round-cap flag in Y, with ZW unused.
	glm::uvec4 drawInfo{};
};
static_assert(sizeof(AttributeUniforms) == 256);
static_assert(offsetof(AttributeUniforms, components) == 80);
static_assert(offsetof(AttributeUniforms, constants) == 160);
static_assert(offsetof(AttributeUniforms, drawInfo) == 240);

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
	std::array<SDL_GPUBuffer*, 5> &bindings
)
{
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

/// Resources independent of the number of drawn boxes.
struct WireBoxRenderer::State
{
	////
	// Object construction/destruction

	/// Construct state from owned placeholder storage and a borrowed device.
	State(Device &device, Buffer indices, Buffer dummy, bool blending)
		: device(device), indices(std::move(indices)), dummy(std::move(dummy)), blending(blending)
	{}

	/// Release the wire-box pipeline; submitted commands retain their resources.
	~State() {
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(device.handle(), pipeline);
	}


	////
	// Fields

	/// Device that must outlive this state.
	Device &device;

	/// Immutable triangle indices assembling twelve independent stroke rectangles.
	Buffer indices;

	/// Placeholder graphics storage for attributes supplied as constants.
	Buffer dummy;

	/// The wire-box graphics pipeline.
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
// WireBoxRenderer

WireBoxRenderer::WireBoxRenderer(std::unique_ptr<State> state) noexcept : state(std::move(state)) {}

WireBoxRenderer::~WireBoxRenderer() = default;

WireBoxRenderer::WireBoxRenderer(WireBoxRenderer&&) noexcept = default;

auto WireBoxRenderer::operator= (WireBoxRenderer&&) noexcept -> WireBoxRenderer& = default;

auto WireBoxRenderer::create (Device &device, const RenderTargetInfo &target, const RendererOptions &options)
	-> std::expected<WireBoxRenderer, RenderError>
{
	// Upload the fixed edge rectangles and fallback storage once
	std::array<Uint16, 72> triangles;
	constexpr std::array<Uint16, 6> order{0, 1, 2, 2, 1, 3};
	for (std::size_t edge = 0; edge < 12; ++edge)
		for (std::size_t i = 0; i < order.size(); ++i)
			triangles[6 * edge + i] = (Uint16)(4 * edge + order[i]);
	auto indices = Buffer::create(device, sizeof(triangles), SDL_GPU_BUFFERUSAGE_INDEX);
	if (!indices)
		return std::unexpected(RenderError{RenderErrorCode::SDLFailure, indices.error().message});
	auto dummy = Buffer::create(device, 16, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
	if (!dummy)
		return std::unexpected(RenderError{RenderErrorCode::SDLFailure, dummy.error().message});
	const std::array<Uint32, 4> zero{};
	UploadBatch uploads(device);
	auto uploaded = uploads.upload(*indices, std::span(triangles));
	if (uploaded)
		uploaded = uploads.upload(*dummy, std::span(zero));
	if (uploaded)
		uploaded = uploads.submit();
	if (!uploaded)
		return std::unexpected(RenderError{RenderErrorCode::SDLFailure, uploaded.error().message});
	auto state = std::make_unique<State>(device, std::move(*indices), std::move(*dummy), options.alphaBlending);

	// Create target-aware shaders, including depth-free fragment translation
	const auto shader = res::shader(render::embedded::FS, "wire_box");
	if (!shader || !shader->stage(ShaderStage::VERTEX) || !shader->stage(ShaderStage::FRAGMENT))
		return std::unexpected(RenderError{RenderErrorCode::InvalidState, "Embedded wire-box shader is missing"});
	const auto release = [&] (SDL_GPUShader *value) { SDL_ReleaseGPUShader(device.handle(), value); };
	using ShaderOwner = std::unique_ptr<SDL_GPUShader, decltype(release)>;
	ShaderOwner vertex(device.createShader(ShaderStage::VERTEX, shader->stage(ShaderStage::VERTEX)->spirv,
		ShaderResources{.uniformBuffers = 2, .storageBuffers = 5}), release);
	if (!vertex)
		return sdlFailure("Creating wire-box vertex shader");
	ShaderOwner fragment(device.createShader(ShaderStage::FRAGMENT, shader->stage(ShaderStage::FRAGMENT)->spirv,
		ShaderResources{.uniformBuffers = 1}, target), release);
	if (!fragment)
		return sdlFailure("Creating wire-box fragment shader");

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
	info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
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
		return sdlFailure("Creating wire-box graphics pipeline");
	return WireBoxRenderer(std::move(state));
}

auto WireBoxRenderer::create (Player &player, const RendererOptions &options)
	-> std::expected<WireBoxRenderer, RenderError> {
	return create(player.device(), *player.mainRenderTargetInfo(), options);
}

auto WireBoxRenderer::supportedAttributes () const -> std::span<const Attribute> {
	return consumed;
}

auto WireBoxRenderer::style () const -> const LineStyle& {
	return state->style;
}

void WireBoxRenderer::setStyle (const LineStyle &style) {
	state->style = style;
}

auto WireBoxRenderer::draw (
	const PrimitiveAttributes &attributes, RenderState &renderState,
	SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass, const DrawOptions &options, InstanceRange range
) const -> std::expected<void, RenderError>
{
	// Unsupported features are the only recoverable draw errors
	if (options.texture || options.lighting.enabled)
		return std::unexpected(RenderError{
			RenderErrorCode::InvalidArgument, "Wire boxes support unlit colors without textures"
		});
	const auto count = range.count == std::dynamic_extent ? attributes.instanceCount() - range.first : range.count;
	if (!count)
		return {};

	// Address per-box attributes and copy the mutable appearance into uniforms
	AttributeUniforms uniforms;
	uniforms.constants[4].x = state->style.defaultWidth * .5f;
	uniforms.drawInfo = {(Uint32)range.first, state->style.cap == LineCap::Round, 0, 0};
	std::array<SDL_GPUBuffer*, 5> bindings;
	bindings.fill(state->dummy.handle());
	prepare<Attribute::Position>(attributes, 0, uniforms, bindings);
	prepare<Attribute::Extent>(attributes, 1, uniforms, bindings);
	prepare<Attribute::Orientation>(attributes, 2, uniforms, bindings);
	prepare<Attribute::Color>(attributes, 3, uniforms, bindings);
	prepare<Attribute::Radius>(attributes, 4, uniforms, bindings);
	const auto size = renderState.viewportSize();
	const WireViewingUniforms viewing{renderState.modelviewProjectionMatrix(), glm::vec4(size.x, size.y, 0, 0)};
	const AppearanceUniforms appearance{glm::uvec4(
		state->blending && state->style.filter == LineFilter::Smoothstep, state->blending, 0, 0
	)};

	// Draw all twelve edge rectangles in one instance per box
	SDL_BindGPUGraphicsPipeline(pass, state->pipeline);
	SDL_PushGPUVertexUniformData(command, 0, &viewing, sizeof(viewing));
	SDL_PushGPUVertexUniformData(command, 1, &uniforms, sizeof(uniforms));
	SDL_PushGPUFragmentUniformData(command, 0, &appearance, sizeof(appearance));
	SDL_BindGPUVertexStorageBuffers(pass, 0, bindings.data(), (Uint32)bindings.size());
	const SDL_GPUBufferBinding index{state->indices.handle(), 0};
	SDL_BindGPUIndexBuffer(pass, &index, SDL_GPU_INDEXELEMENTSIZE_16BIT);
	SDL_DrawGPUIndexedPrimitives(pass, 72, (Uint32)count, 0, 0, 0);
	return {};
}



//////
//
// Module namespace close
//

} // namespace fcg
