
#ifndef __FCG_EXTRAS_GUI_H__
#define __FCG_EXTRAS_GUI_H__


//////
//
// Module documentation
//

/// \defgroup fcg_extra_gui GUI utilities
/// \ingroup fcg_modules
/// \brief Ready-made Dear ImGui interfaces for inspecting and working with framework facilities.
///
/// Use these interfaces from <tt>\ref fcg::Applet::gui "Applet::gui"</tt>, alongside your application's own
/// widgets. Content functions insert widgets at the current ImGui cursor; window functions present them in a
/// separate ImGui window. <tt>\ref fcg::extra::GuiWindowOptions "GuiWindowOptions"</tt> customizes these windows.
/// See \ref fcg_gui for the framework's ImGui integration and display scaling.
///
/// \section fcg_extra_gui_usage Getting started
/// Include <tt>\#include &lt;FCG/Extras/gui.h&gt;</tt> and link \verbatim FCG-Framework::Extras \endverbatim.
/// Link the libraries of the facilities you use as well; the image inspector also uses
/// \verbatim FCG-Framework::Image \endverbatim.
///
/// For example, call this helper from your applet's GUI callback to place image information between other widgets
/// in an application window:
/// \snippet extras_gui_examples.cpp embedded_image_metadata
///
/// \section fcg_extra_gui_workflows Common tasks
/// \subsection fcg_extra_gui_workflows_windows Using separate windows
/// Call a window function each frame while its interface should be available. Provide a title with a unique ImGui
/// ID when showing multiple windows; the usual <tt>##</tt> and <tt>###</tt> naming conventions apply. Window identity
/// follows this name, including when the inspected object changes.
///
/// Optional position and size use ImGui coordinates and <tt>ImGuiCond_FirstUseEver</tt>: saved window settings and
/// subsequent user adjustments take precedence. Omitted geometry leaves placement and sizing to ImGui. Convert
/// dimensions authored at 100% display scale with <tt>\ref fcg::Player::toGuiUnits "Player::toGuiUnits"</tt>:
/// \snippet extras_gui_examples.cpp image_metadata_window
/// Keep the close state in your applet. Passing its address provides a close button; the helper skips a window whose
/// state is false. Set it to true to reopen the window. Omitting the pointer keeps the window available on every call.
///
/// \subsection fcg_extra_gui_workflows_images Inspecting image metadata
/// <tt>\ref fcg::extra::displayImageMetadata "displayImageMetadata"</tt> and
/// <tt>\ref fcg::extra::showImageMetadataWindow "showImageMetadataWindow"</tt> display dimensions, pixel storage,
/// color encoding, and palette and transparency information from an <tt>\ref fcg::Image "Image"</tt>.
/// Expand the detail sections to inspect individual properties. Provenance annotations appear beside color and
/// brightness values, such as <tt>(assumed)</tt> or <tt>(derived; assumed inputs)</tt>. Declared values appear without
/// an origin label. Sample adaptations and known assumption dependencies remain annotated. Hover an annotated value
/// for an explanation. See \ref fcg_pixel_formats for the metadata contracts.
///
/// Each display reads the image's current metadata. If metadata is unavailable, the inspector displays the error
/// message. To embed the same image more than once in one window, enclose each call in a distinct
/// <tt>ImGui::PushID</tt>/<tt>ImGui::PopID</tt> scope.
///
/// \section fcg_extra_gui_lifetime Ownership and lifetime
/// Call on the main thread during an active ImGui frame. Inspected objects and optional close-state pointers are
/// borrowed only for the duration of the call; keep them alive and avoid concurrent modification during that call.



//////
//
// Includes
//

// C++ STL
#include <optional>
#include <string>

// Dear ImGui library
#include <imgui.h>

// Local includes
#include "FCG/Extras/export.h"



//////
//
// Forward declarations
//

// Framework types
namespace fcg {
	class Image;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// The library's namespace for assorted extra utilities.
namespace extra {

/// \addtogroup fcg_extra_gui
/// @{



//////
//
// Structs and enums
//

/// Settings for a separate ImGui window containing a framework interface.
struct GuiWindowOptions
{
	/// Optional UTF-8 window title and ImGui ID; omission uses the window function's default name. A supplied name
	/// must be nonempty and contain no NUL characters. ImGui's \c ## and \c ### naming conventions are supported.
	std::optional<std::string> title = std::nullopt;

	/// Optional borrowed close state, used only during the call. A false value skips the window; clicking its close
	/// button sets the value to false. The default \c nullptr omits the close button.
	bool *open = nullptr;

	/// Flags passed to \c ImGui::Begin; the default enables the standard window controls.
	ImGuiWindowFlags flags = ImGuiWindowFlags_None;

	/// Optional top-left position in ImGui screen coordinates, applied with \c ImGuiCond_FirstUseEver.
	/// Omission leaves placement to ImGui; saved settings and user adjustments take precedence.
	std::optional<ImVec2> initialPosition = std::nullopt;

	/// Optional size in ImGui units, applied with \c ImGuiCond_FirstUseEver. Zero on an axis requests auto-fit;
	/// supplied components must be nonnegative. Omission leaves sizing to ImGui; saved settings and user adjustments
	/// take precedence.
	std::optional<ImVec2> initialSize = std::nullopt;
};



//////
//
// Functions
//

/// \brief Display CPU image metadata at the current ImGui cursor, for example inside an application's inspector.
///
/// Shows dimensions, storage, color encoding, loading annotations, palette and transparency information. Detail
/// sections can be expanded individually. An unavailable metadata query is displayed as an error message.
/// The image's pixels and metadata remain unchanged.
///
/// \pre
/// 	Call on the main thread during an active ImGui frame with the destination window current. Access to \p image
/// 	is externally synchronized. When displaying the same image twice in one window, use distinct caller-provided
/// 	ImGui ID scopes.
///
/// \param image The borrowed image to inspect. It must remain alive throughout the call; an empty image is allowed.
FCG_EXTRAS_EXPORT void displayImageMetadata (const Image &image);

/// \brief Display CPU image metadata in a separate ImGui window. Call each frame from \c Applet::gui.
///
/// The default name is <tt>"Image information"</tt>. A supplied title determines the window's ImGui identity;
/// use distinct names or ID suffixes for independent windows. Optional geometry is applied with
/// \c ImGuiCond_FirstUseEver. The window contains the same information as \c displayImageMetadata.
///
/// \pre
/// 	Call on the main thread during an active ImGui frame. Access to \p image is externally synchronized.
/// 	Any supplied title is nonempty and contains no NUL characters; supplied geometry is finite and size components
/// 	are nonnegative. A supplied close-state pointer remains valid throughout the call.
///
/// \param image The borrowed image to inspect. It must remain alive throughout the call; an empty image is allowed.
/// \param options Window settings, borrowed only during the call. A false close state skips the window.
FCG_EXTRAS_EXPORT void showImageMetadataWindow (const Image &image, const GuiWindowOptions &options = {});



//////
//
// Namespaces close
//

/// @}

// namespace extra
}

// namespace fcg
}


#endif // ifndef __FCG_EXTRAS_GUI_H__
