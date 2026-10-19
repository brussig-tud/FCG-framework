
//////
//
// Includes
//

// C++ STL
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// SDL3 library
#include <SDL3/SDL_video.h>

// FCG Framework
#include <FCG/player.h>
#include <FCG/window.h>

// Dear ImGui (after the public template declaration, without optional arithmetic operators)
#include <imgui.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// A coordinate type whose arithmetic is found through ADL.
struct Distance {
	/// Coordinate component in authored or converted units.
	double value;
};

/// A type that deliberately has no multiplication operator.
struct NoProduct {};

/// A type whose multiplication deliberately returns an incompatible result.
struct InvalidProduct {};

/// A type whose multiplication deliberately fails at runtime.
struct ThrowingProduct {};

/// Scale a custom coordinate while preserving its type.
[[nodiscard]] auto operator* (Distance value, float scale) -> Distance { return {value.value * scale}; }

/// Supply an operator that cannot produce the original value type.
void operator* (InvalidProduct, float) {}

/// Verify that user arithmetic exceptions are allowed to propagate.
[[nodiscard]] auto operator* (ThrowingProduct, float) -> ThrowingProduct {
	throw std::runtime_error("custom multiplication");
}

/// A value supporting implicit conversion from the result of its multiplication.
class ConvertedDistance
{
public:

	////
	// Object construction

	/// Store a coordinate, including one returned by the multiplication operator.
	ConvertedDistance(double coordinate) : value(coordinate) {}


	////
	// Fields

	/// Coordinate component in authored or converted units.
	double value;
};

/// Return a scalar that is implicitly convertible to the original custom type.
[[nodiscard]] auto operator* (ConvertedDistance value, float scale) -> double { return value.value * scale; }

/// A value whose multiplication result cannot be converted successfully at runtime.
class ThrowingConversion
{
public:

	////
	// Object construction

	/// Create an input value without attempting result conversion.
	ThrowingConversion() = default;

	/// Fail when converting the scalar multiplication result back to this type.
	ThrowingConversion(float) { throw std::runtime_error("custom conversion"); }
};

/// Produce the scalar that triggers the throwing conversion constructor.
[[nodiscard]] auto operator* (ThrowingConversion, float scale) -> float { return scale; }

/// Whether the public conversion accepts a type and preserves its result type.
template <class T>
concept Convertible = requires (const fcg::Player &player, T value) {
	{ player.toGuiUnits(value) } -> std::same_as<T>;
};

static_assert(Convertible<float> && Convertible<double> && Convertible<long double>);
static_assert(Convertible<ImVec2> && Convertible<glm::vec2> && Convertible<glm::dvec2>);
static_assert(Convertible<Distance> && Convertible<ConvertedDistance> && Convertible<ThrowingProduct>);
static_assert(Convertible<ThrowingConversion>);
static_assert(!Convertible<int> && !Convertible<unsigned> && !Convertible<bool>);
static_assert(!Convertible<NoProduct> && !Convertible<InvalidProduct>);
static_assert(!noexcept(std::declval<const fcg::Player&>().toGuiUnits(ThrowingProduct{})));

/// Fail a probe without depending on assertions being enabled.
void check (bool condition) {
	if (!condition)
		throw std::runtime_error("GUI unit conversion failed");
}

/// Exercise all public conversion paths, including double precision and custom arithmetic.
void checkValues (const fcg::Player &player, float scale)
{
	check(player.toGuiUnits(400.f) == 400.f * scale);
	check(player.toGuiUnits(0.f) == 0.f);
	check(player.toGuiUnits(-20.f) == -20.f * scale);
	constexpr double precise = 16777217.25;
	check(player.toGuiUnits(precise) == precise * scale);
	const auto imgui = player.toGuiUnits(ImVec2(400.f, -20.f));
	check(imgui.x == 400.f * scale && imgui.y == -20.f * scale);
	check(player.toGuiUnits(glm::vec2(400.f, -20.f)) == glm::vec2(400.f * scale, -20.f * scale));
	check(player.toGuiUnits(glm::dvec2(precise, -precise)) == glm::dvec2(precise * scale, -precise * scale));
	check(player.toGuiUnits(Distance{precise}).value == precise * scale);
	check(player.toGuiUnits(ConvertedDistance(precise)).value == precise * scale);
	bool threw = false;
	try {
		(void)player.toGuiUnits(ThrowingProduct{});
	} catch (const std::runtime_error &error) {
		threw = std::string(error.what()) == "custom multiplication";
	}
	check(threw);
	threw = false;
	try {
		(void)player.toGuiUnits(ThrowingConversion{});
	} catch (const std::runtime_error &error) {
		threw = std::string(error.what()) == "custom conversion";
	}
	check(threw);
}

