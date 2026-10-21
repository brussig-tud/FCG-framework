
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/player.h>
#include <FCG/res.h>
#include <FCG/window.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Default scene storage.
constexpr auto srgb = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;

/// Linear eight-bit storage.
constexpr auto unorm = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

/// Floating-point storage.
constexpr auto floating = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;

/// Fail with a useful test diagnostic.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Require acceptance of a configuration request.
void accept (std::expected<void, fcg::TextureError> result) {
	if (!result)
		throw std::runtime_error(result.error().message);
}

/// State shared by two applets and retained through runtime cleanup.
struct Observations
{
	/// Configured startup format.
	SDL_GPUTextureFormat startup = srgb;

	/// Whether initialization requests should override startup configuration.
	bool negotiate = false;

	/// Completed initialization callbacks.
	unsigned initialized = 0;

	/// Expected next registration index for each recreation pass.
	unsigned nextRecreation = 0;

	/// Completed recreation passes.
	unsigned generations = 0;

	/// Main-loop updates completed by the first applet.
	unsigned updates = 0;

	/// Formats observed in each recreation pass.
	std::vector<SDL_GPUTextureFormat> formats;

	/// Readback retained through a format-only change and invalidated on resize.
	std::optional<uint64_t> depth;

	/// Number of actual drawing callbacks.
	unsigned draws = 0;

	/// Number of observed size changes.
	unsigned resizes = 0;

	/// Whether the test completed all phases.
	bool complete = false;
};

/// Two independently initialized applets that verify registration order and draw with replaced pipelines.
class Probe final : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Retain caller-owned observations and registration order.
	Probe(Observations &observed, unsigned index) : observed(observed), index(index) {}

	/// Release the current pipeline while the GPU still exists.
	~Probe() override {
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(gpu, pipeline);
	}


	////
	// Interface: fcg::Applet

	/// Stable display name.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string label = "Scene format lifecycle";
		return label;
	}

	/// Negotiate without changing the effective target during initialization.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		gpu = device.handle();
		require(observed.initialized++ == index && observed.generations == 0, "Initialization phase was interleaved");
		require(player.mainRenderTargetInfo()->colorFormat == observed.startup, "Startup format was not forwarded");
		if (observed.negotiate)
		{
			accept(player.setSceneFormat(index ? floating : unorm));
			for (const auto format : {SDL_GPU_TEXTUREFORMAT_INVALID, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
				SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT, SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM,
				static_cast<SDL_GPUTextureFormat>(-1), static_cast<SDL_GPUTextureFormat>(10000)})
				require(!player.setSceneFormat(format), "Unsuitable scene format was accepted");
			require(player.mainRenderTargetInfo()->colorFormat == observed.startup, "Init request applied immediately");
		}
	}

	/// Replace pipelines and observe pending requests only at subsequent boundaries.
	void recreatePipelines (fcg::Device &device, fcg::Player &player) override
	{
		require(observed.initialized == 2, "Pipeline creation preceded completion of initialization");
		require(observed.nextRecreation++ == index, "Recreation order changed");
		const auto target = player.mainRenderTargetInfo().value();
		require(target.depthStencilFormat == SDL_GPU_TEXTUREFORMAT_D32_FLOAT
			&& target.sampleCount == SDL_GPU_SAMPLECOUNT_1, "Scene change affected depth or samples");
		if (!index)
		{
			observed.formats.push_back(target.colorFormat);
			if (observed.generations == 0)
			{
				require(target.colorFormat == (observed.negotiate ? floating : observed.startup),
					"Initialization requests did not coalesce");
				// A request from this pass must not affect the next applet's target in the same pass.
				accept(player.setSceneFormat(target.colorFormat == unorm ? srgb : unorm));
			}
			if (observed.updates == 2)
			{
				const auto &mapping = player.getDepthReadbackResult(observed.depth.value());
				const auto depth = mapping.readTexel<float>({0, 0, 0});
				require(depth.has_value() && std::abs(*depth - 1.f) < .00001f,
					"Format-only change invalidated depth contents or readback");
			}
			if (observed.updates == 4)
			{
				bool invalidated = false;
				try { (void)player.getDepthReadbackResult(observed.depth.value()); }
				catch (const std::logic_error&) { invalidated = true; }
				require(invalidated, "Simultaneous resize did not invalidate readback before recreation");
			}
		}
		else
			require(target.colorFormat == observed.formats.back(), "Request from recreation applied within its pass");
		if (pipeline)
			SDL_ReleaseGPUGraphicsPipeline(gpu, pipeline);
		pipeline = nullptr;
		const auto shader = fcg::res::shader("triangle").value();
		const auto release = [&device] (SDL_GPUShader *stage) { SDL_ReleaseGPUShader(device.handle(), stage); };
		const std::unique_ptr<SDL_GPUShader, decltype(release)> vertex(
			device.createShader(fcg::ShaderStage::VERTEX, shader.stage(fcg::ShaderStage::VERTEX)->spirv, 0), release
		);
		const std::unique_ptr<SDL_GPUShader, decltype(release)> fragment(
			device.createShader(fcg::ShaderStage::FRAGMENT, shader.stage(fcg::ShaderStage::FRAGMENT)->spirv, 0), release
		);
		require(vertex && fragment, "Could not create scene shaders");
		const SDL_GPUColorTargetDescription color{.format = target.colorFormat};
		SDL_GPUGraphicsPipelineCreateInfo info{};
		info.vertex_shader = vertex.get();
		info.fragment_shader = fragment.get();
		info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		info.multisample_state.sample_count = target.sampleCount;
		info.target_info = {&color, 1, target.depthStencilFormat, true};
		pipeline = SDL_CreateGPUGraphicsPipeline(gpu, &info);
		require(pipeline, "Could not replace scene pipeline");
		if (index == 1) {
			observed.nextRecreation = 0;
			++observed.generations;
		}
	}

	/// Verify that the full recreation pass precedes resize notifications.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player &player) override {
		checkTarget(player);
		if (!index)
			++observed.resizes;
	}

	/// Verify that input sees compatible pipelines.
	void onEvent (const fcg::Event&, fcg::EventContext&, fcg::Player &player) override {
		checkTarget(player);
	}

	/// Verify that GUI callbacks see compatible pipelines.
	void gui (fcg::Device&, fcg::Player &player) override {
		checkTarget(player);
	}

	/// Exercise deferral, cancellation, size-only changes and idle redraw without continuous redraw requests.
	void update (fcg::Device&, fcg::Player &player, float) override
	{
		checkTarget(player);
		if (index)
			return;
		const auto effective = player.mainRenderTargetInfo()->colorFormat;
		const auto other = effective == srgb ? unorm : srgb;
		switch (++observed.updates)
		{
			case 1:
				require(observed.generations == 2, "Request from initial recreation was not deferred to first boundary");
				break;
			case 2:
				observed.depth = player.scheduleDepthReadback();
				accept(player.setSceneFormat(other));
				require(!player.setSceneFormat(SDL_GPU_TEXTUREFORMAT_D32_FLOAT), "Invalid request accepted after valid request");
				require(player.mainRenderTargetInfo()->colorFormat == effective, "Update request applied during frame");
				break;
			case 3:
				require(observed.generations == 3, "Valid pending request was lost after rejection");
				accept(player.setSceneFormat(floating));
				accept(player.setSceneFormat(effective));
				break;
			case 4:
				require(observed.generations == 3, "Requesting effective format did not cancel pending change");
				accept(player.setSceneFormat(floating));
				resize(player, 640, 360);
				break;
			case 5:
				require(observed.generations == 4 && effective == floating, "Simultaneous resize lost format change");
				resize(player, 480, 300);
				break;
			case 6:
				require(observed.generations == 4 && observed.resizes >= 2, "Size-only change recreated pipelines");
				break;
			default:
				// Keeping format requests flowing must wake redraw even after ordinary pending redraws run out.
				if (observed.updates >= 12) {
					observed.complete = observed.generations >= 9 && observed.draws >= 20;
					player.requestClose();
				}
				break;
		}
	}

	/// Draw compatible pipelines and queue changes from render callbacks, including after idle redraws run out.
	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass *pass, SDL_GPUCommandBuffer*, fcg::Player &player
	) override
	{
		checkTarget(player);
		SDL_BindGPUGraphicsPipeline(pass, pipeline);
		SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
		++observed.draws;
		if (!index && observed.updates >= 6)
			accept(player.setSceneFormat(observed.formats.back() == unorm ? srgb : unorm));
	}


