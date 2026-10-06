/**
 * \defgroup fcg_applets Applets
 * \ingroup fcg_components
 *
 * \ref fcg::Applet defines the application callbacks used by the player. Its declarations describe event handling, updates, rendering, and GUI integration.
 *
 * \par Guide incomplete
 * This guide is a stub. Consult the API declarations below for currently documented behavior.
 *
 * \section fcg_applets_workflows Common workflows
 * Guide incomplete: workflow descriptions remain to be investigated and written.
 *
 * \section fcg_applets_lifetime Ownership and lifetime
 * Guide incomplete: consult individual type and member contracts.
 *
 * \section fcg_applets_errors Errors
 * Guide incomplete: error handling remains to be investigated and written.
 *
 * \section fcg_applets_examples Examples
 * Guide incomplete: worked examples remain to be added and compiled.
 *
 * \see \ref fcg_runtime, \ref fcg_events, \ref fcg_render_state
 */


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
#include "FCG/event.h"
#include "FCG/render_state.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPURenderPass;

// Framework types
namespace fcg {
	class Device;
	class RenderState;
	class Player;
	class Applet;
}




//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/** \addtogroup fcg_applets
 * @{
 */



//////
//
// Interfaces
//

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
	[[nodiscard]] virtual auto name () const -> const std::string& = 0;

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

	/// Handle one synchronous main-window input event.
	virtual void onEvent (const Event &event, EventContext &context, Player &player) {}

	/// Define all *ImGui* widgets the applet wants. The framework *ImGui* context is current during this call, so
	/// applets can issue `ImGui::` calls directly. Rendering of the resulting draw data is handled by the
	/// framework – applets must not call `ImGui::Render` themselves.
	///
	/// \param device The active SDL GPU device that is used for rendering.
	/// \param player Reference to the central applet player.
	virtual void gui (Device &device, Player &player) = 0;

	/// Run all code for updating applet state for the next frame.
	///
	/// \param device The active SDL GPU device that is used for rendering.
	/// \param player Reference to the central applet player.
	/// \param dt
	/// 	The time elapsed since the last frame, in seconds. In case the app just woke up from blocking on an empty
	/// 	event queue, this is guaranteed to be very small (typically a few microseconds or less).
	virtual void update (Device &device, Player &player, float dt) = 0;

	/// Perform all rendering the applet might want to do.
	///
	/// \param device The active SDL GPU device that is used for rendering.
	/// \param renderState The current render state.
	/// \param renderPass A render pass targeting the current swapchain texture of the main window. Record all draw
	///                   calls that should appear on the window into this pass. The pass is begun before and ended
	///                   after all applets had their turn by the framework – do not end (or re-begin) it yourself.
	/// \param commandBuffer The command buffer that owns the render pass. Needed for operations such as pushing
	///                      uniform data that cannot be recorded through the render pass handle.
	/// \param player Reference to the central applet player.
	virtual void render (
		Device &device, RenderState &renderState, SDL_GPURenderPass *renderPass,
		SDL_GPUCommandBuffer *commandBuffer, Player &player
	) = 0;
};

/// The concept of behaving like an \ref fcg::Applet.
template <class A>
concept AppletConcept =
	   std::derived_from<A, Applet>/*
	&& requires (A applet, SDL_GPUDevice *gpuDevice, SDL_GPURenderPass *renderPass, Player &player)
{
	/// Construct an instance of the Applet using defaults for all initial state, ready for consumption by
	/// \ref fcg::run.
	{ A::create() } -> std::same_as<std::unique_ptr<A>>;
}*/;



/** @} */

//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_APPLET_H__
