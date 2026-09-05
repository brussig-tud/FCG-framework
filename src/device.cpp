
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
		SDL_WaitForGPUIdle(m_handle);

		// Release all window claims
		for (auto *window : m_claimedWindows)
			window->unclaim(*this);

		// Clean up
		SDL_DestroyGPUDevice(m_handle);
	}
}

void Device::waitIdle () const {
	SDL_WaitForGPUIdle(m_handle);
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
	ShaderStage stage, std::span<const std::byte> spirv, std::string_view entrypoint
) const -> SDL_GPUShader*
{
	if (stage == ShaderStage::COMPUTE) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating compute shaders is not supported yet");
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

	// The resource counts the shader uses must be known for pipeline creation – reflect them from the
	// bytecode, since the build system does not track them
	SDL_ShaderCross_GraphicsShaderMetadata *meta =
		SDL_ShaderCross_ReflectGraphicsSPIRV(info.bytecode, info.bytecode_size, 0);
	SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
		m_handle, &info, meta ? &meta->resource_info : nullptr, 0
	);
	SDL_free(meta);
	if (!shader)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Transpiling a shader failed: %s", SDL_GetError());
	return shader;
}


//////
//
// Module namespace close
//

} // namespace fcg
