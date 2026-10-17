
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

// Framework implementation
#include "gui_scaling.h"
#include "window_scaling.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Report the location of a failed behavior check.
void check (bool condition, const std::source_location &where = std::source_location::current()) {
	if (!condition)
		throw std::runtime_error("GUI scaling check failed at line " + std::to_string(where.line()));
}

/// Compare observable floating-point quantities with a small absolute tolerance.
[[nodiscard]] auto near (float a, float b) -> bool { return std::abs(a - b) < .0001f; }

/// An ImGui context with a CPU-only renderer acknowledgement and injectable display information.
class Context
{
public:

	////
	// Object construction/destruction

	/// Create real fonts and enable dynamic atlas baking without SDL or a GPU.
	Context()
	{
		auto &io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.DisplaySize = ImVec2(2000, 1600);
		io.DeltaTime = 1.f / 60.f;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
		io.FontDefault = bitmap;
	}

	/// Destroy the isolated test context and its fonts.
	~Context() { ImGui::DestroyContext(context); }


	////
	// Methods

	/// Deliver a window event with arbitrary payloads; queries supply the authoritative state.
	void event (Uint32 type = SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED, SDL_WindowID window = 7)
	{
		SDL_Event event{};
		event.type = type;
		event.window.windowID = window;
		event.window.data1 = 999;
		scaling.processEvent(event);
	}

	/// Count actual display queries through the production cache.
	void refresh () {
		scaling.refresh([this] { ++queries; return display; });
	}

	/// Simulate an event-driven move or density change before the next GUI frame.
	void move (float scale, float density = 1.f) {
		display = {scale, density};
		event();
		refresh();
	}

	/// Produce a real ImGui panel, optionally collapsed, and process CPU atlas updates.
	void panel (bool collapsed = false, bool overrideGeometry = false)
	{
		scaling.selectFont();
		ImGui::NewFrame();
		if (overrideGeometry) {
			ImGui::SetNextWindowPos(ImVec2(80, 90), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Always);
		}
		else {
			ImGui::SetNextWindowPos(ImVec2(103, 117), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(301, 201), ImGuiCond_FirstUseEver);
		}
		ImGui::SetNextWindowCollapsed(collapsed, ImGuiCond_Always);
		ImGui::Begin("Panel", nullptr, ImGuiWindowFlags_NoSavedSettings);
		ImGui::TextUnformatted("Scaled panel");
		ImGui::Dummy(ImVec2(60, 50));
		ImGui::End();
		ImGui::Render();
		for (ImTextureData *texture : ImGui::GetPlatformIO().Textures)
		{
			if (texture->Status == ImTextureStatus_WantCreate)
				texture->SetTexID((ImTextureID)texture->UniqueID);
			if (texture->Status == ImTextureStatus_WantCreate || texture->Status == ImTextureStatus_WantUpdates)
				texture->SetStatus(ImTextureStatus_OK);
			if (texture->Status == ImTextureStatus_WantDestroy) {
				texture->SetTexID(ImTextureID_Invalid);
				texture->SetStatus(ImTextureStatus_Destroyed);
			}
		}
	}


	////
	// Fields

	/// Context created before its fonts and scaling adapter.
	ImGuiContext *context = ImGui::CreateContext();

	/// Atlas-owned bitmap font.
	ImFont *bitmap = ImGui::GetIO().Fonts->AddFontDefaultBitmap();

	/// Atlas-owned scalable font.
	ImFont *vector = ImGui::GetIO().Fonts->AddFontDefaultVector();

	/// Production scaling policy with a synthetic main-window ID.
	fcg::detail::GuiScaling scaling{7, bitmap, vector};

	/// Values supplied by the next injected query.
	fcg::detail::GuiScaling::DisplayScale display{};

	/// Number of queries made by the scaling policy.
	unsigned queries = 0;
};

/// Check event coalescing, isolation, clean frames and recovery from invalid queries.
void eventsAndFailures ()
{
	Context test;
	test.refresh();
	check(test.queries == 1);
	for (unsigned i = 0; i < 100; ++i)
		test.refresh();
	check(test.queries == 1);
	test.display = {2.f, 1.f};
	test.event(SDL_EVENT_WINDOW_RESIZED, 8);
	test.event(SDL_EVENT_KEY_DOWN);
	test.refresh();
	check(test.queries == 1 && ImGui::GetStyle().FontScaleDpi == 1.f);
	unsigned expectedQueries = 1;
	for (auto type : {SDL_EVENT_WINDOW_DISPLAY_CHANGED, SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED,
		SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, SDL_EVENT_WINDOW_RESIZED, SDL_EVENT_WINDOW_SHOWN,
		SDL_EVENT_WINDOW_RESTORED})
	{
		test.event(type);
		test.refresh();
		check(test.queries == ++expectedQueries && ImGui::GetStyle().FontScaleDpi == 2.f);
	}
	for (unsigned i = 0; i < 10; ++i)
		test.event();
	test.refresh();
	check(test.queries == ++expectedQueries);
	for (auto type : {SDL_EVENT_WINDOW_RESIZED, SDL_EVENT_WINDOW_DISPLAY_CHANGED})
		test.event(type);
	test.refresh();
	check(test.queries == ++expectedQueries);
	for (float invalid : {0.f, -1.f, std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN()})
	{
		test.move(invalid);
		check(test.scaling.retryPending() && ImGui::GetStyle().FontScaleDpi == 2.f);
		test.move(2.f, invalid);
		check(test.scaling.retryPending() && ImGui::GetStyle().FontScaleDpi == 2.f);
	}
	test.display = {1.f, 1.f};
	test.refresh();
	check(!test.scaling.retryPending() && ImGui::GetStyle().FontScaleDpi == 1.f);
	const auto queries = test.queries;
	test.refresh();
	check(test.queries == queries);
}

