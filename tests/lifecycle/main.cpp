//////
//
// Smoke test for the framework lifecycle: runs a trivial applet through
// fcg::run(), forcing continuous redraws and requesting shutdown after a fixed
// number of frames. Driven by the 'lifecycle-smoke' CTest (see tests/).
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
	}

	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}

	void gui (fcg::Device &device, fcg::Player &player) override {}

	void update (fcg::Device &device, fcg::Player &player) override {
		if (--m_remainingFrames == 0)
			player.requestClose();
	}

	void render (
		fcg::Device &device, fcg::RenderState &renderState, SDL_GPURenderPass *renderPass, fcg::Player &player
	) override {}


	////
	// State

	std::string m_name = "Lifecycle Probe";
	unsigned m_remainingFrames;
};

} // namespace



////
// Entry point

int main () {
	std::vector<std::unique_ptr<fcg::Applet>> applets;
	applets.push_back(std::make_unique<LifecycleProbe>(30));
	return fcg::run(std::move(applets));
}
