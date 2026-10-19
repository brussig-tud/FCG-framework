
#ifndef __FCG_GUI_SCALING_H__
#define __FCG_GUI_SCALING_H__


//////
//
// Includes
//

// C++ STL
#include <array>
#include <cfloat>
#include <cmath>
#include <unordered_map>

// Dear ImGui
#include <imgui.h>
#include <imgui_internal.h>

// SDL3 library
#include <SDL3/SDL.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Implementation details of the framework.
namespace detail {



//////
//
// Classes
//

/// \brief Event-driven display scaling and preservation of application-owned ImGui state.
///
/// This adapter follows ImGui 1.92.9's style and window scaling rules; audit the member inventories on upgrades.
class GuiScaling
{
public:

	////
	// Types

	/// A coherent display query result, expressed in physical pixels per base unit and per window unit.
	struct DisplayScale {
		/// The operating system's desired content scale for the window.
		float display = 1.f;

		/// Physical pixels per window-coordinate unit.
		float density = 1.f;
	};


	////
	// Object construction

	/// \brief Store the main window ID and borrowed, atlas-owned default fonts.
	///
	/// \param window The main window's SDL ID.
	/// \param bitmap The atlas-owned native-size default font.
	/// \param vector The atlas-owned scalable default font.
	GuiScaling(SDL_WindowID window, ImFont *bitmap, ImFont *vector)
		: windowId(window), bitmapFont(bitmap), vectorFont(vector)
	{}


	////
	// Accessors

	/// ImGui coordinate units per dimension authored at 100% display scale.
	[[nodiscard]] auto unitScale () const noexcept -> float {
		return scale;
	}

	/// Display dimensions for the current GUI frame, zero while minimized.
	[[nodiscard]] auto displaySize () const noexcept -> ImVec2 {
		return m_displaySize;
	}

	/// Framebuffer pixels per ImGui coordinate unit for the current GUI frame.
	[[nodiscard]] auto framebufferScale () const noexcept -> ImVec2 {
		return m_framebufferScale;
	}

	/// Whether the last requested display query failed and should be retried.
	[[nodiscard]] auto retryPending () const -> bool {
		return m_retryPending;
	}


	////
	// Methods

	/// \brief Invalidate cached display information for relevant main-window events.
	///
	/// \param event An SDL event forwarded before starting the next GUI frame.
	void processEvent (const SDL_Event &event)
	{
		switch (event.type)
		{
			case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
			case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_SHOWN:
			case SDL_EVENT_WINDOW_RESTORED:
				if (event.window.windowID == windowId)
					dirty = true;
				break;
			default:
				break;
		}
	}

	/// \brief Query only when invalidated; keep the last valid scale through temporary failures.
	///
	/// The query is injected privately to allow deterministic tests without a display server.
	///
	/// \tparam Query A callable returning <tt>DisplayScale</tt>.
	/// \param query A display query evaluated at most once during this call.
	template <class Query>
	void refresh (Query &&query)
	{
		if (!dirty)
			return;

		const DisplayScale values = query();
		if (!positive(values.display) || !positive(values.density)) {
			m_retryPending = true;
			return;
		}

		const float newScale = values.display / values.density;
		if (!positive(newScale)) {
			m_retryPending = true;
			return;
		}

		dirty = false;
		m_retryPending = false;
		const float nextScale = std::abs(newScale - 1.f) <= tolerance ? 1.f : newScale;
		ImGuiStyle &style = ImGui::GetStyle();
		if (!captured) {
			reference = applied = style;
			captured = true;
		}
		if (std::abs(nextScale - scale) > tolerance) {
			scaleStyle(style, nextScale);
			scaleWindows(nextScale);
			scale = nextScale;
		}

		style.FontScaleDpi = scale;
	}

	/// Choose the framework font at frame boundaries, leaving custom default fonts untouched.
	void selectFont ()
	{
		ImGuiIO &io = ImGui::GetIO();
		ImGuiStyle &style = ImGui::GetStyle();
		style.FontScaleDpi = scale;
		if (io.FontDefault && io.FontDefault != bitmapFont && io.FontDefault != vectorFont)
			return;
		const float base = style._NextFrameFontSizeBase > 0.f ? style._NextFrameFontSizeBase
			: style.FontSizeBase > 0.f ? style.FontSizeBase : 13.f;
		float size = base * style.FontScaleMain * scale;
	#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
		size *= io.FontGlobalScale;
	#endif
		const ImVec2 density = m_framebufferScale;
		const bool native = std::abs(size - 13.f) <= tolerance
			&& std::abs(density.x - 1.f) <= tolerance && std::abs(density.y - 1.f) <= tolerance;
		io.FontDefault = native ? bitmapFont : vectorFont;
	}

