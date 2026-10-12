
#ifndef __FCG_RENDER_PRIMITIVE_ATTRIBUTES_H__
#define __FCG_RENDER_PRIMITIVE_ATTRIBUTES_H__


//////
//
// Includes
//

// C++ STL
#include <array>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>

// GLM library
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// FCG Framework
#include <FCG/buffer.h>
#include <FCG/Render/export.h>
#include <FCG/Render/error.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_primitives
/// @{



//////
//
// Structs and enums
//

/// \brief
/// 	Per-instance attributes stored in a \c PrimitiveAttributes collection.
///
/// A renderer uses only the attributes listed by its \c PrimitiveRenderer::supportedAttributes and ignores all others.
enum class Attribute
{
	/// \brief
	/// 	Center of the instance as a homogeneous point (x, y, z, w). The drawn center is (x, y, z) / w.
	///
	/// All components must be finite and w must be nonzero. Positions can only be supplied as an array or a buffer
	/// view, never as a constant. The number of positions determines the instance count of the collection. Without a
	/// position source, the collection has zero instances.
	Position,

	/// Per-instance normal. The renderers of this library do not use it.
	Normal,

	/// Per-instance radius. The renderers of this library do not use it.
	Radius,

	/// \brief
	/// 	Half size of the instance along its local X, Y and Z axes.
	///
	/// All components must be finite and nonnegative. Zero is allowed and collapses the instance along that axis.
	/// Without an extent source, every instance has the extent (1, 1, 1).
	Extent,

	/// \brief
	/// 	Rotation of the instance, given as a quaternion.
	///
	/// All components must be finite and not all zero. Renderers normalize the quaternion before use, so its length
	/// does not matter. Without an orientation source, instances are not rotated.
	Orientation,

	/// \brief
	/// 	Color of the instance as RGBA.
	///
	/// When drawing with a texture, the color is multiplied by the texture color. When lighting is enabled, its RGB
	/// part is scaled by the lighting and its alpha is kept. Without a color source, instances are opaque white
	/// (1, 1, 1, 1).
	Color,

	/// The number of attributes. Not an attribute itself.
	Count
};

/// \brief
/// 	Value type and memory layout of an <tt>Attribute</tt>.
///
/// \tparam A The attribute.
template <Attribute A> struct AttributeTraits;

/// Value type and memory layout of <tt>Attribute::Position</tt>.
template <> struct AttributeTraits<Attribute::Position>
{
	/// Type of one array element or buffer view element.
	using Value = glm::vec4;

	/// Whether the attribute can be supplied as a constant.
	static constexpr bool constantSupported = false;

	/// Byte offsets of the x, y, z and w components within a <tt>Value</tt>.
	static constexpr std::array<std::size_t, 4> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z), offsetof(Value, w)
	};
};

/// Value type and memory layout of <tt>Attribute::Normal</tt>.
template <> struct AttributeTraits<Attribute::Normal>
{
	/// Type of one array element, buffer view element or constant.
	using Value = glm::vec3;

	/// Whether the attribute can be supplied as a constant.
	static constexpr bool constantSupported = true;

	/// Byte offsets of the x, y and z components within a <tt>Value</tt>.
	static constexpr std::array<std::size_t, 3> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z)
	};
};

/// Value type and memory layout of <tt>Attribute::Radius</tt>.
template <> struct AttributeTraits<Attribute::Radius>
{
	/// Type of one array element, buffer view element or constant.
	using Value = float;

	/// Whether the attribute can be supplied as a constant.
	static constexpr bool constantSupported = true;

	/// Byte offset of the single component within a <tt>Value</tt>.
	static constexpr std::array<std::size_t, 1> componentOffsets{0};
};

/// Value type and memory layout of <tt>Attribute::Extent</tt>.
template <> struct AttributeTraits<Attribute::Extent>
{
	/// Type of one array element, buffer view element or constant.
	using Value = glm::vec3;

	/// Whether the attribute can be supplied as a constant.
	static constexpr bool constantSupported = true;

	/// Byte offsets of the x, y and z components within a <tt>Value</tt>.
	static constexpr std::array<std::size_t, 3> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z)
	};
};

/// Value type and memory layout of <tt>Attribute::Orientation</tt>.
template <> struct AttributeTraits<Attribute::Orientation>
{
	/// Type of one array element, buffer view element or constant.
	using Value = glm::quat;

	/// Whether the attribute can be supplied as a constant.
	static constexpr bool constantSupported = true;

	/// Byte offsets of the quaternion components x, y, z and w within a <tt>Value</tt>, where w is the real part.
	static constexpr std::array<std::size_t, 4> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z), offsetof(Value, w)
	};
};

