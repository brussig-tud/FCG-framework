
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
// [includes]
#include <FCG/applet.h>
// [includes]



//////
//
// Classes
//

// [event_handling]
class InputApplet : public fcg::Applet
{
public:
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string instanceName = "Input";
		return instanceName;
	}

	void onEvent (const fcg::Event &event, fcg::EventContext &context, fcg::Player &player) override {
		switch (event.type())
		{
			case fcg::EventType::KeyDown:
				if (const auto *keys = event.data<fcg::KeyEvent>(); keys && keys->key == SDLK_SPACE) {
					// React to the key press, and stop later applets from seeing it.
					context.markHandled();
				}
				break;

			default:
				break;
		}
	}

	/// No scene pipelines are needed by this applet.
	void recreatePipelines (fcg::Device&, fcg::Player&) override {}

	void init (fcg::Device &device, fcg::Player &player) override {}
	void onViewportResize (fcg::Device &device, const glm::uvec2 &oldViewportSize, fcg::Player &player) override {}
	void gui (fcg::Device &device, fcg::Player &player) override {}
	void update (fcg::Device &device, fcg::Player &player, float dt) override {}
	void render (
		fcg::Device &device, fcg::RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, fcg::Player &player
	) override {}
};
// [event_handling]



//////
//
// Functions
//

/// Read a typed key payload and report whether it describes a Space press.
///
/// \param event The borrowed input event.
///
/// \return Whether this is a Space key press.
// [key_payload]
[[nodiscard]] auto isSpacePress (const fcg::Event &event) -> bool {
	if (const auto *key = event.data<fcg::KeyEvent>(); key && event.type() == fcg::EventType::KeyDown)
		return key->key == SDLK_SPACE;
	return false;
}
// [key_payload]
