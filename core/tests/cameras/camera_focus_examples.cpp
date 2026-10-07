
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// FCG Framework
#include <FCG/camera_focus.h>
#include <FCG/event.h>
#include <FCG/viewing.h>



//////
//
// Classes
//

/// [focus navigation]
/// Composes focus navigation with any camera, independently of applet inheritance.
class FocusNavigation
{
public:

	////
	// Object construction/destruction

	/// Borrow the camera; enable planar mode for fixed-depth XY cameras.
	explicit FocusNavigation(fcg::Camera &camera, bool planar = false)
		: camera(camera), focus(camera, planar)
	{}


	////
	// Methods

	/// Call after rendering with the actual frame matrices and drawable viewport.
	void rendered (const glm::mat4 &view, const glm::mat4 &projection, const glm::uvec2 &viewport) {
		focus.rendered(view, projection, viewport);
	}

	/// Pick on a left-button double click; the owning event handler decides how to filter modifiers.
	[[nodiscard]] auto onEvent (const fcg::Event &event, fcg::Player &player) -> bool {
		const auto *click = event.data<fcg::MouseButtonEvent>();
		return event.type() == fcg::EventType::MouseButtonDown && click
			&& click->button == fcg::MouseButton::Left && click->clicks == 2 && focus.pick(event, player);
	}

	/// Call every frame, including after cancellation, to collect pending readbacks.
	void update (fcg::Player &player, float dt) {
		focus.update(player, dt);
	}

	/// Cancel this controller before applying a manual camera change.
	void setFocalPoint (const glm::vec3 &point) {
		focus.cancel();
		camera.setFocalPoint(point);
	}

	/// Call after the player invalidates readback storage, as in the applet resize callback.
	void onViewportResize () {
		focus.resized();
	}


private:

	////
	// Fields

	/// The camera borrowed for manual mutations; it must outlive this object.
	fcg::Camera &camera;

	/// The controller must be destroyed before a player with outstanding focus work.
	fcg::CameraFocus focus;
};
/// [focus navigation]
