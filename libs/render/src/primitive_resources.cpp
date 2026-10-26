
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>

// SDL3 library
#include <SDL3/SDL.h>

// Embedded Render resources
#include <fcg-render-shaders.h>

// FCG Framework
#include <FCG/buffer.h>
#include <FCG/res.h>

// Local includes
#include "primitive_resources.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Creates an unexpected \c fcg::RenderError with the given code and message.
auto failure (fcg::RenderErrorCode code, std::string message) -> std::unexpected<fcg::RenderError> {
	return std::unexpected(fcg::RenderError{code, std::move(message)});
}

/// Creates an \c SDLFailure error from \p context and SDL's current error message.
auto sdlFailure (const char *context) -> std::unexpected<fcg::RenderError> {
	return failure(fcg::RenderErrorCode::SDLFailure, std::string(context) + ": " + SDL_GetError());
}

/// Translates a Core buffer error into a Render error, prefixing the message with context.
auto bufferFailure (const fcg::BufferError &error) -> std::unexpected<fcg::RenderError> {
	const auto code = error.code == fcg::BufferErrorCode::SDLFailure
		? fcg::RenderErrorCode::SDLFailure : error.code == fcg::BufferErrorCode::InvalidState
		? fcg::RenderErrorCode::InvalidState : fcg::RenderErrorCode::InvalidArgument;
	return failure(code, "Primitive renderer: " + error.message);
}

/// Vertex of the immutable unit geometry; the pipeline describes its field offsets explicitly.
struct Vertex
{
	////
	// Fields

	/// Local position; the unit primitive spans -1 to 1 along each used axis.
	glm::vec3 position;

	/// Flat outward local normal.
	glm::vec3 normal;

	/// Texture coordinates within the face.
	glm::vec2 uv;
};

/// Uniform block describing where the vertex shader reads each used attribute; matches \c Attributes in
/// primitive.vert.glsl.
struct alignas(16) AttributeUniforms
{
	////
	// Fields

	/// Per slot: word offset, word stride, whether a buffer is bound, and a reserved component.
	glm::uvec4 layouts[4]{};

	/// Per slot: \c fcg::AttributeTraits component offsets in words.
	glm::uvec4 components[4]{};

	/// Per slot: constant or default value in shader component order; used when no buffer is bound.
	glm::vec4 constants[4]{{0, 0, 0, 1}, {1, 1, 1, 0}, {0, 0, 0, 1}, {1, 1, 1, 1}};

	/// First instance of the draw in X; SDL's first-instance parameter is always zero.
	glm::uvec4 drawInfo{};
};
static_assert(sizeof(AttributeUniforms) == 13 * 16);
static_assert(offsetof(AttributeUniforms, components) == 4 * 16);
static_assert(offsetof(AttributeUniforms, constants) == 8 * 16);
static_assert(offsetof(AttributeUniforms, drawInfo) == 12 * 16);

/// Uniform block with lighting data; matches \c Lighting in the fragment shaders.
struct alignas(16) LightUniforms
{
	////
	// Fields

	/// Normalized direction toward the light; W is nonzero if lighting is enabled.
	glm::vec4 direction{};

	/// Ambient RGB intensity; W is nonzero for two-sided quad lighting.
	glm::vec4 ambient{};

	/// Diffuse RGB intensity; W is unused.
	glm::vec4 diffuse{};
};
static_assert(sizeof(LightUniforms) == 3 * 16);

/// Projection and surface culling used by the ellipsoid fragment shader.
struct alignas(16) RaycastUniforms
{
	/// Eye-to-clip transformation, including the actual depth mapping.
	glm::mat4 projection;

	/// Clip-to-eye transformation for general viewing rays.
	glm::mat4 invProjection;

	/// Surface culling mode in X; other components are reserved.
	glm::uvec4 flags{};
};
static_assert(sizeof(RaycastUniforms) == 9 * 16);
static_assert(offsetof(RaycastUniforms, invProjection) == 4 * 16);
static_assert(offsetof(RaycastUniforms, flags) == 8 * 16);
static_assert(SDL_GPU_CULLMODE_NONE == 0 && SDL_GPU_CULLMODE_FRONT == 1 && SDL_GPU_CULLMODE_BACK == 2);

