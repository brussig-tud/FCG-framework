
//////
//
// Includes
//

// C++ STL
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

// SDL3 library and shader translation
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

// FCG Framework
#include <FCG/texture.h>
#include <FCG/Render/wire_box_renderer.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Operation selected for an injected failure.
enum class Failure
{
	/// Forward all operations.
	None,

	/// Fail the first geometry-buffer allocation.
	Index,

	/// Fail the second fallback-buffer allocation.
	Dummy,

	/// Fail upload staging allocation.
	Transfer,

	/// Fail upload command acquisition.
	Acquire,

	/// Cancel the upload before reporting submission failure.
	Submit,

	/// Fail the first shader creation.
	Vertex,

	/// Fail the second shader creation.
	Fragment,

	/// Fail pipeline creation after both shaders exist.
	Pipeline
};

/// Selected failure boundary.
Failure failure = Failure::None;

/// Number of buffers owned through the instrumented Core implementation.
int buffers = 0;

/// Number of shaders awaiting release.
int shaders = 0;

/// Number of graphics pipelines awaiting release.
int pipelines = 0;

/// Buffer creations attempted during the current factory call.
unsigned bufferAttempts = 0;

/// Shader creations attempted during the current factory call.
unsigned shaderAttempts = 0;

/// Transfer-buffer allocations attempted by uploads.
unsigned transferAttempts = 0;

/// Pipeline creations attempted by renderer factories.
unsigned pipelineAttempts = 0;

/// Buffer uploads recorded by production owners.
unsigned uploadAttempts = 0;

/// Indexed draws observed in the active pass.
unsigned draws = 0;

/// Instance count of the last indexed draw.
Uint32 lastInstances = 0;

/// Fail independently of assertions.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Create a diagnostic for the selected operational failure.
auto injected () -> std::nullptr_t {
	SDL_SetError("Injected wire-box resource failure");
	return nullptr;
}

/// Observe buffer ownership and fail either fixed allocation.
auto createBuffer (SDL_GPUDevice *device, const SDL_GPUBufferCreateInfo *info) -> SDL_GPUBuffer*
{
	++bufferAttempts;
	if ((failure == Failure::Index && bufferAttempts == 1) || (failure == Failure::Dummy && bufferAttempts == 2))
		return injected();
	auto *result = SDL_CreateGPUBuffer(device, info);
	buffers += result != nullptr;
	return result;
}

/// Observe buffer cleanup through the production owner.
void releaseBuffer (SDL_GPUDevice *device, SDL_GPUBuffer *buffer) {
	--buffers;
	SDL_ReleaseGPUBuffer(device, buffer);
}

/// Fail upload staging while preserving the real transfer implementation.
auto createTransfer (SDL_GPUDevice *device, const SDL_GPUTransferBufferCreateInfo *info) -> SDL_GPUTransferBuffer* {
	++transferAttempts;
	return failure == Failure::Transfer ? injected() : SDL_CreateGPUTransferBuffer(device, info);
}

/// Fail upload command acquisition.
auto acquire (SDL_GPUDevice *device) -> SDL_GPUCommandBuffer* {
	return failure == Failure::Acquire ? injected() : SDL_AcquireGPUCommandBuffer(device);
}

/// Consume recorded upload commands before reporting failure.
auto submit (SDL_GPUCommandBuffer *command) -> bool
{
	if (failure == Failure::Submit) {
		SDL_CancelGPUCommandBuffer(command);
		injected();
		return false;
	}
	return SDL_SubmitGPUCommandBuffer(command);
}

/// Decide whether this shader creation is the injected boundary.
auto failShader () -> bool {
	++shaderAttempts;
	return (failure == Failure::Vertex && shaderAttempts == 1)
		|| (failure == Failure::Fragment && shaderAttempts == 2);
}

/// Observe native SPIR-V shader creation.
auto createShader (SDL_GPUDevice *device, const SDL_GPUShaderCreateInfo *info) -> SDL_GPUShader* {
	if (failShader())
		return injected();
	auto *result = SDL_CreateGPUShader(device, info);
	shaders += result != nullptr;
	return result;
}

/// Observe translated shader creation on non-SPIR-V backends.
auto compileShader (
	SDL_GPUDevice *device, const SDL_ShaderCross_SPIRV_Info *info,
	const SDL_ShaderCross_GraphicsShaderResourceInfo *resources, SDL_PropertiesID properties
) -> SDL_GPUShader*
{
	if (failShader())
		return injected();
	auto *result = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, info, resources, properties);
	shaders += result != nullptr;
	return result;
}

/// Observe temporary shader release after success and failure.
void releaseShader (SDL_GPUDevice *device, SDL_GPUShader *shader) {
	--shaders;
	SDL_ReleaseGPUShader(device, shader);
}

/// Fail pipeline creation after shader ownership has been established.
auto createPipeline (SDL_GPUDevice *device, const SDL_GPUGraphicsPipelineCreateInfo *info) -> SDL_GPUGraphicsPipeline* {
	++pipelineAttempts;
	if (failure == Failure::Pipeline)
		return injected();
	auto *result = SDL_CreateGPUGraphicsPipeline(device, info);
	pipelines += result != nullptr;
	return result;
}

/// Observe final pipeline ownership cleanup.
void releasePipeline (SDL_GPUDevice *device, SDL_GPUGraphicsPipeline *pipeline) {
	--pipelines;
	SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
}

/// Observe data uploads separately from resource allocation.
void uploadBuffer (
	SDL_GPUCopyPass *pass, const SDL_GPUTransferBufferLocation *source, const SDL_GPUBufferRegion *destination,
	bool cycle
) {
	++uploadAttempts;
	SDL_UploadToGPUBuffer(pass, source, destination, cycle);
}

