
//////
//
// Includes
//

// C++ STL
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/applet.h>
#include <FCG/player.h>
#include <FCG/run.h>
#include <FCG/Render/quad_renderer.h>
#include <FCG/Render/box_renderer.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Fail the lifecycle test with an owned Render diagnostic.
template <class T> auto take (std::expected<T, fcg::RenderError> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Preserve operation diagnostics.
void check (std::expected<void, fcg::RenderError> result) {
	if (!result)
		throw std::runtime_error(result.error().message);
}

/// Use Player factories before the first frame and keep their pipelines through resizing.
class Probe final : public fcg::Applet
{
public:

	////
	// Object construction/destruction

	/// Report success through caller-owned state.
	explicit Probe(bool &success) : success(success) {}


	////
	// Interface: Applet

	/// Stable name shown by the framework.
	auto name () const -> const std::string& override {
		static const std::string value = "Render lifecycle";
		return value;
	}

	/// Create all renderer variants against the main pass before rendering starts.
	void init (fcg::Device &device, fcg::Player &player) override
	{
		if (&player.device() != &device)
			throw std::runtime_error("Player factory device differs");
		target = player.mainRenderTargetInfo();
		if (!target)
			throw std::runtime_error("Main target missing during initialization");
		quad.emplace(take(fcg::QuadRenderer::create(player)));
		quadCulled.emplace(take(fcg::QuadRenderer::create(player, {.cullMode = SDL_GPU_CULLMODE_BACK})));
		box.emplace(take(fcg::BoxRenderer::create(player)));
		boxTwoSided.emplace(take(fcg::BoxRenderer::create(player, {.cullMode = SDL_GPU_CULLMODE_NONE})));
		attributes = std::make_unique<fcg::PrimitiveAttributes>(device);
		player.pushContinuousRedraw();
	}

	/// Record a resize notification.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {
		if (frames > 1)
			resized = true;
	}

	/// No user interface needed by this probe.
	void gui (fcg::Device&, fcg::Player&) override {}

	/// Replace shared arrays while earlier frames remain in flight and resize once.
	void update (fcg::Device&, fcg::Player &player, float) override
	{
		if (++frames == 5)
		{
			int count = 0;
			auto **windows = SDL_GetWindows(&count);
			const bool changed = windows && count && SDL_SetWindowSize(windows[0], 320, 240);
			SDL_free(windows);
			if (!changed)
				throw std::runtime_error("Lifecycle resize failed");
		}
		if (frames > 5 && player.viewportSize() == glm::uvec2(320, 240))
			resized = true;
		const auto current = player.mainRenderTargetInfo();
		if (!current || current->colorFormat != target->colorFormat
			|| current->depthStencilFormat != target->depthStencilFormat || current->sampleCount != target->sampleCount)
			throw std::runtime_error("Resize changed main target configuration");
		std::array positions{glm::vec4(-.5f,0,.5f,1), glm::vec4(.5f,0,.5f,1)};
		std::array colors{glm::vec4(1,0,0,1), glm::vec4(0,1,0,1)};
		check(attributes->setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
			update.set<fcg::Attribute::Position>(std::span(positions));
			update.set<fcg::Attribute::Extent>(glm::vec3(.2f));
			update.set<fcg::Attribute::Color>(std::span(colors));
		}));
		if (frames >= 15) {
			success = resized && draws >= 12;
			player.requestClose();
		}
	}

	/// Draw all four factory configurations into the actual main pass, including its depth attachment.
	void render (
		fcg::Device&, fcg::RenderState &state, SDL_GPURenderPass *pass,
		SDL_GPUCommandBuffer *commands, fcg::Player&
	) override {
		check(quad->draw(*attributes, state, commands, pass, {}, {.count = 1}));
		check(quadCulled->draw(*attributes, state, commands, pass, {}, {.first = 1}));
		check(box->draw(*attributes, state, commands, pass));
		check(boxTwoSided->draw(*attributes, state, commands, pass));
		++draws;
	}


private:

	////
	// Fields

	/// Caller-owned success flag.
	bool &success;

	/// Target snapshot before rendering begins.
	std::optional<fcg::RenderTargetInfo> target;

	/// Default two-sided quad pipeline.
	std::optional<fcg::QuadRenderer> quad;

	/// Explicitly culled quad pipeline.
	std::optional<fcg::QuadRenderer> quadCulled;

	/// Default culled box pipeline.
	std::optional<fcg::BoxRenderer> box;

	/// Explicitly two-sided box pipeline.
	std::optional<fcg::BoxRenderer> boxTwoSided;

	/// Shared instance sources.
	std::unique_ptr<fcg::PrimitiveAttributes> attributes;

	/// Number of updates.
	unsigned frames = 0;

	/// Number of actual main-pass draws.
	unsigned draws = 0;

	/// Whether the resize notification arrived.
	bool resized = false;
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run the actual framework loop and require rendering before and after resize.
auto main () -> int {
	bool success = false;
	std::vector<std::unique_ptr<fcg::Applet>> applets;
	applets.push_back(std::make_unique<Probe>(success));
	const auto result = fcg::run(std::move(applets), {.mainWindowTitle = "Render lifecycle"});
	return result ? result : success ? 0 : 1;
}
