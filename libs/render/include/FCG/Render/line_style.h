
#ifndef __FCG_RENDER_LINE_STYLE_H__
#define __FCG_RENDER_LINE_STYLE_H__


//////
//
// Includes
//

// C++ STL
/* nothing here yet */



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_lines
/// @{



//////
//
// Structs and enums
//

/// \brief Shape of a line segment's endpoint boundary.
enum class LineCap {
	/// An outward semicircle with the endpoint's pixel radius.
	Round,

	/// A straight boundary perpendicular to the projected segment.
	Butt
};

/// \brief Method used to compute coverage at a stroke boundary.
enum class LineFilter {
	/// Hard geometric coverage, including when alpha blending is enabled.
	None,

	/// A derivative-scaled smooth transition, applied when alpha blending is enabled.
	Smoothstep
};

/// \brief Appearance of pixel-width strokes drawn by \c LineRenderer and \c WireBoxRenderer.
struct LineStyle
{
	/// Full stroke width in physical pixels when \c Attribute::Radius is absent; finite and nonnegative.
	float defaultWidth = 1.f;

	/// Endpoint shape; clipping intersections always use butt boundaries.
	LineCap cap = LineCap::Round;

	/// Coverage filter; blending-disabled pipelines always draw hard edges.
	LineFilter filter = LineFilter::Smoothstep;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_LINE_STYLE_H__
