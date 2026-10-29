
#ifndef __FCG_RENDER_LINE_SET_BUILDER_H__
#define __FCG_RENDER_LINE_SET_BUILDER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_line_sets Building line sets
/// \ingroup fcg_modules
/// \brief Collect polylines on the CPU and upload them for drawing.
///
/// Use <tt>\ref fcg::LineSetBuilder "LineSetBuilder"</tt> to assemble positions, colors and full pixel widths without
/// writing connectivity indices. Uploading returns a <tt>\ref fcg::LineSet "LineSet"</tt> to pass directly to
/// <tt>\ref fcg::LineRenderer::draw "LineRenderer::draw"</tt>. See \ref fcg_lines for guidance on line rendering.
///
/// \section fcg_line_sets_usage Getting started
/// Include the builder and collect nodes with
/// <tt>\ref fcg::LineSetBuilder::recordPolyline "recordPolyline"</tt>, which calls your callback immediately to
/// append one CPU polyline. Use <tt>\ref fcg::LineSetBuilder::addPolyline "addPolyline"</tt> to copy a list or span
/// of nodes instead. Upload the collected geometry on your rendering device:
/// \snippet line_set_examples.cpp includes
/// \snippet line_set_examples.cpp build
/// Check the upload result before drawing:
/// \snippet line_set_examples.cpp draw
///
/// \section fcg_line_sets_workflows Common tasks
/// Pass \c true as the second argument to
/// <tt>\ref fcg::LineSetBuilder::recordPolyline "recordPolyline"</tt> or
/// <tt>\ref fcg::LineSetBuilder::addPolyline "addPolyline"</tt> to close a loop. Closure connects the last node to
/// the first; adjacent segments retain their independent caps and can overlap at shared nodes.
/// \snippet line_set_examples.cpp closed
/// Widths are full widths in physical framebuffer pixels. Supply them per node or use
/// <tt>\ref fcg::LineSetBuilder::setConstantWidth "setConstantWidth"</tt> for the entire set. The same choice
/// applies to colors through <tt>\ref fcg::LineSetBuilder::setConstantColor "setConstantColor"</tt>.
/// \snippet line_set_examples.cpp widths
/// To rebuild, clear the CPU geometry and upload into the existing GPU set:
/// \snippet line_set_examples.cpp rebuild
/// <tt>\ref fcg::LineSetBuilder::clear "clear"</tt> retains constant settings. Clear a constant explicitly before
/// supplying that attribute per node. Uploads retain the CPU geometry and never wait for GPU completion.
///
/// \section fcg_line_sets_lifetime Ownership and lifetime
/// The device must outlive the uploaded set, which owns its GPU collections and their buffers. Builders own their
/// CPU nodes; input spans and callback references are borrowed only for the duration of their calls.
///
/// \section fcg_line_sets_errors Handling errors
/// Uploads report operational failures through <tt>\ref fcg::RenderError "RenderError"</tt>. A failed replacement
/// leaves the GPU set empty and usable; retry with the same builder. CPU packing exceptions leave it unchanged.
/// Input contracts are unchecked; violating them results in undefined behavior.



//////
//
// Includes
//

// C++ STL
#include <functional>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

// GLM library
#include <glm/glm.hpp>

// FCG Framework
#include <FCG/Render/primitive_attributes.h>
#include <FCG/Render/primitive_connectivity.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_line_sets
/// @{



//////
//
// Structs and enums
//

/// \brief A move-only owner of node attributes and pairwise node connectivity.
///
/// Obtain a set from <tt>LineSetBuilder::upload</tt>, or construct from existing \c PrimitiveAttributes and
/// \c PrimitiveConnectivity collections. The \c LineSetBuilder ensures that the connectivity information is compatible
/// with any \c ConnectedPrimitiveRenderer that draws pairs of nodes, such as <tt>LineRenderer</tt>.
struct LineSet
{
	/// \brief Take ownership of existing node attributes and connectivity.
	///
	/// The collections retain their contents and source views. Each source collection subsequently permits only
	/// assignment and destruction. Bound caller-managed buffers remain borrowed.
	///
	/// \pre
	/// 	No attribute update is active. Each collection's device outlives that collection under this set's ownership.
	///
	/// \param attribs The node attributes whose ownership is transferred to <tt>attributes</tt>.
	/// \param connectivity The node indices whose ownership is transferred to this set's <tt>connectivity</tt>.
	explicit LineSet(PrimitiveAttributes &&attribs, PrimitiveConnectivity &&connectivity)
		: attributes(std::move(attribs)), connectivity(std::move(connectivity))
	{}