/// Returns whether all components of \p value are finite.
auto finite (const glm::vec3 &value) -> bool {
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

/// Returns whether all matrix components are finite, including the homogeneous row.
auto finite (const glm::mat4 &value) -> bool {
	for (int column = 0; column < 4; ++column)
		for (int row = 0; row < 4; ++row)
			if (!std::isfinite(value[column][row]))
				return false;
	return true;
}

/// Validates the source of one used attribute for the instances before \p end and writes its storage binding and
/// uniform layout into \p bindings and \p uniforms at \p slot.
template <fcg::Attribute A> auto prepareSource (
	const fcg::PrimitiveAttributes &attributes, std::size_t slot, std::size_t end,
	AttributeUniforms &uniforms, std::array<SDL_GPUBuffer*, 4> &bindings
) -> std::expected<void, fcg::RenderError>
{
	if (!attributes.usable<A>())
		return failure(
			fcg::RenderErrorCode::InvalidState, "Changed attribute array requires a successful replacement"
		);
	const auto source = attributes.source<A>();
	if (const auto *view = std::get_if<fcg::AttributeBufferView<A>>(&source))
	{
		if (end > view->count)
			return failure(
				fcg::RenderErrorCode::InvalidArgument, "Consumed attribute array does not cover the draw range"
			);
		if (!view->buffer || !view->buffer->handle() || view->buffer->device() != &attributes.device()
			|| !(view->buffer->usage() & SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ))
			return failure(fcg::RenderErrorCode::InvalidState, "Attribute buffer owner is no longer usable");
		const auto size = sizeof(typename fcg::AttributeTraits<A>::Value);
		const auto available = view->buffer->size();
		if (view->offset > available || (available - view->offset) < size
			|| end - 1 > (available - view->offset - size) / view->stride)
			return failure(fcg::RenderErrorCode::InvalidState, "Attribute buffer no longer covers the requested range");
		bindings[slot] = view->buffer->handle();
		uniforms.layouts[slot] = {(Uint32)(view->offset / 4), (Uint32)(view->stride / 4), 1, 0};
	}
	else if (const auto *value = std::get_if<typename fcg::AttributeTraits<A>::Value>(&source))
	{
		const auto *bytes = (const std::byte*)value;
		for (std::size_t i = 0; i < fcg::AttributeTraits<A>::componentOffsets.size(); ++i)
			std::memcpy(
				&uniforms.constants[slot][(int)i], bytes + fcg::AttributeTraits<A>::componentOffsets[i], sizeof(float)
			);
	}
	for (std::size_t i = 0; i < fcg::AttributeTraits<A>::componentOffsets.size(); ++i)
		uniforms.components[slot][(int)i] = (Uint32)(fcg::AttributeTraits<A>::componentOffsets[i] / 4);
	return {};
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
// Class implementations
//

////
// detail::PrimitiveResources

detail::PrimitiveResources::~PrimitiveResources() {
	for (auto *pipeline : pipelines)
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(device.handle(), pipeline);
}

auto detail::PrimitiveResources::create (
	Device &device, const RenderTargetInfo &target, const RendererOptions &options, PrimitiveKind kind
) -> std::expected<std::unique_ptr<PrimitiveResources>, RenderError>
{
	const bool quad = kind != PrimitiveKind::Box;
	const bool ellipsoid = kind == PrimitiveKind::Ellipsoid;
	const auto cull = options.cullMode.value_or(kind == PrimitiveKind::Quad
		? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK);
	if (!device.handle())
		return failure(RenderErrorCode::InvalidState, "Renderer requires a live device");
	if (target.colorFormat == SDL_GPU_TEXTUREFORMAT_INVALID
		|| !SDL_GPUTextureSupportsFormat(device.handle(), target.colorFormat, SDL_GPU_TEXTURETYPE_2D,
			SDL_GPU_TEXTUREUSAGE_COLOR_TARGET))
		return failure(RenderErrorCode::InvalidArgument, "Unsupported color target format");
	const bool depth = target.depthStencilFormat != SDL_GPU_TEXTUREFORMAT_INVALID;
	if (target.sampleCount < SDL_GPU_SAMPLECOUNT_1 || target.sampleCount > SDL_GPU_SAMPLECOUNT_8
		|| !SDL_GPUTextureSupportsSampleCount(device.handle(), target.colorFormat, target.sampleCount))
		return failure(RenderErrorCode::InvalidArgument, "Unsupported target sample count");
	if (depth && (!SDL_GPUTextureSupportsFormat(device.handle(), target.depthStencilFormat, SDL_GPU_TEXTURETYPE_2D,
		SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)
		|| !SDL_GPUTextureSupportsSampleCount(device.handle(), target.depthStencilFormat, target.sampleCount)))
		return failure(RenderErrorCode::InvalidArgument, "Unsupported depth target format or sample count");
	if (cull < SDL_GPU_CULLMODE_NONE || cull > SDL_GPU_CULLMODE_BACK
		|| options.depthCompare < SDL_GPU_COMPAREOP_NEVER || options.depthCompare > SDL_GPU_COMPAREOP_ALWAYS)
		return failure(RenderErrorCode::InvalidArgument, "Invalid culling or depth comparison option");

	std::array<Vertex, 24> geometry{};
	std::array<Uint16, 36> triangles{};
	const std::array<glm::vec2, 4> uv{{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
	if (quad)
		for (std::size_t i = 0; i < 4; ++i)
			geometry[i] = {{2.f * uv[i].x - 1.f, 2.f * uv[i].y - 1.f, 0.f}, {0, 0, 1}, uv[i]};
	else
	{
		const std::array<glm::vec3, 6> normals{{{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}}};
		const std::array<glm::vec3, 6> u{{{0,0,-1}, {0,0,1}, {1,0,0}, {1,0,0}, {1,0,0}, {-1,0,0}}};
		const std::array<glm::vec3, 6> v{{{0,1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {0,1,0}, {0,1,0}}};
		for (std::size_t face = 0; face < 6; ++face)
		{
			for (std::size_t i = 0; i < 4; ++i)
				geometry[4 * face + i] = {
					normals[face] + u[face] * (2.f * uv[i].x - 1.f) + v[face] * (2.f * uv[i].y - 1.f),
					normals[face], uv[i]
				};
			const std::array<Uint16, 6> order{0, 1, 2, 2, 1, 3};
			for (std::size_t i = 0; i < 6; ++i)
				triangles[6 * face + i] = (Uint16)(4 * face + order[i]);
		}
	}
	const auto vertexData = std::span<const Vertex>(geometry.data(), quad ? 4 : 24);
	auto vertex = Buffer::create(device, vertexData.size_bytes(), SDL_GPU_BUFFERUSAGE_VERTEX);
	if (!vertex)
		return bufferFailure(vertex.error());
	std::optional<Buffer> index;
	if (!quad) {
		auto buffer = Buffer::create(device, sizeof(triangles), SDL_GPU_BUFFERUSAGE_INDEX);
		if (!buffer)
			return bufferFailure(buffer.error());
		index.emplace(std::move(*buffer));
	}
	auto dummy = Buffer::create(device, 16, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
	if (!dummy)
		return bufferFailure(dummy.error());
	const std::array<Uint32, 4> zero{};
	UploadBatch uploads(device);
	auto uploaded = uploads.upload(*vertex, vertexData);
	if (uploaded && index)
		uploaded = uploads.upload(*index, std::span(triangles));
	if (uploaded)
		uploaded = uploads.upload(*dummy, std::span(zero));
	if (uploaded)
		uploaded = uploads.submit();
	if (!uploaded)
		return bufferFailure(uploaded.error());
	auto resources = std::make_unique<PrimitiveResources>(
		device, std::move(*vertex), std::move(index), std::move(*dummy), kind, cull
	);

	const SDL_GPUVertexBufferDescription vertexBuffer{0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0};
	const std::array<SDL_GPUVertexAttribute, 3> vertexAttributes{{
		{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, position)},
		{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, normal)},
		{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, uv)}
	}};
	const auto releaseShader = [handle = device.handle()] (SDL_GPUShader *shader) {
		SDL_ReleaseGPUShader(handle, shader);
	};
	using ShaderOwner = std::unique_ptr<SDL_GPUShader, decltype(releaseShader)>;
	for (std::size_t textured = 0; textured < 2; ++textured)
	{
		const auto shader = res::shader(render::embedded::FS, ellipsoid
			? (textured ? "ellipsoid_textured" : "ellipsoid") : (textured ? "textured" : "primitive"));
		if (!shader)
			return failure(RenderErrorCode::InvalidState, "Embedded primitive shader is missing");
		const auto vert = shader->stage(ShaderStage::VERTEX);
		const auto frag = shader->stage(ShaderStage::FRAGMENT);
		if (!vert || !frag)
			return failure(RenderErrorCode::InvalidState, "Embedded primitive shader stage is missing");
		ShaderOwner vs(device.createShader(ShaderStage::VERTEX, vert->spirv,
			ShaderResources{.uniformBuffers = 2, .storageBuffers = 4}), releaseShader);
		if (!vs)
			return sdlFailure("Creating primitive vertex shader");
		ShaderOwner fs(device.createShader(ShaderStage::FRAGMENT, frag->spirv,
			ShaderResources{.uniformBuffers = ellipsoid ? 2u : 1u, .samplers = (unsigned)textured}), releaseShader);
		if (!fs)
			return sdlFailure("Creating primitive fragment shader");
		SDL_GPUColorTargetDescription color{};
		color.format = target.colorFormat;
		color.blend_state.enable_blend = options.alphaBlending;
		color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
		color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
		color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
		SDL_GPUGraphicsPipelineCreateInfo info{};
		info.vertex_shader = vs.get();
		info.fragment_shader = fs.get();
		info.vertex_input_state = {&vertexBuffer, 1, vertexAttributes.data(),
			ellipsoid ? 1u : (Uint32)vertexAttributes.size()};
		info.primitive_type = quad ? SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP : SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
		info.rasterizer_state.cull_mode = ellipsoid ? SDL_GPU_CULLMODE_NONE : cull;
		info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
		info.rasterizer_state.enable_depth_clip = true;
		info.multisample_state.sample_count = target.sampleCount;
		info.depth_stencil_state.enable_depth_test = depth && options.depthTest;
		info.depth_stencil_state.enable_depth_write = depth && options.depthWrite;
		info.depth_stencil_state.compare_op = options.depthCompare;
		info.target_info = {&color, 1, target.depthStencilFormat, depth, {}};
		resources->pipelines[textured] = SDL_CreateGPUGraphicsPipeline(device.handle(), &info);
		if (!resources->pipelines[textured])
			return sdlFailure("Creating primitive pipeline");
	}
	return resources;
}

auto detail::PrimitiveResources::draw (
	const PrimitiveAttributes &attributes, RenderState &state, SDL_GPUCommandBuffer *command,
	SDL_GPURenderPass *pass, const DrawOptions &options, InstanceRange range
) const -> std::expected<void, RenderError>
{
	if (&attributes.device() != &device || !command || !pass)
		return failure(
			RenderErrorCode::InvalidArgument, "Draw requires attributes, commands, and pass on the renderer's device"
		);
	const auto total = attributes.instanceCount();
	if (range.first > total)
		return failure(RenderErrorCode::InvalidArgument, "Instance range starts beyond position count");
	const auto count = range.count == std::dynamic_extent ? total - range.first : range.count;
	if (count > total - range.first || count > std::numeric_limits<Uint32>::max())
		return failure(RenderErrorCode::InvalidArgument, "Instance range exceeds position count");
	if (!count)
		return {};
	if (options.texture && (!options.texture->texture || !options.texture->sampler))
		return failure(RenderErrorCode::InvalidArgument, "Texturing requires both texture and sampler");
	const auto &light = options.lighting;
	const auto scale = std::max({
		std::abs(light.direction.x), std::abs(light.direction.y), std::abs(light.direction.z)
	});
	if (light.enabled
		&& (!finite(light.direction) || scale == 0.f || !finite(light.ambient) || !finite(light.diffuse)))
		return failure(
			RenderErrorCode::InvalidArgument, "Lighting requires finite colors and a finite nonzero direction"
		);
	AttributeUniforms uniforms;
	uniforms.drawInfo.x = (Uint32)range.first;
	std::array<SDL_GPUBuffer*, 4> bindings;
	bindings.fill(dummy.handle());
	const auto end = range.first + count;
	auto prepared = prepareSource<Attribute::Position>(attributes, 0, end, uniforms, bindings);
	if (prepared)
		prepared = prepareSource<Attribute::Extent>(attributes, 1, end, uniforms, bindings);
	if (prepared)
		prepared = prepareSource<Attribute::Orientation>(attributes, 2, end, uniforms, bindings);
	if (prepared)
		prepared = prepareSource<Attribute::Color>(attributes, 3, end, uniforms, bindings);
	if (!prepared)
		return prepared;
	LightUniforms lighting;
	if (light.enabled)
		lighting.direction = {glm::normalize(light.direction / scale), 1.f};
	lighting.ambient = {light.ambient, kind == PrimitiveKind::Quad ? 1.f : 0.f};
	lighting.diffuse = {light.diffuse, 0.f};

	// Reject invalid viewing transforms before any uniforms or bindings are recorded
	std::optional<RaycastUniforms> raycast;
	if (kind == PrimitiveKind::Ellipsoid)
	{
		const auto &modelview = state.modelviewMatrix();
		const auto &projection = state.projectionMatrix();
		if (!finite(modelview) || !finite(projection) || modelview[0][3] != 0.f || modelview[1][3] != 0.f
			|| modelview[2][3] != 0.f || modelview[3][3] != 1.f
			|| !finite(state.invModelviewMatrix()) || !finite(state.invProjectionMatrix()))
			return failure(RenderErrorCode::InvalidArgument,
				"Ellipsoids require finite invertible viewing matrices and an affine modelview");
		raycast.emplace(projection, state.invProjectionMatrix(), glm::uvec4((Uint32)cull, 0, 0, 0));
	}

	// All validation precedes recording. Draw never creates resources or uploads arrays.
	state.pushViewingUniforms(command, ShaderStage::VERTEX, 0);
	SDL_PushGPUVertexUniformData(command, 1, &uniforms, sizeof(uniforms));
	SDL_PushGPUFragmentUniformData(command, 0, &lighting, sizeof(lighting));
	if (raycast)
		SDL_PushGPUFragmentUniformData(command, 1, &*raycast, sizeof(*raycast));
	SDL_BindGPUGraphicsPipeline(pass, pipelines[options.texture.has_value()]);
	const SDL_GPUBufferBinding vertex{vertices.handle(), 0};
	SDL_BindGPUVertexBuffers(pass, 0, &vertex, 1);
	SDL_BindGPUVertexStorageBuffers(pass, 0, bindings.data(), (Uint32)bindings.size());
	if (options.texture) {
		const SDL_GPUTextureSamplerBinding texture{options.texture->texture, options.texture->sampler};
		SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
	}
	if (indices) {
		const SDL_GPUBufferBinding index{indices->handle(), 0};
		SDL_BindGPUIndexBuffer(pass, &index, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_DrawGPUIndexedPrimitives(pass, 36, (Uint32)count, 0, 0, 0);
	} else
		SDL_DrawGPUPrimitives(pass, 4, (Uint32)count, 0, 0);
	return {};
}



//////
//
// Module namespace close
//

} // namespace fcg
