
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstdlib>
#include <format>
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

/// \brief Exercise rendering, depth downloads, resizing and shutdown through <tt>fcg::run</tt>.
///
/// Loads the embedded triangle shader and draws each frame while leaving depth at its clear value. The
/// lifecycle-smoke test verifies depth before resizing and after both shrinking and growing the window.
class LifecycleProbe final : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Report successful drawing through creator-owned state.
	explicit LifecycleProbe(bool *rendered, unsigned frames)
		: remainingFrames(frames), rendered(*rendered)
	{}

	/// The framework destroys applets before the GPU device, so releasing our pipeline here is safe.
	~LifecycleProbe() override
	{
		for (unsigned resize = 0; resize < resizeNotified.size(); ++resize)
			if (!resizeNotified[resize]) {
				SDL_LogError(
					SDL_LOG_CATEGORY_ERROR, "Lifecycle resize request %u received no viewport callback", resize + 1
				);
				rendered = false;
			}
		for (unsigned phase = 0; phase < depthChecks.size(); ++phase)
			if (depthChecks[phase] < 2) {
				SDL_LogError(
					SDL_LOG_CATEGORY_ERROR, "Lifecycle depth phase %u verified only %u downloads", phase, depthChecks[phase]
				);
				rendered = false;
			}
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
	}


	////
	// Interface: fcg::Applet

	/// The name shown by the framework's lifecycle log.
	[[nodiscard]] auto name () const -> const std::string& override {
		const static std::string name = "Lifecycle Probe";
		return name;
	}

	/// Create a pipeline and request continuous redraws for the automated probe.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		notifiedViewport = player.viewportSize();
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
		colorTarget.format = player.mainRenderTargetInfo()->colorFormat;
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

	/// Verify each notification and discard tokens invalidated by the resize invalidation.
	void onViewportResize (fcg::Device&, const glm::uvec2 &oldViewport, fcg::Player &player) override
	{
		if (oldViewport != notifiedViewport || player.viewportSize() == oldViewport)
			throw std::runtime_error("Lifecycle viewport resize callback has inconsistent dimensions");
		notifiedViewport = player.viewportSize();
		if (resizeRequests)
			resizeNotified[resizeRequests - 1] = true;
		// Resize replaces the texture with uninitialized storage; render it before asking for its previous contents.
		depthToken.reset();
		readbackCooldown = 2;
	}

	/// This probe needs no user interface.
	void gui (fcg::Device&, fcg::Player&) override {}

	/// Collect downloads, request two resizes and stop after the configured number of updates.
	void update (fcg::Device&, fcg::Player &player, float) override
	{
		// Collect each token before requesting another. The frame loop has already waited for pending downloads.
		if (depthToken)
		{
			const auto &view = player.getDepthReadbackResult(*depthToken);
			if (&view != &player.getDepthReadbackResult(*depthToken))
				throw std::runtime_error("Repeated depth queries did not preserve the borrowed mapping");
			const auto extent = view.extent();
			if (glm::uvec2(extent) != readbackExtent)
				throw std::runtime_error(std::format(
					"Depth readback extent differs at update {}: {}x{}, expected {}x{}",
					updates, extent.x, extent.y, readbackExtent.x, readbackExtent.y
				));
			const std::array pixels {
				glm::uvec2(extent.x / 2, extent.y / 2),
				glm::uvec2(0, 0), glm::uvec2(extent.x - 1, 0),
				glm::uvec2(0, extent.y - 1), glm::uvec2(extent.x - 1, extent.y - 1)
			};
			for (const auto &pixel : pixels)
			{
				const auto depth = view.readTexel<float>(glm::uvec3(pixel, 0)).value();
				if (depth != 1.f)
					throw std::runtime_error(std::format(
						"Depth readback texel ({}, {}) differs at update {} in phase {}: {}, expected 1",
						pixel.x, pixel.y, updates, readbackPhase, depth
					));
			}
			++depthChecks[readbackPhase];
			depthToken.reset();
		}
		++updates;
		if (updates == 10 || updates == 20)
		{
			if (resizeRequests && !resizeNotified[resizeRequests - 1])
				throw std::runtime_error(std::format(
					"Lifecycle resize request {} received no viewport callback", resizeRequests
				));
			if (depthChecks[resizeRequests] < 2)
				throw std::runtime_error(std::format(
					"Lifecycle depth phase {} verified fewer than two downloads before resizing", resizeRequests
				));
			// Resize between readbacks, so the next frame recreates both depth texture and transfer storage.
			int count = 0;
			auto **windows = SDL_GetWindows(&count);
			const bool resized = windows && count > 0 && SDL_SetWindowSize(
				windows[0], updates == 10 ? 480 : 800, updates == 10 ? 320 : 600
			);
			SDL_free(windows);
			if (!resized)
				throw std::runtime_error(std::format("Could not request lifecycle resize {}", resizeRequests + 1));
			++resizeRequests;
		}
		else if (rendered && !readbackCooldown && (!resizeRequests || resizeNotified[resizeRequests - 1])) {
			readbackExtent = player.viewportSize();
			readbackPhase = resizeRequests;
			depthToken = player.scheduleDepthReadback();
		}
		if (remainingFrames && --remainingFrames == 0)
			player.requestClose();
	}

	/// Draw the triangle and count rendered frames before downloading newly created depth storage.
	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass *renderPass, SDL_GPUCommandBuffer*, fcg::Player&
	) override
	{
		if (!pipeline)
			return;
		SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
		rendered = true;
		if (readbackCooldown)
			--readbackCooldown;
	}


	////
	// Fields

	/// The device that owns our pipeline; outlives the applet.
	SDL_GPUDevice *gpuDevice = nullptr;

	/// The previous frame's depth-download token, consumed before requesting another.
	std::optional<uint64_t> depthToken;

	/// Drawable viewport dimensions recorded when scheduling the current download.
	glm::uvec2 readbackExtent = {0, 0};

	/// The phase that owns the current download: initial, shrunk, or grown viewport.
	unsigned readbackPhase = 0;

	/// Verified downloads in the initial viewport and after each requested resize.
	std::array<unsigned, 3> depthChecks = {};

	/// Number of applet update iterations.
	unsigned updates = 0;

	/// Number of rendered frames to wait before downloading newly allocated depth storage.
	unsigned readbackCooldown = 2;

	/// Number of successful window resize requests.
	unsigned resizeRequests = 0;

	/// Whether each requested resize produced a viewport callback.
	std::array<bool, 2> resizeNotified = {};

	/// The dimensions published by initialization or the most recent resize callback.
	glm::uvec2 notifiedViewport = {0, 0};

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
[[nodiscard]] auto main () -> int
{
	// Frames to render before automatic shutdown; 0 runs until the window is closed. The CTest run leaves this unset
	// (30 frames) - set it manually for a visual check, e.g.
	//   FCG_PROBE_FRAMES=0 ./build/debug/bin/lifecycle-smoke
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