/// Value type and memory layout of <tt>Attribute::Color</tt>.
template <> struct AttributeTraits<Attribute::Color>
{
	/// Type of one array element, buffer view element or constant.
	using Value = glm::vec4;

	/// Whether the attribute can be supplied as a constant.
	static constexpr bool constantSupported = true;

	/// Byte offsets of the red, green, blue and alpha components within a <tt>Value</tt>.
	static constexpr std::array<std::size_t, 4> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z), offsetof(Value, w)
	};
};

/// \brief
/// 	Describes an array of attribute values in a GPU buffer that you manage, for use with
/// 	<tt>PrimitiveAttributes::Update::bind</tt>.
///
/// Element \c i starts at byte <tt>offset + i * stride</tt> of \c buffer and must have the memory layout of
/// <tt>AttributeTraits<A>::Value</tt>, as described by <tt>AttributeTraits<A>::componentOffsets</tt>. A stride larger
/// than the value size lets you bind one attribute of an array of interleaved records.
///
/// The buffer must have been created on the device of the collection with \c SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ
/// usage. \c offset and \c stride must be multiples of 4 bytes, \c stride must be at least the size of the value type,
/// and all \c count elements must lie within the buffer. The values are read on the GPU without any checks, so they
/// must meet the requirements of the respective \c Attribute themselves.
///
/// \tparam A The attribute.
template <Attribute A> struct AttributeBufferView
{
	/// The buffer containing the values. Not owned.
	const Buffer *buffer = nullptr;

	/// Byte offset of the first element within the buffer.
	std::size_t offset = 0;

	/// Byte distance between the starts of consecutive elements. The default is the size of the value type.
	std::size_t stride = sizeof(typename AttributeTraits<A>::Value);

	/// Number of elements. Zero means an empty array.
	std::size_t count = 0;
};

/// \brief
/// 	The source of an attribute, as returned by <tt>PrimitiveAttributes::source</tt>.
///
/// Holds \c std::monostate if the attribute is absent, the value if it is a constant, or an \c AttributeBufferView if
/// it is an array.
///
/// \tparam A The attribute.
template <Attribute A>
using AttributeSource = std::variant<std::monostate, typename AttributeTraits<A>::Value, AttributeBufferView<A>>;



//////
//
// Classes
//

/// \brief
/// 	A collection of per-instance attributes in GPU memory that any number of primitive renderers can draw.
///
/// Each \c Attribute is either absent, a constant shared by all instances, an array copied from CPU memory, or an
/// array in a GPU buffer that you manage. The number of positions determines the number of instances. Change the
/// sources with <tt>setAttributes</tt>, then pass the collection to <tt>PrimitiveRenderer::draw</tt>:
/// \code
/// fcg::PrimitiveAttributes attributes(device);
/// auto result = attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
/// 	update.set<fcg::Attribute::Position>(std::span(positions));
/// 	update.set<fcg::Attribute::Color>(glm::vec4(1.f, 0.f, 0.f, 1.f));
/// });
/// if (result)
/// 	result = boxes.draw(attributes, state, commandBuffer, renderPass);
/// \endcode
///
/// The collection keeps no CPU copy of uploaded arrays, and updates never wait for the GPU. If an update fails, the
/// previous sources stay in effect; see \c setAttributes for the consequences of a failed GPU submission.
///
/// The collection is not thread-safe: calls on it, including draws that use it, must not run concurrently. The device
/// must outlive the collection.
class FCG_RENDER_EXPORT PrimitiveAttributes
{
	////
	// Types

	/// Kind of a committed source or of a pending change.
	enum class Kind
	{
		/// Pending only: keep the committed source.
		Unchanged,

		/// The attribute has no source.
		Absent,

		/// The source is the constant stored in <tt>Descriptor::constant</tt>.
		Constant,

		/// Pending only: an array in CPU memory to upload. Committed arrays are stored as \c View of the
		/// collection-owned buffer.
		Array,

		/// A typed view into a GPU buffer.
		View
	};

	/// Type-erased description of a committed source or a pending change. Only pending arrays reference CPU memory.
	struct Descriptor
	{
		/// Kind of source or pending change.
		Kind kind = Kind::Unchanged;

		/// CPU bytes of a pending array, referenced only during an update.
		std::span<const std::byte> bytes;

		/// Constant components in shader component order.
		glm::vec4 constant{};

		/// Buffer of a view; not owned. For committed arrays, points to the collection-owned buffer.
		const Buffer *buffer = nullptr;

