
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <string>
#include <stdexcept>
#include <optional>
#include <utility>

// FCG Framework
// [includes]
#include <FCG/applet.h>
#include <FCG/run.h>
#include <FCG/player.h>
#include <FCG/res.h>
// [includes]



//////
//
// Classes
//

// [first_applet]
class ExampleApplet : public fcg::Applet
{
public:
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string instanceName = "Example";
		return instanceName;
	}

	void recreatePipelines (fcg::Device &device, fcg::Player &player) override {
		// Create or replace scene pipelines for player.mainRenderTargetInfo() here.
	}

	void init (fcg::Device &device, fcg::Player &player) override {
		// Create buffers, textures and other resources independent of the scene format here.
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}

	void gui (fcg::Device &device, fcg::Player &player) override {}

	void update (fcg::Device &device, fcg::Player &player, float dt) override {}

	void render (
		fcg::Device &device, fcg::RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, fcg::Player &player
	) override {
		// Record draw calls into renderPass here.
	}
};
// [first_applet]

/// An applet that negotiates floating-point scene storage before building a triangle pipeline.
// [scene_format_applet]
class FloatSceneApplet : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Release the pipeline while its device is still alive.
	~FloatSceneApplet() override {
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(gpu->handle(), pipeline);
	}


	////
	// Interface: fcg::Applet

	/// Return the applet's display name.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string label = "Floating-point scene";
		return label;
	}

	/// Request RGBA16-float storage when supported for the scene's combined usage.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		gpu = &device;
		const auto format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
		if (SDL_GPUTextureSupportsFormat(device.handle(), format, SDL_GPU_TEXTURETYPE_2D,
			SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)
			&& SDL_GPUTextureSupportsSampleCount(device.handle(), format, SDL_GPU_SAMPLECOUNT_1))
			if (auto result = player.setSceneFormat(format); !result)
				throw std::runtime_error(result.error().message);
	}

	/// Create or replace a pipeline for the format selected after all applets initialize.
	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(device.handle(), std::exchange(pipeline, nullptr));
		const auto target = player.mainRenderTargetInfo().value();
		const auto shader = fcg::res::shader("triangle").value();
		const auto release = [&device] (SDL_GPUShader *stage) { SDL_ReleaseGPUShader(device.handle(), stage); };
		const std::unique_ptr<SDL_GPUShader, decltype(release)> vertex(
			device.createShader(fcg::ShaderStage::VERTEX, shader.stage(fcg::ShaderStage::VERTEX)->spirv, 0), release
		);
		const std::unique_ptr<SDL_GPUShader, decltype(release)> fragment(
			device.createShader(fcg::ShaderStage::FRAGMENT, shader.stage(fcg::ShaderStage::FRAGMENT)->spirv, 0), release
		);
		if (!vertex || !fragment)
			throw std::runtime_error("Could not create triangle shaders");
		const SDL_GPUColorTargetDescription color{.format = target.colorFormat};
		SDL_GPUGraphicsPipelineCreateInfo info{};
		info.vertex_shader = vertex.get();
		info.fragment_shader = fragment.get();
		info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		info.multisample_state.sample_count = target.sampleCount;
		info.target_info = {&color, 1, target.depthStencilFormat, true};
		pipeline = SDL_CreateGPUGraphicsPipeline(device.handle(), &info);
		if (!pipeline)
			throw std::runtime_error(SDL_GetError());
	}

	/// Receive size changes without changing the pipeline.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {}

	/// Leave GUI construction to other applets.
	void gui (fcg::Device&, fcg::Player&) override {}

	/// Leave animation to other applets.
	void update (fcg::Device&, fcg::Player&, float) override {}

	/// Draw with the pipeline matching the effective scene format.
	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass *pass, SDL_GPUCommandBuffer*, fcg::Player&
	) override {
		SDL_BindGPUGraphicsPipeline(pass, pipeline);
		SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
	}


private:

	////
	// Fields

	/// Borrowed GPU device, which outlives this applet.
	fcg::Device *gpu = nullptr;

	/// Owned pipeline for the effective scene format.
	SDL_GPUGraphicsPipeline *pipeline = nullptr;
};
// [scene_format_applet]




//////
//
// Function implementations
//

// [run_single]
int main ()
{
	return fcg::run<ExampleApplet>();
}
// [run_single]
