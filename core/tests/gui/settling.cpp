
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// SDL3 library
#include <SDL3/SDL.h>

// Dear ImGui library
#include <imgui.h>

// FCG Framework
#include <FCG/run.h>
#include <FCG/player.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Total GUI frames, bounded to catch a loop that never returns to waiting.
unsigned frames = 0;

/// Number of times the GUI has settled and the runtime has returned to waiting.
unsigned waits = 0;

/// Most recent value-column positions in the currently visible table.
std::array<float, 3> positions{};

/// Frames submitted for the currently visible table.
unsigned tableFrames = 0;

/// Scene render callbacks, independent of GUI layout passes.
unsigned renders = 0;

/// Fail behavior checks independently of assertion settings.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Keep operating-system input from masking missing GUI follow-up frames.
auto pollEvent (SDL_Event*) -> bool { return false; }

/// Check quiescence before allowing an event to open another unmeasured table or quit.
auto waitEvent (SDL_Event *event, int timeout) -> bool
{
	require(timeout == -1, "Settled GUI unexpectedly requested periodic redraws");
	require(tableFrames >= 3 && std::abs(positions[0] - positions[1]) < .01f
		&& std::abs(positions[1] - positions[2]) < .01f,
		"Runtime waited for input before the proportional table layout settled");
	++waits;
	tableFrames = 0;
	positions = {};
	*event = {};
	event->type = waits == 1 ? SDL_EVENT_USER : SDL_EVENT_QUIT;
	return true;
}

// Anonymous namespace end
}



//////
//
// Includes
//

// Exercise the production loop with real ImGui and GPU rendering, but deterministic input
#define SDL_PollEvent pollEvent
#define SDL_WaitEventTimeout waitEvent
#include "../../src/run.cpp"
#undef SDL_WaitEventTimeout
#undef SDL_PollEvent



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// A static scene with a wrapped proportional table at startup and another after an input event.
class SettlingProbe final : public fcg::Applet
{
public:

	////
	// Interface: fcg::Applet

	/// Return a descriptive name for runtime diagnostics.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string label = "GUI settling";
		return label;
	}

	/// Isolate the layout from saved application settings.
	void init (fcg::Device&, fcg::Player&) override { ImGui::GetIO().IniFilename = nullptr; }

	/// The static clear-only scene needs no pipelines.
	void recreatePipelines (fcg::Device&, fcg::Player&) override {}

	/// The probe authors its panel size directly in ImGui coordinates.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {}

	/// GUI frames advance without explicit application redraw requests.
	void update (fcg::Device&, fcg::Player&, float) override {}

	/// Reproduce the point information table's iterative wrapped-text measurements.
	void gui (fcg::Device&, fcg::Player&) override
	{
		require(++frames < 40, "GUI output never stopped scheduling follow-up frames");
		// Color animation must not perpetually renew the layout budget
		ImGui::PushStyleColor(ImGuiCol_Text, frames % 2 ? IM_COL32_WHITE : IM_COL32(255, 128, 128, 255));
		ImGui::SetNextWindowSize({380, 560}, ImGuiCond_FirstUseEver);
		ImGui::Begin("Mesh Loader");
		if (ImGui::BeginTable(waits == 0 ? "Startup" : "Opened later", 2, ImGuiTableFlags_SizingStretchProp))
		{
			const char *labels[]{"Points", "Point radius", "Bounds minimum", "Bounds maximum", "Bounds extent",
				"Cell widths", "Occupied cells", "Cell references"};
			const char *values[]{"42", "0.125", "-1.25, -2.5, -3.75", "1.25, 2.5, 3.75", "2.5, 5, 7.5",
				"0.25, 0.25, 0.25", "123", "456"};
			for (unsigned row = 0; row < std::size(labels); ++row)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(labels[row]);
				ImGui::TableNextColumn();
				if (row == 0)
					positions[tableFrames++ % positions.size()] = ImGui::GetCursorPosX();
				ImGui::TextWrapped("%s", values[row]);
			}
			ImGui::EndTable();
		}
		ImGui::End();
		ImGui::PopStyleColor();
	}

	/// Count scene refreshes so GUI settling cannot accidentally force scene rendering.
	void render (fcg::Device&, fcg::RenderState&, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, fcg::Player&) override {
		++renders;
	}
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Verify settling without input and after a single event, followed by indefinite idle waiting.
auto main () -> int
{
	try {
		require(fcg::run({}, std::make_unique<SettlingProbe>()) == 0, "Runtime failed");
		require(waits == 2 && renders == 1, "GUI settling did not return to idle or refreshed the static scene");
	}
	catch (const std::exception &error) {
		std::cerr << "GUI settling at frame " << frames << ": " << error.what() << '\n';
		return 1;
	}
	return 0;
}