		/// Byte offset of the first element.
		std::size_t offset = 0;

		/// Byte distance between consecutive elements.
		std::size_t stride = 0;

		/// Number of elements.
		std::size_t count = 0;

		/// Set if the byte size of a pending array overflows <tt>std::size_t</tt>; \c bytes is left empty in that case.
		bool overflow = false;
	};

	/// Implementation state: collection-owned GPU buffers, committed sources and upload staging.
	struct State;


public:

	////
	// Types

	/// \brief
	/// 	Records changes to the attribute sources of a \c PrimitiveAttributes collection.
	///
	/// You receive an \c Update in the callback passed to <tt>PrimitiveAttributes::setAttributes</tt> and cannot
	/// create one yourself. Each call replaces the pending source of one attribute; if an attribute is assigned
	/// several times, the last assignment counts. Attributes that are not assigned keep their current source. The
	/// changes take effect when \c setAttributes succeeds. Do not keep a reference or pointer to the \c Update after
	/// the callback returns.
	class Update
	{
		////
		// Friend declarations

		/// \c PrimitiveAttributes creates the \c Update for the duration of an update and applies the recorded changes.
		friend class PrimitiveAttributes;


	public:

		////
		// Object construction/destruction

		/// Not copyable.
		Update(const Update&) = delete;

		/// Not copy-assignable.
		auto operator= (const Update&) -> Update& = delete;


		////
		// Methods

		/// \brief
		/// 	Sets the attribute to an array of per-instance values in CPU memory.
		///
		/// The values are copied to GPU memory when the enclosing \c setAttributes call completes. Values of
		/// <tt>Attribute::Position</tt>, \c Attribute::Extent and \c Attribute::Orientation are checked against the
		/// requirements of the respective attribute at that point.
		///
		/// \pre
		/// 	\p values must remain valid and unchanged until the enclosing \c setAttributes call returns.
		///
		/// \tparam A
		/// 	The attribute to set.
		/// \tparam T
		/// 	The element type. Must be <tt>AttributeTraits<A>::Value</tt>, optionally \c const qualified.
		/// \tparam N
		/// 	The extent of the span.
		///
		/// \param values
		/// 	The values, one per instance.
		template <Attribute A, class T, std::size_t N>
			requires std::same_as<std::remove_cv_t<T>, typename AttributeTraits<A>::Value>
		void set (std::span<T, N> values)
		{
			auto &pending = descriptors[(std::size_t)A];
			pending = {};
			pending.kind = Kind::Array;
			pending.count = values.size();
			pending.stride = sizeof(T);
			pending.overflow = values.size() > std::numeric_limits<std::size_t>::max() / sizeof(T);
			if (!pending.overflow)
				pending.bytes = std::as_bytes(values);
		}

		/// \brief
		/// 	Sets the attribute to a constant shared by all instances.
		///
		/// Not available for <tt>Attribute::Position</tt>. Constants of \c Attribute::Extent and
		/// \c Attribute::Orientation are checked against the requirements of the respective attribute when the
		/// enclosing \c setAttributes call completes.
		///
		/// \tparam A
		/// 	The attribute to set.
		///
		/// \param value
		/// 	The constant. It is copied.
		template <Attribute A> requires AttributeTraits<A>::constantSupported
		void set (const typename AttributeTraits<A>::Value &value)
		{
			auto &pending = descriptors[(std::size_t)A];
			pending = {};
			pending.kind = Kind::Constant;
			const auto *bytes = (const std::byte*)&value;
			for (std::size_t i = 0; i < AttributeTraits<A>::componentOffsets.size(); ++i)
				std::memcpy(&pending.constant[(int)i], bytes + AttributeTraits<A>::componentOffsets[i], sizeof(float));
		}

		/// \brief
		/// 	Sets the attribute to an array in a GPU buffer that you manage, without copying it.
		///
		/// The view must meet the requirements described for <tt>AttributeBufferView</tt>; this is checked when the
		/// enclosing \c setAttributes call completes. Every subsequent draw reads the current buffer contents, which
		/// are not checked.
		///
		/// \pre
		/// 	The \c Buffer referenced by \p view must not be moved or destroyed until the attribute is assigned
		/// 	another source or the collection is destroyed.
		///
		/// \tparam A
		/// 	The attribute to set.
		///
		/// \param view
		/// 	The buffer and the layout of the values within it.
		template <Attribute A> void bind (AttributeBufferView<A> view) {
			descriptors[(std::size_t)A] = {
				.kind = Kind::View, .buffer = view.buffer, .offset = view.offset,
				.stride = view.stride, .count = view.count
			};
		}