/// Exercise the runtime binding and independence from mutable ImGui state.
class UnitsProbe final : public fcg::Applet
{
public:

	////
	// Interface: fcg::Applet

	/// Name used by the runtime log.
	[[nodiscard]] auto name () const -> const std::string& override {
		static const std::string label = "GUI units probe";
		return label;
	}

	/// Keep the loop running and verify that a window alone does not attach a GUI to a manually created player.
	void init (fcg::Device &device, fcg::Player &player) override {
		player.pushContinuousRedraw();
		fcg::Player unattached(device, player.mainWindow(), {});
		checkValues(unattached, 1.f);
	}

	/// No viewport-dependent resources are owned by this probe.
	void onViewportResize (fcg::Device&, const glm::uvec2&, fcg::Player&) override {}

	/// Draw a panel, then deliberately dirty the global geometry before render preparation.
	void gui (fcg::Device &device, fcg::Player &player) override
	{
		auto *window = player.mainWindow()->handle();
		scale = SDL_GetWindowDisplayScale(window) / SDL_GetWindowPixelDensity(window);
		if (std::abs(scale - 1.f) <= .001f)
			scale = 1.f;
		checkValues(player, scale);
		auto &io = ImGui::GetIO();
		display = io.DisplaySize;
		density = io.DisplayFramebufferScale;
		ImGui::SetNextWindowSize(player.toGuiUnits(ImVec2(200.f, 100.f)), ImGuiCond_Always);
		ImGui::Begin("Conversion probe");
		ImGui::TextUnformatted("GUI units");
		ImGui::End();
		auto &style = ImGui::GetStyle();
		const float savedDpi = style.FontScaleDpi, savedMain = style.FontScaleMain;
		style.FontScaleDpi = 7.f;
		style.FontScaleMain = 3.f;
		io.DisplaySize = {32, 64};
		io.DisplayFramebufferScale = {8, 9};
		checkValues(player, scale);
		fcg::Player unattached(device, nullptr, {});
		checkValues(unattached, 1.f);
		style.FontScaleDpi = savedDpi;
		style.FontScaleMain = savedMain;
	}

	/// The conversion remains available after all GUI callbacks.
	void update (fcg::Device&, fcg::Player &player, float) override {
		checkValues(player, scale);
		if (++frames == 3)
			player.requestClose();
	}

	/// Check that render preparation restores the geometry selected before layout.
	void render (
		fcg::Device&, fcg::RenderState&, SDL_GPURenderPass*, SDL_GPUCommandBuffer*, fcg::Player &player
	) override
	{
		checkValues(player, scale);
		const auto *draw = ImGui::GetDrawData();
		check(draw && draw->DisplaySize.x == display.x && draw->DisplaySize.y == display.y);
		check(draw->FramebufferScale.x == density.x && draw->FramebufferScale.y == density.y);
		++renders;
	}


	////
	// Fields

	/// Render callbacks reached by the probe.
	static inline unsigned renders = 0;


private:

	////
	// Fields

	/// Expected coordinate scale independently queried from SDL.
	float scale = 1.f;

	/// Geometry observed before deliberately modifying global state.
	ImVec2 display{};

	/// Framebuffer density observed before deliberately modifying global state.
	ImVec2 density{};

	/// Number of iterations, bounding the probe's duration.
	unsigned frames = 0;
};

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run the public API probe through the real runtime and require actual rendering.
auto main () -> int
{
	try {
		const auto status = fcg::run({}, std::make_unique<UnitsProbe>());
		return status == 0 && UnitsProbe::renders > 0 ? 0 : 1;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