	/// Owned node attributes, including positions and optional pixel radii and colors.
	PrimitiveAttributes attributes;

	/// Owned connectivity, with each consecutive pair of node indices selecting one segment.
	PrimitiveConnectivity connectivity;
};

/// \brief A Cartesian node with optional linear RGBA color and full pixel width.
struct LineNode
{
	/// \brief Construct a node without an explicit color.
	///
	/// \param position The Cartesian position in model coordinates.
	/// \param width The full stroke width in physical pixels, or absent.
	LineNode(glm::vec3 position, std::optional<float> width={}) : position(position), width(width) {}

	/// \brief Construct a colored node.
	///
	/// \param position The Cartesian position in model coordinates.
	/// \param color The linear RGBA color.
	/// \param width The full stroke width in physical pixels, or absent.
	LineNode(glm::vec3 position, glm::vec4 color, std::optional<float> width={})
		: position(position), color(color), width(width) {}

	/// Cartesian position in model coordinates.
	glm::vec3 position;

	/// Linear RGBA color, or absent to use the set's constant or the renderer default.
	std::optional<glm::vec4> color;

	/// Full stroke width in physical pixels, or absent to use the set's constant or the renderer default.
	std::optional<float> width;
};



//////
//
// Classes
//

/// \brief Collect open or closed polylines for one uploaded line set.
///
/// For <em>width</em> and <em>color</em>, choose either a set-wide constant, values on every node, or no values.
/// Missing widths use \c LineStyle::defaultWidth; missing colors use opaque white. Colors and widths must be finite and
/// nonnegative. Generated counts and byte sizes must fit the SDL GPU API's 32-bit limits.
///
/// Nonempty open polylines require at least two nodes, and closed ones require at least three. Empty polylines are
/// ignored. Contracts are unchecked for efficiency, and violations result in undefined behavior. Serialize access to
/// each builder.
class FCG_RENDER_EXPORT LineSetBuilder
{
public:

	////
	// Types

	/// \brief A temporary polyline supplied only to a \c LineSetBuilder::recordPolyline callback.
	class FCG_RENDER_EXPORT Polyline
	{
		////
		// Friend declarations

		/// The builder creates the callback's polyline and takes its collected nodes.
		friend class LineSetBuilder;


	public:

		////
		// Object construction/destruction

		/// Not copyable.
		Polyline(const Polyline&) = delete;

		/// Not copy-assignable.
		auto operator= (const Polyline&) -> Polyline& = delete;

		/// Not movable.
		Polyline(Polyline&&) = delete;

		/// Not move-assignable.
		auto operator= (Polyline&&) -> Polyline& = delete;


		////
		// Methods

		/// \brief Append a node, copying its values.
		///
		/// \param node The node to append.
		void addNode (LineNode node);

		/// \brief Append a node without an explicit color.
		///
		/// \param position The Cartesian position in model coordinates.
		/// \param width The full stroke width in physical pixels, or absent.
		void addNode (glm::vec3 position, std::optional<float> width={});

		/// \brief Append a colored node.
		///
		/// \param position The Cartesian position in model coordinates.
		/// \param color The linear RGBA color.
		/// \param width The full stroke width in physical pixels, or absent.
		void addNode (glm::vec3 position, glm::vec4 color, std::optional<float> width={});


	private:

		////
		// Object construction

		/// Construct an empty callback draft.
		Polyline() = default;


		////
		// Fields

		/// CPU nodes owned by this callback draft.
		std::vector<LineNode> nodes;
	};


	////
	// Object construction

	/// Construct an empty builder with no constants.
	LineSetBuilder() = default;

	/// \brief Create an empty builder with no constants.
	///
	/// \return The empty builder.
	[[nodiscard]] static auto create () -> LineSetBuilder;


