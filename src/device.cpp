
//////
//
// Includes
//

// SDL3 library
#include <SDL3/SDL.h>

// SDL_shadercross (runtime SPIR-V translation for non-Vulkan backends)
#include <SDL3_shadercross/SDL_shadercross.h>

// C++ STL
#include <mutex>
#include <span>
#include <string>

// Local includes
#include "FCG/window.h"
#include "FCG/device.h"



//////
//
// Module namespace open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// Device

auto Device::create () -> std::optional<Device>
{
	// Create the GPU device. All common shader formats are offered so that SDL picks the best available
	// backend for the platform. Debug builds enable the GPU debug mode, which performs additional
	// validation of SDL GPU API usage – particularly useful in a teaching context.
	SDL_GPUDevice *handle = SDL_CreateGPUDevice(
		static_cast<SDL_GPUShaderFormat>(
			SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL
		),
	#ifdef NDEBUG
		false,
	#else
		true,
	#endif
		nullptr
	);
	if (!handle) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating the GPU device failed: %s", SDL_GetError());
		return std::nullopt;
	}

	// Assemble and return the object
	return std::make_optional<Device>(PrivateConstructorKey{}, handle);
}

Device::~Device ()
{
	if (m_handle)
	{
		// Wait for the GPU to finish all pending work before destroying the device
		const bool idle = SDL_WaitForGPUIdle(m_handle);
		collectRetiredFences(idle);

		// Release all window claims
		for (auto *window : m_claimedWindows)
			window->unclaim(*this);

		// Clean up
		SDL_DestroyGPUDevice(m_handle);
	}
}

void Device::waitIdle () const {
	collectRetiredFences(SDL_WaitForGPUIdle(m_handle));
}

void Device::retireFence (std::unique_ptr<RetiredFence> fence) {
	if (!fence || !fence->handle)
		return;
	if (SDL_QueryGPUFence(m_handle, fence->handle)) {
		SDL_ReleaseGPUFence(m_handle, fence->handle);
		return;
	}
	std::lock_guard lock(m_fenceMutex);
	fence->next = std::move(m_retiredFences);
	m_retiredFences = std::move(fence);
}

void Device::collectRetiredFences (bool idle) const {
	std::lock_guard lock(m_fenceMutex);
	auto *link = &m_retiredFences;
	while (*link) {
		if (idle || SDL_QueryGPUFence(m_handle, (*link)->handle)) {
			auto fence = std::move(*link);
			*link = std::move(fence->next);
			SDL_ReleaseGPUFence(m_handle, fence->handle);
		} else {
			link = &(*link)->next;
		}
	}
}

auto Device::claimWindow (std::unique_ptr<Window> &window) -> bool {
	if (!window->claim(*this))
		return false;
	m_claimedWindows.insert(window.get());
	return true;
}

void Device::unclaimWindow (std::unique_ptr<Window> &window) {
	window->unclaim(*this);
	m_claimedWindows.erase(window.get());
}

auto Device::createShader (
	ShaderStage stage, std::span<const std::byte> spirv, unsigned numUniformBlocks, std::string_view entrypoint
) const -> SDL_GPUShader*
{
	return createShader(stage, spirv, ShaderResources{.uniformBuffers = numUniformBlocks}, entrypoint);
}

auto Device::createShader (
	ShaderStage stage, std::span<const std::byte> spirv, const ShaderResources &resources, std::string_view entrypoint
) const -> SDL_GPUShader*
{
	if (stage != ShaderStage::VERTEX && stage != ShaderStage::FRAGMENT) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating compute shaders is not supported yet");
		return nullptr;
	}
	if (resources.uniformBuffers > 4) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "A shader stage supports at most four uniform blocks");
		return nullptr;
	}
	if (spirv.empty()) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot create a shader from empty SPIR-V bytecode");
		return nullptr;
	}

	const SDL_GPUShaderStage sdlStage = (stage == ShaderStage::VERTEX)
		? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
	const std::string entry {entrypoint};

	// On Vulkan backends we can use the SPIR-V directly
	if (SDL_GetGPUShaderFormats(m_handle) & SDL_GPU_SHADERFORMAT_SPIRV) {
		SDL_GPUShaderCreateInfo info {};
		info.code_size = spirv.size();
		info.code = reinterpret_cast<const Uint8*>(spirv.data());
		info.entrypoint = entry.c_str();
		info.format = SDL_GPU_SHADERFORMAT_SPIRV;
		info.stage = sdlStage;
		info.num_uniform_buffers = resources.uniformBuffers;
		info.num_storage_buffers = resources.storageBuffers;
		info.num_storage_textures = resources.storageTextures;
		info.num_samplers = resources.samplers;

		SDL_GPUShader *shader = SDL_CreateGPUShader(m_handle, &info);
		if (!shader)
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating a SPIR-V shader failed: %s", SDL_GetError());
		return shader;
	}

	// All other backends (Metal, Direct3D 12) receive runtime-translated SPIR-V via SDL_shadercross
	static std::once_flag shadercrossInit;
	std::call_once(shadercrossInit, [] { SDL_ShaderCross_Init(); });

	SDL_ShaderCross_SPIRV_Info info {};
	info.bytecode = reinterpret_cast<const Uint8*>(spirv.data());
	info.bytecode_size = spirv.size();
	info.entrypoint = entry.c_str();
	info.shader_stage = (stage == ShaderStage::VERTEX)
		? SDL_SHADERCROSS_SHADERSTAGE_VERTEX : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;

	// Use the same explicit resource contract on every backend.
	const SDL_ShaderCross_GraphicsShaderResourceInfo resourceInfo {
		.num_samplers = resources.samplers,
		.num_storage_textures = resources.storageTextures,
		.num_storage_buffers = resources.storageBuffers,
		.num_uniform_buffers = resources.uniformBuffers
	};
	SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(m_handle, &info, &resourceInfo, 0);
	if (!shader)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Transpiling a shader failed: %s", SDL_GetError());
	return shader;
}


//////
//
// Module namespace close
//

} // namespace fcg
