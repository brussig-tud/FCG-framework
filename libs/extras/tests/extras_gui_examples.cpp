
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Dear ImGui library
#include <imgui.h>

// FCG Framework
#include <FCG/Extras/gui.h>
#include <FCG/Image/image.h>
#include <FCG/player.h>



//////
//
// Functions
//

/// \brief Embed image information between application widgets during an applet's GUI callback.
///
/// \param image The image being inspected.
// [embedded_image_metadata]
void imageMetadataEmbeddedExample (const fcg::Image &image)
{
	if (ImGui::Begin("Inspector")) {
		ImGui::TextUnformatted("Current image");
		fcg::extra::displayImageMetadata(image);
		ImGui::Separator();
		ImGui::TextUnformatted("Application controls go here.");
	}
	ImGui::End();
}
// [embedded_image_metadata]

/// \brief Show a separately named inspector during an applet's GUI callback.
///
/// \param image The image being inspected.
/// \param player The applet's player, used to convert authored dimensions.
/// \param open A close state kept in the applet; set it to true to reopen the inspector.
// [image_metadata_window]
void imageMetadataWindowExample (const fcg::Image &image, const fcg::Player &player, bool &open)
{
	fcg::extra::showImageMetadataWindow(image, {
		.title = "Image information###main-image-info",
		.open = &open,
		.initialPosition = player.toGuiUnits(ImVec2(20.f, 20.f)),
		.initialSize = player.toGuiUnits(ImVec2(496.f, 360.f))
	});
}
// [image_metadata_window]
