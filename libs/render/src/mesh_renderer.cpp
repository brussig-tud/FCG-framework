
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

// Framework and embedded shader resources
#include <FCG/player.h>
#include <FCG/res.h>
#include <FCG/Render/gpu_mesh.h>
#include <FCG/Render/mesh_renderer.h>
#include <fcg-render-shaders.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Fragment parameters matching the mesh shaders' Draw uniform layout.
struct MeshUniforms
{
	/// Linear RGBA color multiplier.
	glm::vec4 color;

	/// Normalized eye-space light direction; W enables lighting.
	glm::vec4 direction{0};

	/// Ambient RGB intensity; W enables reversed back-face normals.
	glm::vec4 ambient;

	/// Diffuse RGB intensity; W is reserved.
	glm::vec4 diffuse;
};

/// Capture an SDL failure before cleaning up pipeline resources.
auto failure (const char *operation) -> std::unexpected<fcg::RenderError> {
	return std::unexpected(fcg::RenderError{fcg::RenderErrorCode::SDLFailure,
		std::string(operation) + ": " + SDL_GetError()});
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
// MeshRenderer

MeshRenderer::~MeshRenderer() {
	if (m_device)
		for (auto *pipeline : m_pipelines)
			if (pipeline) SDL_ReleaseGPUGraphicsPipeline(m_device->handle(), pipeline);
}

MeshRenderer::MeshRenderer(MeshRenderer &&other) noexcept
	: m_device(std::exchange(other.m_device, nullptr)), m_layout(other.m_layout),
	m_pipelines(std::exchange(other.m_pipelines, {})), m_twoSided(other.m_twoSided)
{}

auto MeshRenderer::operator= (MeshRenderer &&other) noexcept -> MeshRenderer& {
	if (this != &other) {
		MeshRenderer replacement(std::move(other));
		std::swap(m_device, replacement.m_device);
		std::swap(m_layout, replacement.m_layout);
		std::swap(m_pipelines, replacement.m_pipelines);
		std::swap(m_twoSided, replacement.m_twoSided);
	}
	return *this;
}

auto MeshRenderer::create (const GPUMesh &mesh, const RenderTargetInfo &target, const RendererOptions &options)
	-> std::expected<MeshRenderer, RenderError> {
	return create(mesh.device(), target, mesh.layout(), options);
}

auto MeshRenderer::create (Player &player, GPUMeshLayout layout, const RendererOptions &options)
	-> std::expected<MeshRenderer, RenderError> {
	return create(player.device(), *player.mainRenderTargetInfo(), layout, options);
}

auto MeshRenderer::create (
	Device &device, const RenderTargetInfo &target, GPUMeshLayout layout, const RendererOptions &options
) -> std::expected<MeshRenderer, RenderError>
{
	// Declare precisely the attributes present in the canonical layout
	MeshRenderer result(device, layout);
	const auto cull = options.cullMode.value_or(SDL_GPU_CULLMODE_BACK);
	result.m_twoSided = cull == SDL_GPU_CULLMODE_NONE;
	std::vector<SDL_GPUVertexAttribute> attributes{{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0}};
	Uint32 offset = 12;
	if (layout.normals) {
		attributes.push_back({1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offset});
		offset += 12;
	}
	if (layout.colors) {
		attributes.push_back({2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offset});
		offset += 16;
	}
	if (layout.texCoords)
		attributes.push_back({3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offset});
	const SDL_GPUVertexBufferDescription buffer{
		0, Uint32(layout.stride()), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0
	};
	const bool depth = target.depthStencilFormat != SDL_GPU_TEXTUREFORMAT_INVALID;
	const auto release = [&] (SDL_GPUShader *shader) {
		SDL_ReleaseGPUShader(device.handle(), shader);
	};
	using ShaderOwner = std::unique_ptr<SDL_GPUShader, decltype(release)>;

	// Create only variants supported by this layout, with complete failure cleanup
	for (unsigned textured=0; textured<=unsigned(layout.texCoords); ++textured)
	{
		// Create a shader pair for the pipeline variant
		const auto name = "mesh_" + std::to_string(layout.key()) + (textured ? "_textured" : "");
		const auto shader = res::shader(render::embedded::FS, name);
		if (!shader || !shader->stage(ShaderStage::VERTEX) || !shader->stage(ShaderStage::FRAGMENT))
			return std::unexpected(RenderError{RenderErrorCode::InvalidState, "Missing embedded mesh shader " + name});
		ShaderOwner vertex(device.createShader(ShaderStage::VERTEX, shader->stage(ShaderStage::VERTEX)->spirv,
			ShaderResources{.uniformBuffers = 1}), release);
		if (!vertex)
			return failure("Creating mesh vertex shader");
		ShaderOwner fragment(
			device.createShader(
				ShaderStage::FRAGMENT, shader->stage(ShaderStage::FRAGMENT)->spirv,
				ShaderResources{.uniformBuffers = 1, .samplers = textured}, target
			), release
		);
		if (!fragment)
			return failure("Creating mesh fragment shader");

		// Create the pipeline variant
		// - describe color target
		SDL_GPUColorTargetDescription color{};
		color.format = target.colorFormat;
		color.blend_state.enable_blend = options.alphaBlending;
		color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
		color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
		color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
		color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
		color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
		// - configure the variant
		SDL_GPUGraphicsPipelineCreateInfo info{};
		info.vertex_shader = vertex.get();
		info.fragment_shader = fragment.get();
		info.vertex_input_state = {
			&buffer, 1, attributes.data(), Uint32(attributes.size())
		};
		info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
		info.rasterizer_state.cull_mode = cull;
		info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
		info.rasterizer_state.enable_depth_clip = true;
		info.multisample_state.sample_count = target.sampleCount;
		info.depth_stencil_state.enable_depth_test = depth && options.depthTest;
		info.depth_stencil_state.enable_depth_write = depth && options.depthWrite;
		info.depth_stencil_state.compare_op = options.depthCompare;
		info.target_info = {
			&color, 1, target.depthStencilFormat, depth, {}
		};
		// - create the pipeline object for this variant
		result.m_pipelines[textured] = SDL_CreateGPUGraphicsPipeline(device.handle(), &info);
		if (!result.m_pipelines[textured])
			return failure("Creating mesh pipeline");
	}
	return result;
}

void MeshRenderer::draw (
	const GPUMesh &mesh, RenderState &state, SDL_GPUCommandBuffer *command, SDL_GPURenderPass *pass,
	const MeshDrawOptions &options
) const
{
	// Empty geometry records no commands
	if (!mesh.m_indexCount)
		return;

	// Prepare uniforms
	const auto &light = options.lighting;
	MeshUniforms uniforms{
		options.color, {}, {light.ambient, m_twoSided ? 1.f : 0.f}, {light.diffuse, 0.f}
	};
	if (light.enabled) {
		const auto scale = std::max({std::abs(light.direction.x), std::abs(light.direction.y), std::abs(light.direction.z)});
		uniforms.direction = {glm::normalize(light.direction / scale), 1.f};
	}

	// Bind preselected resources and issue exactly one indexed draw
	state.pushViewingUniforms(command, ShaderStage::VERTEX, 0);
	SDL_PushGPUFragmentUniformData(command, 0, &uniforms, sizeof(uniforms));
	SDL_BindGPUGraphicsPipeline(pass, m_pipelines[options.texture.has_value()]);
	const SDL_GPUBufferBinding vertices{mesh.m_vertices->handle(), 0};
	const SDL_GPUBufferBinding indices{mesh.m_indices->handle(), 0};
	SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
	SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
	if (options.texture) {
		const SDL_GPUTextureSamplerBinding texture{options.texture->texture, options.texture->sampler};
		SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
	}
	SDL_DrawGPUIndexedPrimitives(
		pass, Uint32(mesh.m_indexCount), 1, 0, 0, 0
	);
}



//////
//
// Module namespace close
//

// namespace fcg
}
