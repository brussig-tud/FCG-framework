
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <span>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
// [includes]
#include <FCG/device.h>
#include <FCG/render_target.h>
#include <FCG/window.h>
// [includes]



//////
//
// Function implementations
//

// [device_setup]
void setup (std::unique_ptr<fcg::Window> &window)
{
	auto device = fcg::Device::create();
	if (!device)
		throw std::runtime_error("No GPU device available");

	// Claim the window so that frames can be acquired from it.
	if (!device->claimWindow(window))
		throw std::runtime_error("Claiming the window failed");

	// Keep the device alive for as long as it is used; the window must outlive it.
}
// [device_setup]

// [shader_creation]
[[nodiscard]] auto makeFragmentShader (const fcg::Device &device, std::span<const std::byte> spirv)
	-> SDL_GPUShader*
{
	// One uniform block, no samplers or storage resources.
	return device.createShader(
		fcg::ShaderStage::FRAGMENT, spirv, fcg::ShaderResources{.uniformBuffers = 1}, "main"
	);
}
// [shader_creation]

/// Create a fragment shader for pipelines with the given depth-attachment configuration.
// [targeted_shader_creation]
[[nodiscard]] auto makeTargetedFragmentShader (
	const fcg::Device &device, std::span<const std::byte> spirv, const fcg::RenderTargetInfo &target
) -> SDL_GPUShader* {
	// One uniform block, no samplers or storage resources
	return device.createShader(
		fcg::ShaderStage::FRAGMENT, spirv, fcg::ShaderResources{.uniformBuffers = 1}, target
	);
}
// [targeted_shader_creation]
