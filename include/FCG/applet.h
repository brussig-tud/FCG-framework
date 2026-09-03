
#ifndef __FCG_APPLET_H__
#define __FCG_APPLET_H__


//////
//
// Includes
//

// C++ STL
#include <string>
#include <memory>
#include <concepts>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPURenderPass;

// Framework types
namespace fcg {
	class Player;
	class Device;
	class Applet;
}




//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Interfaces
//

/// The polymorphic interface of an \c AppletFactory.
class FCG_FRAMEWORK_EXPORT AppletFactory
{
public:

	////
	// Construction/Destruction

	/// Virtual base destructor. Forces vtable creation.
	virtual ~AppletFactory () = default;

	/// Create an instance of the applet.
	virtual auto create () -> std::unique_ptr<Applet> = 0;
};

/// The concept of behaving like an \ref AppletFactory.
template <class A>
concept AppletFactoryConcept = std::derived_from<A, AppletFactory>;


/// The polymorphic interface of an \c Applet.
class FCG_FRAMEWORK_EXPORT Applet
{
public:

	////
	// Construction/Destruction

	/// Virtual base destructor. Forces vtable creation.
	virtual ~Applet () = default;


	////
	// Methods

	/// The human-readable name of the applet.
	virtual auto name () -> std::string& = 0;

	/// Run any post-creation initialization code. The GPU device that the applet will later use for rendering is
	/// provided, e.g. for creating pipelines, buffers and textures.
	///
	/// \param device The active SDL GPU device that will be used for rendering.
	/// \param player Reference to the central applet player.
	virtual void init (Device &device, Player &player) = 0;

	/// Called whenever the main viewport of the player (i.e., the one that the render passes provided to \ref render
	/// are targeting) changes dimensions. To get the new dimensions, applets should query
	/// \ref fcg::Player::viewportSize
	///
	/// \param device The active SDL GPU device that is used for rendering.
	/// \param oldViewportSize The dimensions of the main viewport before the most recent resizing
	/// \param player Reference to the central applet player.
	virtual void onViewportResize (Device &device, const glm::uvec2 &oldViewportSize, Player &player) = 0;

	/// Define all *ImGui* widgets the applet wants. The framework *ImGui* context is current during this call, so
	/// applets can issue `ImGui::` calls directly. Rendering of the resulting draw data is handled by the
	/// framework – applets must not call `ImGui::Render` themselves.
	///
	/// \param device The active SDL GPU device that is used for rendering.
	/// \param player Reference to the central applet player.
	virtual void gui (Device &device, Player &player) = 0;

	/// Run all code for updating applet state for the next frame.
	/// TODO: additional arguments required for update, for example frame stats (delta-t and so on)
	///
	/// \param device The active SDL GPU device that is used for rendering.
	/// \param player Reference to the central applet player.
	virtual void update (Device &device, Player &player) = 0;

	/// Perform all rendering the applet might want to do.
	///
	/// \param device The active SDL GPU device that is used for rendering.
	/// \param renderPass A render pass targeting the current swapchain texture of the main window. Record all draw
	///                   calls that should appear on the window into this pass. The pass is begun before and ended
	///                   after all applets had their turn by the framework – do not end (or re-begin) it yourself.
	/// \param player Reference to the central applet player.
	virtual void render (Device &device, SDL_GPURenderPass *renderPass, Player &player) = 0;
};

/// The concept of behaving like an \ref Applet.
template <class A>
concept AppletConcept = std::derived_from<A, Applet>;



//////
//
// Namespaces close
//

// namespace FCG
}


#endif  // ifndef __FCG_APPLET_H__