	/// \brief Retain frame geometry from native window queries and the acquired render target.
	///
	/// Invalid window dimensions retain the previous normal dimensions. An acquired target takes precedence over
	/// the queried pixel dimensions; invalid ratios retain the previous framebuffer scale. No ImGui state is read.
	///
	/// \param window Native SDL window dimensions, or zero when the query failed.
	/// \param pixels SDL pixel dimensions, or zero when the query failed.
	/// \param target Acquired frame dimensions, or zero when no frame is available.
	/// \param minimized Whether the display dimensions should be zero for this frame.
	void updateGeometry (ImVec2 window, ImVec2 pixels, ImVec2 target, bool minimized)
	{
		if (positive(window.x) && positive(window.y))
			m_windowSize = window;
		m_displaySize = minimized ? ImVec2(0.f, 0.f) : m_windowSize;
		if (!positive(m_displaySize.x) || !positive(m_displaySize.y))
			return;
		const ImVec2 extent = positive(target.x) && positive(target.y) ? target : pixels;
		const ImVec2 density(extent.x / m_displaySize.x, extent.y / m_displaySize.y);
		if (positive(density.x) && positive(density.y))
			m_framebufferScale = density;
	}


private:

	////
	// Types

	/// Unrounded geometry and last displayed geometry for a single window dimension.
	struct Geometry {
		/// Window units before framework scaling.
		ImVec2 reference{};

		/// The last geometry written by the framework, in current window units.
		ImVec2 applied{};
	};

	/// Geometry tracked separately to preserve moves, resizes and collapsed window dimensions.
	struct WindowGeometry
	{
		/// Position relative to the main viewport origin.
		Geometry position;

		/// Current window size, including the collapsed size.
		Geometry size;

		/// Expanded window size.
		Geometry fullSize;

		/// Cached content size.
		Geometry content;
	};


	////
	// Constants

	/// Tolerance used for display-scale comparisons and native font selection.
	static constexpr float tolerance = .001f;

	/// \brief Scalar members scaled by ImGui 1.92.9's <tt>ImGuiStyle::ScaleAllSizes</tt>.
	///
	/// The two tab width thresholds below require sentinel-aware normalization.
	inline static constexpr std::array scalarDimensions {
		&ImGuiStyle::_MainScale,
		&ImGuiStyle::WindowRounding, &ImGuiStyle::WindowBorderSize, &ImGuiStyle::WindowBorderHoverPadding,
		&ImGuiStyle::ChildRounding, &ImGuiStyle::ChildBorderSize,
		&ImGuiStyle::PopupRounding, &ImGuiStyle::PopupBorderSize,
		&ImGuiStyle::FrameRounding, &ImGuiStyle::FrameBorderSize,
		&ImGuiStyle::IndentSpacing, &ImGuiStyle::ColumnsMinSpacing,
		&ImGuiStyle::ScrollbarSize, &ImGuiStyle::ScrollbarRounding, &ImGuiStyle::ScrollbarPadding,
		&ImGuiStyle::GrabMinSize, &ImGuiStyle::GrabRounding, &ImGuiStyle::LogSliderDeadzone,
		&ImGuiStyle::ImageRounding, &ImGuiStyle::ImageBorderSize,
		&ImGuiStyle::TabRounding, &ImGuiStyle::TabBorderSize,
		&ImGuiStyle::TabMinWidthBase, &ImGuiStyle::TabMinWidthShrink,
		&ImGuiStyle::TabCloseButtonMinWidthSelected, &ImGuiStyle::TabCloseButtonMinWidthUnselected,
		&ImGuiStyle::TabBarBorderSize, &ImGuiStyle::TabBarOverlineSize,
		&ImGuiStyle::TreeLinesSize, &ImGuiStyle::TreeLinesRounding,
		&ImGuiStyle::MenuItemRounding, &ImGuiStyle::SelectableRounding,
		&ImGuiStyle::DragDropTargetRounding, &ImGuiStyle::DragDropTargetBorderSize,
		&ImGuiStyle::DragDropTargetPadding, &ImGuiStyle::ColorMarkerSize,
		&ImGuiStyle::InputTextCursorSize, &ImGuiStyle::SeparatorSize,
		&ImGuiStyle::SeparatorTextBorderSize, &ImGuiStyle::MouseCursorScale
	};

	/// Vector members scaled by ImGui 1.92.9's <tt>ImGuiStyle::ScaleAllSizes</tt>.
	inline static constexpr std::array vectorDimensions {
		&ImGuiStyle::WindowPadding, &ImGuiStyle::WindowMinSize, &ImGuiStyle::FramePadding,
		&ImGuiStyle::ItemSpacing, &ImGuiStyle::ItemInnerSpacing, &ImGuiStyle::CellPadding,
		&ImGuiStyle::TouchExtraPadding, &ImGuiStyle::SeparatorTextPadding,
		&ImGuiStyle::DisplayWindowPadding, &ImGuiStyle::DisplaySafeAreaPadding
	};


	////
	// Methods