/// Verify the complete edge template is drawn once with portable zero offsets.
void drawIndexed (SDL_GPURenderPass *pass, Uint32 indices, Uint32 instances, Uint32 first, Sint32 offset, Uint32 base) {
	require(indices == 72 && first == 0 && offset == 0 && base == 0, "Wrong edge-template draw");
	++draws;
	lastInstances = instances;
	SDL_DrawGPUIndexedPrimitives(pass, indices, instances, first, offset, base);
}

// Anonymous namespace end
}



//////
//
// Includes
//

// Compile production resource owners with narrow SDL boundary substitutions
#define SDL_UploadToGPUBuffer uploadBuffer
#define SDL_CreateGPUBuffer createBuffer
#define SDL_ReleaseGPUBuffer releaseBuffer
#define SDL_CreateGPUTransferBuffer createTransfer
#define SDL_AcquireGPUCommandBuffer acquire
#define SDL_SubmitGPUCommandBuffer ::submit
#include "../../../core/src/buffer.cpp"
#undef SDL_SubmitGPUCommandBuffer
#undef SDL_AcquireGPUCommandBuffer
#undef SDL_CreateGPUTransferBuffer
#undef SDL_ReleaseGPUBuffer
#undef SDL_CreateGPUBuffer
#undef SDL_UploadToGPUBuffer

// Exercise both native and translated production shader creation paths
#define SDL_CreateGPUShader ::createShader
#define SDL_ShaderCross_CompileGraphicsShaderFromSPIRV compileShader
#include "../../../core/src/device.cpp"
#undef SDL_ShaderCross_CompileGraphicsShaderFromSPIRV
#undef SDL_CreateGPUShader

// Exercise the independently owned renderer and its draw boundary
#define SDL_ReleaseGPUShader releaseShader
#define SDL_CreateGPUGraphicsPipeline createPipeline
#define SDL_ReleaseGPUGraphicsPipeline releasePipeline
#define SDL_DrawGPUIndexedPrimitives drawIndexed
#include "../src/wire_box_renderer.cpp"
#undef SDL_DrawGPUIndexedPrimitives
#undef SDL_ReleaseGPUGraphicsPipeline
#undef SDL_CreateGPUGraphicsPipeline
#undef SDL_ReleaseGPUShader



//////
//
// Functions
//

/// Verify operational failures release partial resources and subsequent valid creation recovers.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	int result = 0;
	try
	{
		auto device = fcg::Device::create();
		require(device.has_value(), SDL_GetError());
		const fcg::RenderTargetInfo target{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
		for (auto point : {Failure::Index, Failure::Dummy, Failure::Transfer, Failure::Acquire,
			Failure::Submit, Failure::Vertex, Failure::Fragment, Failure::Pipeline, Failure::None})
		{
			failure = point;
			bufferAttempts = shaderAttempts = 0;
			/* Create and release one complete or partially initialized renderer */ {
				auto renderer = fcg::WireBoxRenderer::create(*device, target);
				require(bool(renderer) == (point == Failure::None), "Wrong creation result");
				if (!renderer)
					require(renderer.error().code == fcg::RenderErrorCode::SDLFailure
						&& renderer.error().message.find("Injected") != std::string::npos, "Lost failure diagnostic");
			}
			require(buffers == 0 && shaders == 0 && pipelines == 0, "Partial creation leaked a resource");
		}

		// Real render-pass commands verify one draw per nonempty box range and no draw-time allocations
		failure = Failure::None;
		auto renderer = fcg::WireBoxRenderer::create(*device, target);
		require(renderer.has_value(), SDL_GetError());
		fcg::PrimitiveAttributes attributes(*device);
		const std::array positions{glm::vec4(-.5f,0,.5f,1), glm::vec4(.5f,0,.5f,1)};
		require(attributes.setAttributes([&] (auto &update) {
			update.template set<fcg::Attribute::Position>(std::span(positions));
		}).has_value(), "Position upload failed");
		SDL_GPUTextureCreateInfo info{};
		info.type = SDL_GPU_TEXTURETYPE_2D;
		info.format = target.colorFormat;
		info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		info.width = info.height = 32;
		info.layer_count_or_depth = info.num_levels = 1;
		info.sample_count = SDL_GPU_SAMPLECOUNT_1;
		auto texture = fcg::Texture::create(*device, info);
		require(texture.has_value(), SDL_GetError());
		auto *command = SDL_AcquireGPUCommandBuffer(device->handle());
		require(command, SDL_GetError());
		SDL_GPUColorTargetInfo color{};
		color.texture = texture->handle();
		color.load_op = SDL_GPU_LOADOP_CLEAR;
		color.store_op = SDL_GPU_STOREOP_STORE;
		auto *pass = SDL_BeginGPURenderPass(command, &color, 1, nullptr);
		require(pass, SDL_GetError());
		fcg::RenderState state(*device, {32,32});
		const std::array before{bufferAttempts, transferAttempts, shaderAttempts, pipelineAttempts, uploadAttempts};
		require(renderer->draw(attributes,state,command,pass).has_value(), "Draw failed");
		require(draws == 1 && lastInstances == 2, "Boxes were not instanced together");
		require(renderer->draw(attributes,state,command,pass,{}, {1,1}).has_value(), "Range draw failed");
		require(draws == 2 && lastInstances == 1, "Range does not count boxes");
		require(renderer->draw(attributes,state,command,pass,{}, {2,0}).has_value(), "Empty draw failed");
		require(draws == 2 && std::array{bufferAttempts, transferAttempts, shaderAttempts, pipelineAttempts, uploadAttempts}
			== before, "Draw allocated/uploaded or empty range recorded commands");
		SDL_EndGPURenderPass(pass);
		require(SDL_SubmitGPUCommandBuffer(command), SDL_GetError());
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