private:

	////
	// Methods

	/// Check both applets have completed recreation for the currently effective target.
	void checkTarget (fcg::Player &player) const {
		require(observed.generations && observed.nextRecreation == 0
			&& player.mainRenderTargetInfo()->colorFormat == observed.formats.back(),
			"Applet callback preceded pipeline recreation");
	}

	/// Synchronize a real drawable resize before the next acquisition.
	void resize (fcg::Player &player, int width, int height) {
		require(SDL_SetWindowSize(player.mainWindow()->handle(), width, height), "Scene probe resize failed");
		require(SDL_SyncWindow(player.mainWindow()->handle()), "Scene probe resize timed out");
	}


	////
	// Fields

	/// Shared observations owned by main.
	Observations &observed;

	/// Registration index.
	unsigned index;

	/// GPU that outlives this applet.
	SDL_GPUDevice *gpu = nullptr;

	/// Current compatible scene pipeline.
	SDL_GPUGraphicsPipeline *pipeline = nullptr;
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run configuration and lifecycle scenarios using a real GPU and display.
auto main (int argc, char **argv) -> int
{
	try
	{
		require(argc == 2, "Expected a scene format scenario");
		Observations observed;
		const std::string scenario = argv[1];
		observed.negotiate = scenario == "negotiate";
		observed.startup = scenario == "unorm" ? unorm : scenario == "float" ? floating : srgb;
		std::vector<std::unique_ptr<fcg::Applet>> applets;
		for (unsigned index = 0; index < 2; ++index)
			applets.push_back(std::make_unique<Probe>(observed, index));
		const auto status = fcg::run(std::move(applets), {.sceneRenderTargetFormat = observed.startup});
		require(status == EXIT_SUCCESS && observed.complete, "Scene format probe did not finish all phases");
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
