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
//
// Includes
//

// C++ STL
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
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



//////
//
// Classes
//

/// Dummy applet for the lifecycle probe.
class LifecycleProbe final : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Report successful drawing through creator-owned state.
	explicit LifecycleProbe (bool *rendered, unsigned frames)
		: remainingFrames(frames), rendered(*rendered)
	{}

	/// The framework destroys applets before the GPU device, so releasing our pipeline here is safe.
	~LifecycleProbe () override {
		rendered = rendered && depthChecks >= 2 && resized;
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
	}


	////
	// Interface: fcg::Applet

	[[nodiscard]] auto name () const -> const std::string& override {
		const static std::string name = "Lifecycle Probe";
		return name;
	}

	void init (fcg::Device &device, fcg::Player &player) override
	{
		// The frame loop blocks waiting for events, which never arrive in a headless test environment - force redraws
		// so frames actually render.
		player.pushContinuousRedraw();

		// Build a pipeline from the embedded 'triangle' shader. A failure leaves rendered false, so main fails the
		// test even if the framework exits normally after the requested number of frames.
		auto shader = fcg::res::shader("triangle");
		if (!shader) {
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Embedded shader 'triangle' not found");
			return;
		}
		const auto vertex = shader->stage(fcg::ShaderStage::VERTEX);
		const auto fragment = shader->stage(fcg::ShaderStage::FRAGMENT);
		if (!vertex || !fragment) {
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Embedded shader 'triangle' is missing a graphics stage");
			return;
		}
		// Release temporary shaders after pipeline creation, also covering partial initialization and failure paths.
		const auto shaderReleaser = [handle = device.handle()] (SDL_GPUShader *shader) {
			SDL_ReleaseGPUShader(handle, shader);
		};
		using TemporaryShaderPtr = std::unique_ptr<SDL_GPUShader, decltype(shaderReleaser)>;
		const TemporaryShaderPtr vertexShader(
			device.createShader(fcg::ShaderStage::VERTEX, vertex->spirv, 0), shaderReleaser
		);
		const TemporaryShaderPtr fragmentShader(
			device.createShader(fcg::ShaderStage::FRAGMENT, fragment->spirv, 0), shaderReleaser
		);
		if (!vertexShader || !fragmentShader)
			return;  // createShader already logged the error

		SDL_GPUColorTargetDescription colorTarget {};
		colorTarget.format = player.swapchainFormat();
		SDL_GPUGraphicsPipelineTargetInfo targetInfo {};
		targetInfo.color_target_descriptions = &colorTarget;
		targetInfo.num_color_targets = 1;
		// The applet render pass includes depth even though this probe does not enable depth testing or writing.
		targetInfo.has_depth_stencil_target = true;
		targetInfo.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

		SDL_GPUGraphicsPipelineCreateInfo pipelineInfo {};
		pipelineInfo.vertex_shader = vertexShader.get();
		pipelineInfo.fragment_shader = fragmentShader.get();
		pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pipelineInfo.target_info = targetInfo;

		pipeline = SDL_CreateGPUGraphicsPipeline(device.handle(), &pipelineInfo);
		if (!pipeline)
			SDL_LogCritical(SDL_LOG_CATEGORY_ERROR, "Creating the pipeline failed: %s", SDL_GetError());
		gpuDevice = device.handle();
	}

	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {
		// Resize replaces the texture with uninitialized storage; render it before asking for its previous contents.
		depthToken.reset();
		readbackCooldown = 2;
	}

	void gui (fcg::Device&, fcg::Player&) override {}

	void update (fcg::Device&, fcg::Player &player, float) override {
		// Collect each token before requesting another. The frame loop has already waited for pending downloads.
		if (depthToken) {
			const auto view = player.getDepthReadbackResult(*depthToken);
			const auto extent = view.extent();
			const auto depth = view.texel({extent.x / 2, extent.y / 2});
			if (depth != 1.f)
				throw std::runtime_error("Depth readback differs at update " + std::to_string(updates)
					+ ": " + std::to_string(depth));
			++depthChecks;
			depthToken.reset();
		}
		++updates;
		if (updates == 10) {
			// Resize between readbacks, so the next frame recreates both depth texture and transfer storage.
			int count = 0;
			auto **windows = SDL_GetWindows(&count);
			resized = windows && count > 0 && SDL_SetWindowSize(windows[0], 480, 320);
			SDL_free(windows);
			if (!resized)
				throw std::runtime_error("Could not resize lifecycle test window");
		} else if (readbackCooldown) {
			--readbackCooldown;
		} else if (rendered && updates > 1) {
			depthToken = player.scheduleDepthReadback();
		}
		if (remainingFrames && --remainingFrames == 0)
			player.requestClose();
	}

	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass *renderPass, SDL_GPUCommandBuffer*, fcg::Player&
	) override {
		if (!pipeline)
			return;
		SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
		rendered = true;
	}


	////
	// Fields

	/// The device that owns our pipeline; outlives the applet.
	SDL_GPUDevice *gpuDevice = nullptr;

	/// The previous frame's depth-download token, consumed before requesting another.
	std::optional<uint64_t> depthToken;

	/// Number of verified depth downloads and update iterations.
	unsigned depthChecks = 0, updates = 0;

	/// Newly allocated depth textures need a rendered frame before readback.
	unsigned readbackCooldown = 2;

	/// Whether the resize operation succeeded during the lifecycle run.
	bool resized = false;

	/// Our test pipeline
	SDL_GPUGraphicsPipeline *pipeline = nullptr;

	/// For tracking when we need to stop and report a test result.
	unsigned remainingFrames;

	/// Whether at least one draw used a successfully created shader pipeline.
	bool &rendered;
};



//////
//
// Functions
//

/// The test program entry point.
int main ()
{
	// Frames to render before automatic shutdown; 0 runs until the window is closed. The CTest run leaves this unset
	// (30 frames) - set it manually for a visual check, e.g.
	//   FCG_PROBE_FRAMES=0 ./build/local-debug/bin/lifecycle-smoke
	const char *env = std::getenv("FCG_PROBE_FRAMES");
	const unsigned frames = env ? std::strtoul(env, nullptr, 10) : 30;

	// Setup
	bool rendered = false;
	std::vector<std::unique_ptr<fcg::Applet>> applets;
	applets.push_back(std::make_unique<LifecycleProbe>(&rendered, frames));
	const auto result = fcg::run(
		std::move(applets),
		{.mainWindowTitle = "FCG Lifecycle Probe"}
	);

	// Done, report test result
	return result != 0 ? result : (rendered ? EXIT_SUCCESS : EXIT_FAILURE);
}