/// Check exact baseline restoration and direct edits made while scaled.
void stylePreservation ()
{
	Context test;
	auto &style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(7.25f, 9.75f);
	style.FramePadding = ImVec2(5.25f, 3.75f);
	style.TabCloseButtonMinWidthSelected = FLT_MAX;
	style.TabCloseButtonMinWidthUnselected = -1.f;
	test.move(1.5f);
	check(style.WindowPadding.x == 10.f && style.WindowPadding.y == 14.f);
	check(style.TabCloseButtonMinWidthSelected == FLT_MAX);
	style.WindowPadding.x = 19.5f;
	style.TabCloseButtonMinWidthSelected = 21.f;
	style.Colors[ImGuiCol_Text] = ImVec4(.1f, .2f, .3f, 1.f);
	style.Alpha = .8f;
	style.HoverDelayNormal = .9f;
	style.FontScaleMain = 1.25f;
	style.FontSizeBase = 17.f;
	style.WindowTitleAlign = ImVec2(.25f, .75f);
	test.move(2.f);
	check(style.WindowPadding.x == 26.f && style.WindowPadding.y == 19.f);
	check(style.TabCloseButtonMinWidthSelected == 28.f);
	for (unsigned i = 0; i < 50; ++i)
	{
		test.move(1.f);
		check(style.WindowPadding.x == 13.f && style.WindowPadding.y == 9.75f);
		check(style.FramePadding.x == 5.25f && style.FramePadding.y == 3.75f);
		check(style.TabCloseButtonMinWidthSelected == 14.f && style.TabCloseButtonMinWidthUnselected == -1.f);
		test.move(1.5f);
		test.move(2.f);
	}
	check(style.Colors[ImGuiCol_Text].x == .1f && style.Alpha == .8f);
	check(style.HoverDelayNormal == .9f && style.WindowTitleAlign.y == .75f);
	check(style.FontScaleMain == 1.25f && style.FontSizeBase == 17.f);
}

/// Check startup scales and density-only moves separately from coordinate-scale changes.
void coordinateModels ()
{
	for (float scale : {1.f, 1.5f, 2.f})
	{
		Context test;
		ImGui::GetStyle().ScrollbarSize = 17.f;
		test.move(scale);
		check(ImGui::GetStyle().ScrollbarSize == std::trunc(17.f * scale));
		check(ImGui::GetStyle().FontScaleDpi == scale);
	}
	Context test;
	test.refresh();
	test.panel();
	ImGuiWindow *window = ImGui::FindWindowByName("Panel");
	const ImVec2 size = window->SizeFull, position = window->Pos;
	test.move(2.f, 2.f);
	check(ImGui::GetStyle().FontScaleDpi == 1.f);
	check(window->SizeFull.x == size.x && window->Pos.x == position.x);
	ImGui::GetIO().DisplayFramebufferScale = ImVec2(2, 2);
	test.scaling.selectFont();
	check(ImGui::GetIO().FontDefault == test.vector);
	test.panel();
	check(ImGui::GetFont()->CurrentRasterizerDensity == 2.f);
}

/// Keep a panel near the lower-right corner inside a proportionally resized native client area.
void hostWindowScaling ()
{
	Context test;
	glm::ivec2 client(800, 600);
	fcg::detail::WindowScaling host(client);
	auto move = [&] (float scale)
	{
		host.refresh(scale, 1.f, client, false, [&] (glm::ivec2 size) { return client = size; });
		ImGui::GetIO().DisplaySize = ImVec2(float(client.x), float(client.y));
		test.move(scale);
	};
	move(1.f);
	test.panel();
	ImGui::SetWindowPos("Panel", ImVec2(450, 350));
	test.panel();
	auto *panel = ImGui::FindWindowByName("Panel");
	for (float scale : {1.5f, 2.f, 1.f})
	{
		move(scale);
		test.panel();
		check(near(panel->Pos.x, 450.f * scale));
		check(near(panel->Pos.y, 350.f * scale));
		check(panel->Pos.x + panel->Size.x <= client.x);
		check(panel->Pos.y + panel->Size.y <= client.y);
		const ImVec2 position = panel->Pos, size = panel->Size;
		move(scale);
		test.panel();
		check(panel->Pos.x == position.x && panel->Pos.y == position.y);
		check(panel->Size.x == size.x && panel->Size.y == size.y);
	}
}

