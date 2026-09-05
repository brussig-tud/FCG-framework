//////
//
// Smoke test for the framework lifecycle: runs a trivial applet through
// fcg::run(), forcing continuous redraws and requesting shutdown after a fixed
// number of frames. Driven by the 'lifecycle-smoke' CTest (see tests/).
//
// The probe additionally loads the embedded 'triangle' shader, builds a
// graphics pipeline from it and draws a full-screen triangle each frame, so
// the whole shader path (embed -> SPIR-V -> pipeline) is exercised headlessly.
// All test logic lives in this test-owned applet - the framework itself
// contains no test hooks.
//
//////

// C++ STL
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/applet.h>
#include <FCG/device.h>
#include <FCG/player.h>
#include <FCG/render_state.h>
#include <FCG/res.h>
#include <FCG/run.h>

namespace {

class LifecycleProbe final : public fcg::Applet {
public:

	explicit LifecycleProbe (unsigned frames) : m_remainingFrames(frames) {}


	////
	// Applet interface

	auto name () -> std::string& override { return m_name; }

	void init (fcg::Device &device, fcg::Player &player) override {
		// The frame loop blocks waiting for events, which never arrive in a
		// headless test environment - force redraws so frames actually render.
		player.pushContinuousRedraw();

		// Build a pipeline from the embedded 'triangle' shader. On failure we log critically and keep
		// running, so the CTest run fails via its timeout (loudly, with the log visible).
		auto shader = fcg::res::shader("triangle");
		if (!shader) {
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Embedded shader 'triangle' not found");
			return;
		}
		auto *vertexShader = device.createShader(
			fcg::ShaderStage::VERTEX, shader->stage(fcg::ShaderStage::VERTEX)->spirv
		);
		auto *fragmentShader = device.createShader(
			fcg::ShaderStage::FRAGMENT, shader->stage(fcg::ShaderStage::FRAGMENT)->spirv
		);
		if (!vertexShader || !fragmentShader)
			return;  // createShader already logged the error

		SDL_GPUColorTargetDescription colorTarget {};
		colorTarget.format = player.swapchainFormat();
		SDL_GPUGraphicsPipelineTargetInfo targetInfo {};
		targetInfo.color_target_descriptions = &colorTarget;
		targetInfo.num_color_targets = 1;

		SDL_GPUGraphicsPipelineCreateInfo pipelineInfo {};
		pipelineInfo.vertex_shader = vertexShader;
		pipelineInfo.fragment_shader = fragmentShader;
		pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pipelineInfo.target_info = targetInfo;

		m_pipeline = SDL_CreateGPUGraphicsPipeline(device.handle(), &pipelineInfo);
		if (!m_pipeline)
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Creating the pipeline failed: %s", SDL_GetError());
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}

	void gui (fcg::Device &device, fcg::Player &player) override {}

	void update (fcg::Device &device, fcg::Player &player) override {
		if (m_remainingFrames && --m_remainingFrames == 0)
			player.requestClose();
	}

	void render (
		fcg::Device &device, fcg::RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, fcg::Player &player
	) override {
		if (!m_pipeline)
			return;
		SDL_BindGPUGraphicsPipeline(renderPass, m_pipeline);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
	}


	////
	// State

	std::string m_name = "Lifecycle Probe";
	unsigned m_remainingFrames;
	SDL_GPUGraphicsPipeline *m_pipeline = nullptr;
};

} // namespace



////
// Entry point

int main () {
	// Frames to render before automatic shutdown; 0 runs until the window is closed. The CTest run
	// leaves this unset (30 frames) - set it manually for a visual check, e.g.
	//   FCG_PROBE_FRAMES=0 ./build/local-debug/bin/lifecycle-smoke
	const char *env = std::getenv("FCG_PROBE_FRAMES");
	const unsigned frames = env ? std::strtoul(env, nullptr, 10) : 30;

	std::vector<std::unique_ptr<fcg::Applet>> applets;
	applets.push_back(std::make_unique<LifecycleProbe>(frames));
	return fcg::run(
		std::move(applets),
		{.mainWindowTitle = "FCG Lifecycle Probe"}
	);
}
