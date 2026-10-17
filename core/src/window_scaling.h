
#ifndef __FCG_WINDOW_SCALING_H__
#define __FCG_WINDOW_SCALING_H__


//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>
#include <limits>

// GLM library
#include <glm/glm.hpp>



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

/// Tracks a normal window's unrounded client dimensions at 100% display scale.
class WindowScaling
{
public:

	////
	// Object construction

	/// Initialize the reference dimensions from the requested size at 100% scale.
	explicit WindowScaling(glm::ivec2 size) : m_reference(size) {}


	////
	// Methods

	/// \brief Reconcile native window geometry with the current display scale.
	///
	/// A resize callback returns the actual size after requesting and synchronizing the resize. A constrained
	/// or rejected request preserves the reference size and is not repeated until another scale transition.
	///
	/// \tparam Resize A callable accepting and returning client dimensions as <tt>glm::ivec2</tt>.
	/// \param display The window display scale in pixels per base unit.
	/// \param density The window pixel density in pixels per window-coordinate unit.
	/// \param size The current client dimensions in window coordinates.
	/// \param suspended Whether the window is minimized, maximized or fullscreen.
	/// \param resize The operation used to request a new client size.
	template <class Resize>
	void refresh (float display, float density, glm::ivec2 size, bool suspended, Resize &&resize)
	{
		if (suspended) {
			m_suspended = true;
			return;
		}
		if (!positive(display) || !positive(density) || size.x <= 0 || size.y <= 0)
			return;
		const float ratio = display / density;
		if (!positive(ratio))
			return;
		const float nextScale = std::abs(ratio - 1.f) <= tolerance ? 1.f : ratio;
		const bool changed = std::abs(nextScale - m_scale) > tolerance;
		// A late acknowledgement of our requested size is not an application/user resize.
		if (m_initialized && !m_suspended && size != m_observed && size != m_requested) {
			m_reference = glm::dvec2(size) / double(m_scale);
			m_requested = {0, 0};
		}
		m_observed = size;
		if (m_initialized && !m_suspended && !changed)
			return;
		m_initialized = true;
		m_suspended = false;
		if (changed)
			m_scale = nextScale;
		m_requested = {dimension(m_reference.x * m_scale), dimension(m_reference.y * m_scale)};
		if (m_requested != size)
			m_observed = resize(m_requested);
	}


private:

	////
	// Constants

	/// Matches the GUI's tolerance for display-coordinate scale changes.
	static constexpr float tolerance = .001f;


	////
	// Methods

	/// Whether a scale query can be used for coordinate conversion.
	[[nodiscard]] static auto positive (float value) -> bool {
		return std::isfinite(value) && value > 0.f;
	}

	/// Round once and constrain the result to the positive range accepted by SDL.
	[[nodiscard]] static auto dimension (double value) -> int {
		return static_cast<int>(std::clamp(std::round(value), 1., double(std::numeric_limits<int>::max())));
	}


	////
	// Fields

	/// Unrounded normal-window client dimensions at 100% scale.
	glm::dvec2 m_reference;

	/// Most recently observed client dimensions, including constrained resize results.
	glm::ivec2 m_observed = {0, 0};

	/// Most recently requested dimensions, used to recognize delayed resize acknowledgements.
	glm::ivec2 m_requested = {0, 0};

	/// Coordinate scale of the most recently handled transition.
	float m_scale = 1.f;

	/// Whether initial scaling has been applied or attempted.
	bool m_initialized = false;

	/// Whether normal-window sizing must be reapplied after restoration.
	bool m_suspended = false;
};



//////
//
// Namespaces close
//

// namespace detail
}

// namespace fcg
}


#endif  // ifndef __FCG_WINDOW_SCALING_H__