		/// \brief
		/// 	Removes the source of the attribute. Renderers then use the default value described for the respective
		/// 	<tt>Attribute</tt>.
		///
		/// \tparam A
		/// 	The attribute to remove.
		template <Attribute A> void clear () {
			descriptors[(std::size_t)A] = {.kind = Kind::Absent};
		}


	private:

		////
		// Object construction/destruction

		/// Constructs an empty change set; only \c PrimitiveAttributes creates updates.
		Update() = default;


		////
		// Fields

		/// Last assignment per attribute; attributes left at \c Kind::Unchanged keep their committed source.
		std::array<Descriptor, (std::size_t)Attribute::Count> descriptors{};
	};


	////
	// Object construction/destruction

	/// \brief
	/// 	Creates an empty collection: all attributes are absent and the instance count is zero.
	///
	/// \param device
	/// 	The device on which GPU memory is allocated. It must outlive the collection.
	explicit PrimitiveAttributes(Device &device);

	/// Destroys the collection and its GPU memory without waiting for the GPU. Draws that were already submitted still
	/// complete.
	~PrimitiveAttributes();

	/// Not copyable.
	PrimitiveAttributes(const PrimitiveAttributes&) = delete;

	/// Not copy-assignable.
	auto operator= (const PrimitiveAttributes&) -> PrimitiveAttributes& = delete;

	/// Not movable.
	PrimitiveAttributes(PrimitiveAttributes&&) = delete;

	/// Not move-assignable.
	auto operator= (PrimitiveAttributes&&) -> PrimitiveAttributes& = delete;


	////
	// Accessors

	/// Returns the device the collection was created on.
	[[nodiscard]] auto device () const -> Device&;

	/// Returns the number of instances: the element count of the position source, or zero if position is absent.
	[[nodiscard]] auto instanceCount () const -> std::size_t;

	/// \brief
	/// 	Returns the current source of an attribute.
	///
	/// A constant is returned as its value. Both arrays set with \c Update::set and buffers bound with \c Update::bind
	/// are returned as an <tt>AttributeBufferView</tt>. For an array set with <tt>Update::set</tt>, the view refers to
	/// a buffer owned by the collection; for an empty array, its \c buffer may be null. Such a view stays valid until
	/// the next successful \c setAttributes call that changes the attribute, or until the collection is destroyed.
	///
	/// \tparam A
	/// 	The attribute to query.
	///
	/// \return The source of the attribute.
	template <Attribute A> [[nodiscard]] auto source () const -> AttributeSource<A>
	{
		const auto &data = descriptor(A);
		if (data.kind == Kind::View)
			return AttributeBufferView<A>{data.buffer, data.offset, data.stride, data.count};
		if (data.kind != Kind::Constant)
			return std::monostate{};
		typename AttributeTraits<A>::Value value{};
		auto *bytes = (std::byte*)&value;
		for (std::size_t i = 0; i < AttributeTraits<A>::componentOffsets.size(); ++i)
			std::memcpy(bytes + AttributeTraits<A>::componentOffsets[i], &data.constant[(int)i], sizeof(float));
		return value;
	}

	/// \brief
	/// 	Returns whether renderers can draw the current source of an attribute.
	///
	/// An attribute becomes unusable when \c setAttributes without a copy pass fails to submit an array assigned to
	/// it. The GPU contents of the attribute are undefined then, and drawing with a renderer that uses the attribute
	/// fails with <tt>RenderErrorCode::InvalidState</tt>. The next successful assignment of any source to the
	/// attribute, including <tt>Update::clear</tt>, makes it usable again.
	///
	/// \tparam A
	/// 	The attribute to query.
	///
	/// \return Whether the attribute can be drawn.
	template <Attribute A> [[nodiscard]] auto usable () const -> bool { return isUsable(A); }


	////
	// Methods

