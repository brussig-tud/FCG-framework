
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

// SDL3 library and shader translation
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

// FCG Framework and embedded resources
#include <FCG/device.h>
#include <FCG/render_target.h>
#include <FCG/res.h>
#include <fcg-render-shaders.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Last source produced by the real MSL translator.
std::string translated;

/// Last source produced by the real HLSL translator.
std::string translatedHlsl;

/// Properties borrowed by the last shader compilation.
SDL_PropertiesID borrowedProperties = 0;

/// Number of owned translation-property handles destroyed by Device.
unsigned destructions = 0;

/// Number of calls reaching the production translation boundary.
unsigned compilations = 0;

/// Whether to report a failure after translating the next shader.
bool failCompilation = false;

/// Fail even when assertions are disabled.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Exercise Device's MSL dispatch while retaining a real GPU device for shader ownership.
auto shaderFormats (SDL_GPUDevice*) -> SDL_GPUShaderFormat {
	return SDL_GPU_SHADERFORMAT_MSL;
}

/// Inspect the real translation, then create a shader for the device's actual backend.
auto compileShader (
	SDL_GPUDevice *device, const SDL_ShaderCross_SPIRV_Info *info,
	const SDL_ShaderCross_GraphicsShaderResourceInfo *resources, SDL_PropertiesID metadata
) -> SDL_GPUShader*
{
	// Consume the properties while their owner is still alive
	++compilations;
	borrowedProperties = info->props;
	require(metadata == 0, "Translation options leaked into metadata properties");
	const std::unique_ptr<char, decltype(&SDL_free)> msl(
		(char*)SDL_ShaderCross_TranspileMSLFromSPIRV(info), SDL_free);
	require(msl != nullptr, SDL_GetError());
	translated = msl.get();
	const std::unique_ptr<char, decltype(&SDL_free)> hlsl(
		(char*)SDL_ShaderCross_TranspileHLSLFromSPIRV(info), SDL_free);
	require(hlsl != nullptr, SDL_GetError());
	translatedHlsl = hlsl.get();

	// Return genuine backend shaders except when testing the production failure cleanup
	if (failCompilation) {
		SDL_SetError("Injected shader compilation failure");
		return nullptr;
	}
	return SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, info, resources, metadata);
}

/// Observe release at the production ownership boundary and forward to SDL.
void destroyProperties (SDL_PropertiesID properties) {
	require(properties == borrowedProperties, "Device destroyed unrelated translation properties");
	++destructions;
	SDL_DestroyProperties(properties);
}

// Anonymous namespace end
}



//////
//
// Includes
//

// Compile the production dispatch with narrow substitutions; shadercross itself is unmodified by these macros
#define SDL_GetGPUShaderFormats shaderFormats
#define SDL_ShaderCross_CompileGraphicsShaderFromSPIRV compileShader
#define SDL_DestroyProperties destroyProperties
#include "../../../core/src/device.cpp"
#undef SDL_DestroyProperties
#undef SDL_ShaderCross_CompileGraphicsShaderFromSPIRV
#undef SDL_GetGPUShaderFormats



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Verify target selection and ownership through the real Device implementation and MSL compiler.
void exercise (fcg::Device &device, std::string_view name)
{
	// Reuse the shipped shaders, including ordinary fragments that do not declare depth
	const auto shader = fcg::res::shader(fcg::render::embedded::FS, name);
	require(shader.has_value(), "Missing embedded shader");
	const bool ellipsoid = name.starts_with("ellipsoid");
	const bool line = name == "line";
	const bool textured = name.ends_with("textured");
	const fcg::RenderTargetInfo withDepth{
		SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREFORMAT_D32_FLOAT};
	const fcg::RenderTargetInfo withoutDepth{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
	const auto probe = [&] (fcg::ShaderStage stage, const fcg::RenderTargetInfo *target, bool fail=false)
	{
		// Dispatch the public overload and let Device own its optional properties through the call
		const auto data = shader->stage(stage);
		require(data.has_value(), "Missing shader stage");
		const bool fragment = stage == fcg::ShaderStage::FRAGMENT;
		const bool suppressed = fragment && target && target->depthStencilFormat == SDL_GPU_TEXTUREFORMAT_INVALID;
		const fcg::ShaderResources resources{
			.uniformBuffers = fragment ? (ellipsoid ? 2u : 1u) : 2u,
			.storageBuffers = fragment ? 0u : 4u, .samplers = fragment && textured ? 1u : 0u};
		const auto beforeDestructions = destructions;
		const auto beforeCompilations = compilations;
		failCompilation = fail;
		auto *result = target ? device.createShader(stage, data->spirv, resources, *target)
			: device.createShader(stage, data->spirv, resources);
		const auto release = [&] (SDL_GPUShader *value) { SDL_ReleaseGPUShader(device.handle(), value); };
		const std::unique_ptr<SDL_GPUShader, decltype(release)> owner(result, release);
		failCompilation = false;

		// Check the generated interface, successful native ownership, and cleanup on both return paths
		require((result != nullptr) == !fail, SDL_GetError());
		require(compilations == beforeCompilations + 1, "Device bypassed the MSL translation path");
		require((borrowedProperties != 0) == suppressed, "Device configured unnecessary or missing properties");
		require(destructions == beforeDestructions + (suppressed ? 1u : 0u), "Translation properties leaked");
		if (fragment && ellipsoid) {
			require((translated.find("[[depth(") != std::string::npos) == !suppressed, "Wrong fragment-depth interface");
			require(translated.find("sample_perspective") != std::string::npos
				&& translated.find("sample_no_perspective") != std::string::npos, "Sample interpolation changed");
		}
		if (line && fragment) {
			require((translated.find("[[depth(") != std::string::npos) == !suppressed,
				"Line shader has the wrong fragment-depth interface");
			require(translated.find("no_perspective") != std::string::npos,
				"Line shader lost its screen-linear interpolation");
			require(translatedHlsl.find("noperspective") != std::string::npos
				&& translatedHlsl.find("SV_Depth") != std::string::npos,
				"Line HLSL lost its interpolation or depth interface");
		}
		require(translated.find(fragment ? "[[color(0)]]" : "[[position]]") != std::string::npos,
			"Shader lost its color or position output");
	};

	// A depth-free call must not affect subsequent default or depth-attached calls
	probe(fcg::ShaderStage::FRAGMENT, nullptr);
	probe(fcg::ShaderStage::FRAGMENT, &withDepth);
	probe(fcg::ShaderStage::FRAGMENT, &withoutDepth);
	probe(fcg::ShaderStage::FRAGMENT, nullptr);
	probe(fcg::ShaderStage::VERTEX, &withoutDepth);
	probe(fcg::ShaderStage::FRAGMENT, &withoutDepth, true);
	probe(fcg::ShaderStage::FRAGMENT, &withDepth);
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Exercise target-aware shader creation on every native backend, including MSL translation on Linux.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	int result = 0;
	try {
		auto device = fcg::Device::create();
		require(device.has_value(), SDL_GetError());
		for (const auto name : {"primitive", "textured", "ellipsoid", "ellipsoid_textured", "line"})
			exercise(*device, name);
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