/// Check window round trips, user edits, collapsed dimensions and application overrides.
void windowGeometry ()
{
	Context test;
	test.refresh();
	test.panel();
	ImGuiWindow *window = ImGui::FindWindowByName("Panel");
	for (unsigned i = 0; i < 50; ++i)
	{
		test.move(1.5f);
		check(window->Pos.x == 154.f && window->SizeFull.x == 451.f);
		test.move(2.f);
		check(window->Pos.x == 206.f && window->SizeFull.x == 602.f);
		test.move(1.f);
		check(window->Pos.x == 103.f && window->Pos.y == 117.f);
		check(window->SizeFull.x == 301.f && window->SizeFull.y == 201.f);
	}
	test.move(1.5f);
	window->Pos.x += 15.f;
	window->Size.x = window->SizeFull.x = 600.f;
	test.move(2.f);
	check(window->Pos.x == 225.f && window->SizeFull.x == 800.f);
	test.move(1.f);
	check(window->Pos.x == 112.f && window->SizeFull.x == 400.f);
	test.panel(true);
	const float expanded = window->SizeFull.y, collapsed = window->Size.y;
	test.move(2.f);
	check(window->SizeFull.y == expanded * 2.f && window->Size.y == collapsed * 2.f);
	test.panel(false, true);
	check(window->Pos.x == 80.f && window->SizeFull.x == 320.f);
}

/// Check native-size bitmap selection, application enlargement, pending changes and custom fonts.
void fonts ()
{
	Context test;
	test.refresh();
	auto &io = ImGui::GetIO();
	auto &style = ImGui::GetStyle();
	test.scaling.selectFont();
	check(io.FontDefault == test.bitmap);
	for (float scale : {.75f, 1.25f, 2.f}) {
		style.FontScaleMain = scale;
		test.scaling.selectFont();
		check(io.FontDefault == test.vector);
	}
	style.FontScaleMain = 1.f;
	style.FontSizeBase = 17.f;
	test.scaling.selectFont();
	check(io.FontDefault == test.vector);
	style.FontSizeBase = 13.f;
	style._NextFrameFontSizeBase = 18.f;
	test.scaling.selectFont();
	check(io.FontDefault == test.vector);
	style._NextFrameFontSizeBase = 0.f;
	io.DisplayFramebufferScale = ImVec2(1, 1.5f);
	test.scaling.selectFont();
	check(io.FontDefault == test.vector);
	io.DisplayFramebufferScale = ImVec2(1, 1);
	test.scaling.selectFont();
	check(io.FontDefault == test.bitmap);
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
	io.FontGlobalScale = 2.f;
	test.scaling.selectFont();
	check(io.FontDefault == test.vector);
	io.FontGlobalScale = 1.f;
#endif
	ImFont *custom = io.Fonts->AddFontDefaultVector();
	io.FontDefault = custom;
	test.move(2.f);
	test.scaling.selectFont();
	check(io.FontDefault == custom);
	io.FontDefault = nullptr;
	test.scaling.selectFont();
	check(io.FontDefault == test.vector);
}

/// Check that a stale window query cannot give fonts a different density than the acquired target.
void framebufferConsistency ()
{
	using fcg::detail::GuiScaling;
	Context test;
	test.move(2.f, 2.f);
	auto &io = ImGui::GetIO();
	io.DisplaySize = ImVec2(800, 600);
	io.DisplayFramebufferScale = GuiScaling::framebufferScale(io.DisplaySize, ImVec2(1200, 900), ImVec2(2, 2));
	check(near(io.DisplayFramebufferScale.x, 1.5f));
	test.panel();
	const auto *draw = ImGui::GetDrawData();
	check(draw && draw->FramebufferScale.x == io.DisplayFramebufferScale.x);
	check(near(ImGui::GetFont()->CurrentRasterizerDensity, draw->FramebufferScale.x));
	check(GuiScaling::framebufferScale(ImVec2(0, 0), ImVec2(1200, 900), ImVec2(2, 2)).x == 2.f);
	check(GuiScaling::framebufferScale(ImVec2(800, 600), ImVec2(0, 0), ImVec2(0, 0)).x == 1.f);
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run CPU-only behavior checks; report failures through the test process exit status.
auto main () -> int
{
	try {
		eventsAndFailures();
		stylePreservation();
		coordinateModels();
		hostWindowScaling();
		windowGeometry();
		fonts();
		framebufferConsistency();
	}
	catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	return 0;
}