	/// \brief
	/// 	Changes attribute sources and uploads new arrays to the GPU in a single submission.
	///
	/// Calls \p fn once with an \c Update on which it records the changes. Then all changes are checked, and the
	/// arrays assigned with \c Update::set are uploaded and submitted to the GPU in one command buffer. If no arrays
	/// were assigned, nothing is submitted. The call does not wait for the GPU. Draws recorded after it returns use the
	/// new sources.
	///
	/// If checking the changes or allocating GPU memory fails, the call returns an error and all sources stay
	/// unchanged. If the submission fails, the call returns <tt>RenderErrorCode::SDLFailure</tt>, all sources stay
	/// unchanged, and the attributes that were assigned arrays become unusable (see <tt>usable</tt>) until they are
	/// assigned again. If \p fn throws, the exception propagates, all sources stay unchanged and nothing is uploaded.
	///
	/// \tparam Fn
	/// 	A callable that accepts a <tt>PrimitiveAttributes::Update&</tt>.
	///
	/// \param fn
	/// 	Records the changes. CPU arrays passed to \c Update::set must remain valid and unchanged until this call
	/// 	returns.
	///
	/// \return
	/// 	Success, or an error. The error is <tt>RenderErrorCode::InvalidState</tt> if this is called from within the
	/// 	callback of another \c setAttributes call on the same collection, or if the device has been moved from. It
	/// 	is <tt>RenderErrorCode::InvalidArgument</tt> if a value, constant or buffer view does not meet its
	/// 	requirements, and <tt>RenderErrorCode::SDLFailure</tt> if allocating, uploading or submitting failed.
	template <class Fn> [[nodiscard]] auto setAttributes (Fn &&fn) -> std::expected<void, RenderError> {
		return performUpdate(nullptr, std::forward<Fn>(fn));
	}

	/// \brief
	/// 	Changes attribute sources and records the uploads of new arrays into a copy pass that you provide.
	///
	/// Works like the overload without a copy pass, but records the uploads into \p copyPass instead of submitting
	/// them, and never makes attributes unusable. On success, the new sources are in effect immediately. Draws that use
	/// the new arrays must be recorded after \p copyPass has ended, either in the same command buffer or in one that
	/// is submitted later. The uploads reach the GPU only when the command buffer containing \p copyPass is submitted.
	/// The call does not wait for the GPU.
	///
	/// \pre
	/// 	\p copyPass is active and belongs to a command buffer on the device of the collection.
	///
	/// \note
	/// 	If that command buffer is canceled or its submission fails, or if this call fails after recording some
	/// 	uploads, the GPU contents of the affected arrays are undefined. Assign these arrays again before drawing.
	///
	/// \tparam Fn
	/// 	A callable that accepts a <tt>PrimitiveAttributes::Update&</tt>.
	///
	/// \param copyPass
	/// 	The active copy pass to record the uploads into. Must not be null.
	/// \param fn
	/// 	Records the changes. CPU arrays passed to \c Update::set must remain valid and unchanged until this call
	/// 	returns.
	///
	/// \return
	/// 	Success once the uploads are recorded, or an error. The errors are those of the overload without a copy
	/// 	pass, plus <tt>RenderErrorCode::InvalidArgument</tt> if \p copyPass is null.
	template <class Fn> [[nodiscard]] auto setAttributes (SDL_GPUCopyPass *copyPass, Fn &&fn)
		-> std::expected<void, RenderError>
	{
		if (!copyPass)
			return std::unexpected(RenderError{RenderErrorCode::InvalidArgument, "Attributes require a copy pass"});
		return performUpdate(copyPass, std::forward<Fn>(fn));
	}


private:

	////
	// Methods

	/// Runs one update: rejects recursion, invokes the callback, applies the changes, and discards pending uploads on
	/// every exit path, including exceptions.
	template <class Fn> auto performUpdate (SDL_GPUCopyPass *pass, Fn &&fn) -> std::expected<void, RenderError>
	{
		if (auto result = beginUpdate(); !result)
			return result;
		try {
			Update update;
			std::invoke(std::forward<Fn>(fn), update);
			auto result = finishUpdate(pass, update);
			cancelUpdate();
			return result;
		} catch (...) {
			cancelUpdate();
			throw;
		}
	}

	/// Rejects moved-from devices and recursive updates before the callback runs, then marks an update as running.
	[[nodiscard]] auto beginUpdate () -> std::expected<void, RenderError>;

	/// Validates the recorded changes, allocates and uploads arrays, records or submits the uploads, and commits the
	/// new sources. With a null \p pass, the uploads are submitted in a command buffer owned by this call.
	[[nodiscard]] auto finishUpdate (SDL_GPUCopyPass *pass, const Update &update) -> std::expected<void, RenderError>;

	/// Discards pending uploads and marks the running update as finished.
	void cancelUpdate () noexcept;

	/// Returns the committed source of an attribute.
	[[nodiscard]] auto descriptor (Attribute attribute) const -> const Descriptor&;

	/// Returns whether the committed source of an attribute can be drawn; see <tt>usable</tt>.
	[[nodiscard]] auto isUsable (Attribute attribute) const -> bool;


	////
	// Fields

	/// Implementation state; never null. Keeps no copies of uploaded CPU arrays.
	std::unique_ptr<State> state;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_PRIMITIVE_ATTRIBUTES_H__
