
//////
//
// Includes
//

// C++ STL
#include <array>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// Framework
#include <FCG/fullscreen.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Local shorthand for encoder failure categories.
using Code = fcg::FullscreenErrorCode;

/// Construct an owned encoder diagnostic.
auto error (Code code, std::string message) -> std::unexpected<fcg::FullscreenError> {
	return std::unexpected(fcg::FullscreenError{code, std::move(message)});
}

/// Capture backend diagnostics immediately.
auto sdlError (const char *operation) -> std::unexpected<fcg::FullscreenError> {
	return error(Code::SDLFailure, std::string(operation) + ": " + SDL_GetError());
}

/// Validate portable viewport dimensions.
auto validExtent (glm::uvec2 extent) -> bool {
	return extent.x && extent.y && extent.x <= 16384 && extent.y <= 16384;
}

/// Construct a built-in fragment encoder.
auto builtin (fcg::Device &device, SDL_GPUTextureFormat format, const char *name)
	-> std::expected<fcg::FullscreenPass, fcg::FullscreenError>
{
	auto shader = fcg::res::shader(name);
	if (!shader || !shader->stage(fcg::ShaderStage::FRAGMENT))
		return error(Code::InvalidState, "Built-in fullscreen fragment shader is missing");
	return fcg::FullscreenPass::create(device, *shader->stage(fcg::ShaderStage::FRAGMENT), {.samplers=1}, format);
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

namespace fcg {



//////
//
// Class implementations
//

////
// FullscreenTarget

auto FullscreenTarget::fromTexture (
	const Texture &texture,
	Uint32 mipLevel,
	Uint32 layer,
	SDL_GPULoadOp load,
	SDL_GPUStoreOp store
) -> std::expected<FullscreenTarget, FullscreenError>
{
	if (!texture.handle())
		return error(Code::InvalidState, "Fullscreen target is empty");
	const auto &info = texture.info();
	const bool volume = info.type == SDL_GPU_TEXTURETYPE_3D;
	if (!(info.usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) || info.sample_count != SDL_GPU_SAMPLECOUNT_1
		|| mipLevel >= info.num_levels || layer >= (volume ? std::max(1u, info.layer_count_or_depth >> mipLevel) : info.layer_count_or_depth)
		|| load < SDL_GPU_LOADOP_LOAD || load > SDL_GPU_LOADOP_DONT_CARE
		|| store < SDL_GPU_STOREOP_STORE || store > SDL_GPU_STOREOP_DONT_CARE)
		return error(Code::InvalidArgument, "Fullscreen target subresource, usage, or load/store operation is invalid");
	FullscreenTarget target;
	target.color.texture = texture.handle();
	target.color.mip_level = mipLevel;
	target.color.layer_or_depth_plane = layer;
	target.color.load_op = load;
	target.color.store_op = store;
	target.format = info.format;
	target.extent = {std::max(1u, info.width >> mipLevel), std::max(1u, info.height >> mipLevel)};
	target.device = texture.device();
	return target;
}


////
// FullscreenPass

auto FullscreenPass::create (
	Device &device,
	const res::ShaderStageData &fragment,
	const ShaderResources &resources,
	SDL_GPUTextureFormat outputFormat
) -> std::expected<FullscreenPass, FullscreenError>
{
	if (fragment.stage != ShaderStage::FRAGMENT || fragment.spirv.empty())
		return error(Code::InvalidArgument, "Fullscreen shader must contain fragment SPIR-V");
	if (resources.uniformBuffers > 4 || resources.samplers > 16 || resources.storageTextures > 8 || resources.storageBuffers > 8)
		return error(Code::UnsupportedConfiguration, "Fullscreen shader exceeds portable resource limits");
	if (outputFormat <= SDL_GPU_TEXTUREFORMAT_INVALID || outputFormat > SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT
		|| !SDL_GPUTextureSupportsFormat(device.handle(), outputFormat, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET))
		return error(Code::UnsupportedConfiguration, "Fullscreen output format is not a supported color target");
	auto vertexData = res::shader("fullscreen");
	if (!vertexData || !vertexData->stage(ShaderStage::VERTEX))
		return error(Code::InvalidState, "Built-in fullscreen vertex shader is missing");
	auto *vertex = device.createShader(ShaderStage::VERTEX, vertexData->stage(ShaderStage::VERTEX)->spirv, ShaderResources{});
	if (!vertex)
		return sdlError("Creating fullscreen vertex shader");
	auto *pixel = device.createShader(ShaderStage::FRAGMENT, fragment.spirv, resources);
	if (!pixel) {
		auto failure = sdlError("Creating fullscreen fragment shader");
		SDL_ReleaseGPUShader(device.handle(), vertex);
		return failure;
	}
	SDL_GPUColorTargetDescription output{};
	output.format = outputFormat;
	SDL_GPUGraphicsPipelineCreateInfo info{};
	info.vertex_shader = vertex;
	info.fragment_shader = pixel;
	info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
	info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	info.target_info.color_target_descriptions = &output;
	info.target_info.num_color_targets = 1;
	auto *pipeline = SDL_CreateGPUGraphicsPipeline(device.handle(), &info);
	auto failure = pipeline ? std::unexpected(FullscreenError{Code::SDLFailure, {}}) : sdlError("Creating fullscreen pipeline");
	SDL_ReleaseGPUShader(device.handle(), vertex);
	SDL_ReleaseGPUShader(device.handle(), pixel);
	if (!pipeline)
		return failure;
	return FullscreenPass(device, pipeline, outputFormat, resources);
}

auto FullscreenPass::passthrough (Device &device, SDL_GPUTextureFormat outputFormat)
	-> std::expected<FullscreenPass, FullscreenError> {
	return builtin(device, outputFormat, "passthrough");
}

auto FullscreenPass::linearToSRGB (Device &device, SDL_GPUTextureFormat outputFormat)
	-> std::expected<FullscreenPass, FullscreenError> {
	if (outputFormat != SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM && outputFormat != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM)
		return error(Code::UnsupportedConfiguration, "SDR encoding requires an RGBA8 or BGRA8 UNORM output");
	return builtin(device, outputFormat, "srgb");
}

FullscreenPass::~FullscreenPass() {
	if (m_pipeline)
		SDL_ReleaseGPUGraphicsPipeline(m_device->handle(), m_pipeline);
}

FullscreenPass::FullscreenPass(FullscreenPass &&other) noexcept
	: m_device(std::exchange(other.m_device, nullptr)), m_pipeline(std::exchange(other.m_pipeline, nullptr)),
	  m_format(other.m_format), m_resources(other.m_resources)
{}

auto FullscreenPass::operator= (FullscreenPass &&other) noexcept -> FullscreenPass&
{
	if (this != &other) {
		if (m_pipeline)
			SDL_ReleaseGPUGraphicsPipeline(m_device->handle(), m_pipeline);
		m_device = std::exchange(other.m_device, nullptr);
		m_pipeline = std::exchange(other.m_pipeline, nullptr);
		m_format = other.m_format;
		m_resources = other.m_resources;
	}
	return *this;
}

auto FullscreenPass::validate (const FullscreenBindings &bindings) const -> std::expected<void, FullscreenError>
{
	if (!m_pipeline)
		return error(Code::InvalidState, "Fullscreen encoder is empty");
	if (bindings.samplers.size() != m_resources.samplers || bindings.storageTextures.size() != m_resources.storageTextures
		|| bindings.storageBuffers.size() != m_resources.storageBuffers || bindings.uniforms.size() != m_resources.uniformBuffers)
		return error(Code::InvalidArgument, "Fullscreen binding counts do not match shader resources");
	for (const auto &binding : bindings.samplers)
		if (!binding.texture || !binding.sampler || !binding.texture->handle() || !binding.sampler->handle()
			|| binding.texture->device() != m_device || binding.sampler->device() != m_device
			|| !(binding.texture->info().usage & SDL_GPU_TEXTUREUSAGE_SAMPLER)
			|| binding.texture->info().sample_count != SDL_GPU_SAMPLECOUNT_1)
			return error(Code::InvalidArgument, "Fullscreen sampled binding is invalid");
	for (const auto *texture : bindings.storageTextures)
		if (!texture || !texture->handle() || texture->device() != m_device
			|| !(texture->info().usage & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ))
			return error(Code::InvalidArgument, "Fullscreen storage texture binding is invalid");
	for (const auto *buffer : bindings.storageBuffers)
		if (!buffer || !buffer->handle() || buffer->device() != m_device
			|| !(buffer->usage() & SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ))
			return error(Code::InvalidArgument, "Fullscreen storage buffer binding is invalid");
	for (const auto block : bindings.uniforms)
		if (block.empty() || block.size() > 4096)
			return error(Code::InvalidArgument, "Fullscreen uniform blocks require 1 to 4096 bytes");
	return {};
}

auto FullscreenPass::draw (
	SDL_GPUCommandBuffer *command,
	SDL_GPURenderPass *pass,
	glm::uvec2 extent,
	const FullscreenBindings &bindings
) const -> std::expected<void, FullscreenError>
{
	if (auto valid = validate(bindings); !valid)
		return valid;
	if (!command || !pass || !validExtent(extent))
		return error(Code::InvalidArgument, "Fullscreen draw requires command, pass, and valid output dimensions");
	std::array<SDL_GPUTextureSamplerBinding, 16> samplers{};
	std::array<SDL_GPUTexture*, 8> textures{};
	std::array<SDL_GPUBuffer*, 8> buffers{};
	for (std::size_t i = 0; i < bindings.samplers.size(); ++i)
		samplers[i] = {bindings.samplers[i].texture->handle(), bindings.samplers[i].sampler->handle()};
	for (std::size_t i = 0; i < bindings.storageTextures.size(); ++i)
		textures[i] = bindings.storageTextures[i]->handle();
	for (std::size_t i = 0; i < bindings.storageBuffers.size(); ++i)
		buffers[i] = bindings.storageBuffers[i]->handle();
	SDL_BindGPUGraphicsPipeline(pass, m_pipeline);
	if (!bindings.samplers.empty())
		SDL_BindGPUFragmentSamplers(pass, 0, samplers.data(), (Uint32)bindings.samplers.size());
	if (!bindings.storageTextures.empty())
		SDL_BindGPUFragmentStorageTextures(pass, 0, textures.data(), (Uint32)bindings.storageTextures.size());
	if (!bindings.storageBuffers.empty())
		SDL_BindGPUFragmentStorageBuffers(pass, 0, buffers.data(), (Uint32)bindings.storageBuffers.size());
	for (Uint32 i = 0; i < bindings.uniforms.size(); ++i)
		(void)pushUniforms(command, ShaderStage::FRAGMENT, i, bindings.uniforms[i]);
	const SDL_GPUViewport viewport{0.f, 0.f, (float)extent.x, (float)extent.y, 0.f, 1.f};
	const SDL_Rect scissor{0, 0, (int)extent.x, (int)extent.y};
	SDL_SetGPUViewport(pass, &viewport);
	SDL_SetGPUScissor(pass, &scissor);
	SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
	return {};
}

auto FullscreenPass::record (
	SDL_GPUCommandBuffer *command,
	const FullscreenTarget &target,
	const FullscreenBindings &bindings
) const -> std::expected<void, FullscreenError>
{
	if (auto valid = validate(bindings); !valid)
		return valid;
	if (!command || !target.color.texture || target.device != m_device || target.format != m_format || !validExtent(target.extent)
		|| target.color.load_op < SDL_GPU_LOADOP_LOAD || target.color.load_op > SDL_GPU_LOADOP_DONT_CARE
		|| target.color.store_op < SDL_GPU_STOREOP_STORE || target.color.store_op > SDL_GPU_STOREOP_DONT_CARE
		|| target.color.resolve_texture)
		return error(Code::InvalidArgument, "Fullscreen target does not match the encoder");
	for (const auto &binding : bindings.samplers)
		if (binding.texture->handle() == target.color.texture)
			return error(Code::InvalidArgument, "Fullscreen texture feedback is forbidden");
	for (const auto *texture : bindings.storageTextures)
		if (texture->handle() == target.color.texture)
			return error(Code::InvalidArgument, "Fullscreen storage texture feedback is forbidden");
	auto *pass = SDL_BeginGPURenderPass(command, &target.color, 1, nullptr);
	if (!pass)
		return sdlError("Beginning fullscreen render pass");
	auto result = draw(command, pass, target.extent, bindings);
	SDL_EndGPURenderPass(pass);
	return result;
}



//////
//
// Module namespace close
//

} // namespace fcg