	////
	// Methods

	/// \brief Set the full pixel width for the entire set, including previously added nodes.
	///
	/// \pre No node has an explicit width.
	///
	/// \param width The full stroke width in physical pixels.
	void setConstantWidth (float width);

	/// Remove the set-wide width, permitting per-node widths or the renderer default.
	void clearConstantWidth ();

	/// \brief Set the linear RGBA color for the entire set, including previously added nodes.
	///
	/// \pre No node has an explicit color.
	///
	/// \param color The linear RGBA color.
	void setConstantColor (glm::vec4 color);

	/// Remove the set-wide color, permitting per-node colors or opaque white.
	void clearConstantColor ();

	/// \brief Record one polyline in a synchronous callback and append it when the callback returns.
	///
	/// A throwing callback appends nothing. Its exception propagates unchanged.
	///
	/// \pre The callback neither retains the draft nor modifies this builder.
	///
	/// \tparam Fn A callable accepting \c Polyline&.
	/// \param fn The callback that appends nodes to its borrowed draft.
	/// \param closed Whether to connect the last node to the first.
	///
	/// \throws ... Exceptions from the callback propagate unchanged.
	template<class Fn> void recordPolyline (Fn &&fn, bool closed=false) {
		Polyline draft;
		std::invoke(std::forward<Fn>(fn), draft);
		append(std::move(draft.nodes), closed);
	}

	/// \brief Copy a polyline's nodes into this builder.
	///
	/// \param nodes The nodes, borrowed until return.
	/// \param closed Whether to connect the last node to the first.
	void addPolyline (std::span<const LineNode> nodes, bool closed=false);

	/// \brief Copy a list of nodes into this builder.
	///
	/// \param nodes The nodes, borrowed until return.
	/// \param closed Whether to connect the last node to the first.
	void addPolyline (std::initializer_list<LineNode> nodes, bool closed=false);

	/// Remove geometry, retaining constant settings and reusable CPU capacity.
	void clear ();

	/// \brief Upload a new GPU set without consuming the CPU geometry.
	///
	/// Creates both collections on \p device and returns a set owning them. An empty builder returns an empty set.
	///
	/// \pre The live device outlives the returned set.
	///
	/// \param device The owning device.
	///
	/// \return The uploaded set, or an operational upload error from the underlying collections or SDL.
	[[nodiscard]] auto upload (Device &device) const -> std::expected<LineSet, RenderError>;

	/// \brief Replace an existing set, reusing GPU storage and retaining the CPU geometry.
	///
	/// All attribute sources and connectivity are replaced. An empty builder clears the set without submitting.
	/// Uploads submit once and never wait for GPU completion. CPU packing exceptions leave the set unchanged;
	/// subsequent failures or exceptions leave it empty and usable for retry.
	///
	/// \pre Both collections are live, share a live device, and are used without concurrent updates or draws.
	///
	/// \param destination The set to replace.
	///
	/// \return Success, or an operational upload error from the underlying collections or SDL.
	[[nodiscard]] auto upload (LineSet &destination) const -> std::expected<void, RenderError>;


private:

	////
	// Types

	/// A contiguous range in the retained node vector.
	struct Path
	{
		/// Offset of its first node.
		std::size_t first;

		/// Number of nodes in this path.
		std::size_t count;

		/// Whether to emit the closing segment.
		bool closed;
	};


	////
	// Methods

	/// \brief Append a collected draft, preserving the builder if allocation fails.
	///
	/// \param nodes The nodes to copy into retained CPU capacity.
	/// \param closed Whether to emit the closing segment.
	void append (std::vector<LineNode> nodes, bool closed);


	////
	// Fields

	/// Flattened CPU nodes, in polyline insertion order.
	std::vector<LineNode> nodes;

	/// CPU polyline boundaries, in insertion order.
	std::vector<Path> paths;

	/// Optional full pixel width shared by all nodes.
	std::optional<float> constantWidth;

	/// Optional linear RGBA color shared by all nodes.
	std::optional<glm::vec4> constantColor;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_LINE_SET_BUILDER_H__
