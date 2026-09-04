
#ifndef __FCG_ORBIT_CAMERA_H__
#define __FCG_ORBIT_CAMERA_H__


//////
//
// Includes
//

// C++ STL
#include <memory>

// FCG Framework
#include <FCG/applet.h>




//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Our module namespace
namespace applet {



//////
//
// Classes
//

/// An applet implementing an orbit camera.
class OrbitCamera : public Applet
{
public:

	////
	// Types

	/// The applet factory
	struct Factory : public AppletFactory
	{
		//////
		// Object construction/destruction

		/// The destructor.
		virtual ~Factory () = default;


		////
		// Interface: fcg::AppletFactory

		auto create () -> std::unique_ptr<fcg::Applet> override {
			return std::make_unique<OrbitCamera>();
		}
	};


	////
	// Object construction/destruction

	/// Default constructor.
	OrbitCamera() = default;

	/// The destructor.
	~OrbitCamera() override = default;


	////
	// Interface: fcg::Applet

	auto name () -> std::string& override {
		static std::string name = "Orbit Camera";
		return name;
	}

	void onViewportResize (Device &device, const glm::uvec2 &oldViewportSize, Player &player) override {
		// Nothing to do yet.
	}

	void init (Device &device, Player &player) override {
		// Nothing to do yet.
	}

	void gui (Device &device, Player &player) override {
		// Nothing to do yet.
	}

	void update (Device &device, Player &player) override {
		// Nothing to do yet.
	}

	void render (
		Device &device, RenderState &renderState, SDL_GPURenderPass *renderPass, Player &player
	) override {
		// Nothing to do yet.
	}


protected:

	////
	// Fields

	/* nothing here yet */
};



//////
//
// Namespaces close
//

// namespace applet
}

// namespace fcg
}


#endif  // ifndef __FCG_ORBIT_CAMERA_H__