	/// Whether a queried dimension or scale is finite and strictly positive.
	[[nodiscard]] static auto positive (float value) -> bool {
		return std::isfinite(value) && value > .0f;
	}

	/// Incorporate a direct application edit without re-normalizing unchanged rounded values.
	void remember (float &reference, float applied, float live, bool sentinel=false) const {
		if (live != applied)
			reference = sentinel ? live : live / scale;
	}

	/// Restore only dimensional members from their unrounded reference, preserving all other live style state.
	void scaleStyle (ImGuiStyle &style, float nextScale)
	{
		for (auto member : scalarDimensions) {
			const float live = style.*member;
			const bool threshold =   member == &ImGuiStyle::TabCloseButtonMinWidthSelected
			                      || member == &ImGuiStyle::TabCloseButtonMinWidthUnselected;
			remember(
				reference.*member, applied.*member, live, threshold && (live <= .0f || live == FLT_MAX)
			);
		}
		for (auto member : vectorDimensions) {
			remember((reference.*member).x, (applied.*member).x, (style.*member).x);
			remember((reference.*member).y, (applied.*member).y, (style.*member).y);
		}
		ImGuiStyle scaled = reference;
		if (nextScale != 1.f)
			scaled.ScaleAllSizes(nextScale);
		for (auto member : scalarDimensions)
			style.*member = scaled.*member;
		for (auto member : vectorDimensions)
			style.*member = scaled.*member;
		applied = scaled;
	}

	/// Preserve the unrounded reference for geometry that has not changed since the last transition.
	void scaleGeometry (ImVec2 &live, Geometry &saved, float nextScale, bool position) {
		remember(saved.reference.x, saved.applied.x, live.x);
		remember(saved.reference.y, saved.applied.y, live.y);
		const float x = saved.reference.x * nextScale, y = saved.reference.y * nextScale;
		live = position ? ImVec2(std::floor(x), std::floor(y)) : ImVec2(std::trunc(x), std::trunc(y));
		saved.applied = live;
	}

	/// Scale existing windows using the same geometry fields as ImGui's internal viewport-scaling helper.
	void scaleWindows (float nextScale)
	{
		ImGuiContext &context = *ImGui::GetCurrentContext();
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		for (ImGuiWindow *window : context.Windows)
		{
			if (window->Viewport != viewport)
				continue;
			auto [entry, inserted] = trackedWindows.try_emplace(window->ID);
			WindowGeometry &saved = entry->second;
			ImVec2 position(window->Pos.x - viewport->Pos.x, window->Pos.y - viewport->Pos.y);
			if (inserted) {
				// A newly observed window's geometry is already expressed at the previous scale.
				saved.position.applied = saved.size.applied = saved.fullSize.applied = saved.content.applied
					= ImVec2(FLT_MAX, FLT_MAX);
			}
			scaleGeometry(position, saved.position, nextScale, true);
			window->Pos = ImVec2(position.x + viewport->Pos.x, position.y + viewport->Pos.y);
			scaleGeometry(window->Size, saved.size, nextScale, false);
			scaleGeometry(window->SizeFull, saved.fullSize, nextScale, false);
			scaleGeometry(window->ContentSize, saved.content, nextScale, false);
		}
		if (context.MovingWindow) {
			context.ActiveIdClickOffset.x *= nextScale / scale;
			context.ActiveIdClickOffset.y *= nextScale / scale;
		}
	}


	////
	// Fields

	/// The main window whose display events invalidate the cache.
	SDL_WindowID windowId;

	/// Borrowed bitmap default font, owned by the current font atlas.
	ImFont *bitmapFont;

	/// Borrowed scalable default font, owned by the current font atlas.
	ImFont *vectorFont;

	/// Whether a display query is needed before starting a GUI frame.
	bool dirty = true;

	/// Whether an invalid query needs a subsequent retry.
	bool m_retryPending = false;

	/// Whether initial applet style customizations have been captured.
	bool captured = false;

	/// Last valid native window dimensions, retained through minimization and failed queries.
	ImVec2 m_windowSize = {0.f, 0.f};

	/// Main viewport dimensions published at the current frame boundary.
	ImVec2 m_displaySize = {0.f, 0.f};

	/// Render-target pixels per coordinate unit, shared by font selection and rendering.
	ImVec2 m_framebufferScale = {1.f, 1.f};

	/// The last successfully applied coordinate scale.
	float scale = 1.f;

	/// Unrounded dimensional baseline; non-dimensional fields are never restored from this copy.
	ImGuiStyle reference{};

	/// The last scaled style, used only to identify direct dimensional edits.
	ImGuiStyle applied{};

	/// Normalized window geometry, inspected only during coordinate-scale transitions.
	std::unordered_map<ImGuiID, WindowGeometry> trackedWindows;
};



//////
//
// Namespaces close
//

// namespace detail
}

// namespace fcg
}


#endif
