
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

/// \defgroup fcg_primitives Instanced primitives
/// \ingroup fcg_components
/// \brief Shared attributes and filled quad and box renderers; see \ref fcg_render_guide.

/// \addtogroup fcg_primitives
/// @{



//////
//
// Structs and enums
//

/// Fixed logical attributes; renderer support is independent of storage in a collection.
enum class Attribute
{
	/// Finite homogeneous point with nonzero W; an array determines the instance count.
	Position,

	/// User-supplied normal; unused by the initial renderers.
	Normal,

	/// User-supplied radius; unused by the initial renderers.
	Radius,

	/// Finite, nonnegative local half sizes.
	Extent,

	/// Finite, nonzero quaternion normalized by the renderer.
	Orientation,

	/// RGBA instance color.
	Color,

	/// Number of logical attributes; not an attribute itself.
	Count
};

/// Value types and component layouts for fixed logical attributes.
///
/// \tparam A The logical attribute.
template <Attribute A> struct AttributeTraits;

/// Typed object representation for \c Attribute::Position.
template <> struct AttributeTraits<Attribute::Position>
{
	////
	// Types

	/// Array element and constant value type.
	using Value = glm::vec4;


	////
	// Fields

	/// Whether a constant source is permitted.
	static constexpr bool constantSupported = false;

	/// Byte offsets of shader components, in XYZ or XYZW order, including quaternion storage ordering.
	static constexpr std::array<std::size_t, 4> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z), offsetof(Value, w)
	};
};

/// Typed object representation for \c Attribute::Normal.
template <> struct AttributeTraits<Attribute::Normal>
{
	////
	// Types

	/// Array element and constant value type.
	using Value = glm::vec3;


	////
	// Fields

	/// Whether a constant source is permitted.
	static constexpr bool constantSupported = true;

	/// Byte offsets of shader components, in XYZ or XYZW order, including quaternion storage ordering.
	static constexpr std::array<std::size_t, 3> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z)
	};
};

/// Typed object representation for \c Attribute::Radius.
template <> struct AttributeTraits<Attribute::Radius>
{
	////
	// Types

	/// Array element and constant value type.
	using Value = float;


	////
	// Fields

	/// Whether a constant source is permitted.
	static constexpr bool constantSupported = true;

	/// Byte offsets of shader components, in XYZ or XYZW order, including quaternion storage ordering.
	static constexpr std::array<std::size_t, 1> componentOffsets{0};
};

/// Typed object representation for \c Attribute::Extent.
template <> struct AttributeTraits<Attribute::Extent>
{
	////
	// Types

	/// Array element and constant value type.
	using Value = glm::vec3;


	////
	// Fields

	/// Whether a constant source is permitted.
	static constexpr bool constantSupported = true;

	/// Byte offsets of shader components, in XYZ or XYZW order, including quaternion storage ordering.
	static constexpr std::array<std::size_t, 3> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z)
	};
};

/// Typed object representation for \c Attribute::Orientation.
template <> struct AttributeTraits<Attribute::Orientation>
{
	////
	// Types

	/// Array element and constant value type.
	using Value = glm::quat;


	////
	// Fields

	/// Whether a constant source is permitted.
	static constexpr bool constantSupported = true;

	/// Byte offsets of shader components, in XYZ or XYZW order, including quaternion storage ordering.
	static constexpr std::array<std::size_t, 4> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z), offsetof(Value, w)
	};
};

/// Typed object representation for \c Attribute::Color.
template <> struct AttributeTraits<Attribute::Color>
{
	////
	// Types

	/// Array element and constant value type.
	using Value = glm::vec4;


	////
	// Fields

	/// Whether a constant source is permitted.
	static constexpr bool constantSupported = true;

	/// Byte offsets of shader components, in XYZ or XYZW order, including quaternion storage ordering.
	static constexpr std::array<std::size_t, 4> componentOffsets{
		offsetof(Value, x), offsetof(Value, y), offsetof(Value, z), offsetof(Value, w)
	};
};

/// Borrowed array of typed values in a graphics storage buffer.
///
/// Owners must remain stable and alive while used. Offsets and strides are multiples of four bytes.
/// Values follow \c AttributeTraits component offsets, allowing interleaved records and padded GLM types.
/// GPU-supplied transforms must satisfy the corresponding \c Attribute value contracts.
///
/// \tparam A The logical attribute.
template <Attribute A> struct AttributeBufferView
{
	////
	// Fields

	/// Borrowed buffer on the collection's device with graphics storage read usage.
	const Buffer *buffer = nullptr;

	/// Byte offset of the first typed element.
	std::size_t offset = 0;

	/// Byte distance between typed elements, at least the value type's size.
	std::size_t stride = sizeof(typename AttributeTraits<A>::Value);

	/// Number of accessible elements; zero represents an empty array.
	std::size_t count = 0;
};

/// Read-only source: absence, a constant, or a borrowed GPU view.
///
/// \tparam A The logical attribute.
template <Attribute A>
using AttributeSource = std::variant<std::monostate, typename AttributeTraits<A>::Value, AttributeBufferView<A>>;



//////
//
// Classes
//

/// Shared GPU attributes with retained allocations and no retained CPU arrays.
///
/// Access is externally serialized. Devices outlive resources. Updates never wait for GPU idle.
/// All referenced CPU arrays must remain valid and unchanged until \c setAttributes returns.
/// Validation, allocation, staging failure, and callback exceptions preserve visible metadata.
/// After automatic submission failure changed arrays are unusable until replaced successfully.
class FCG_RENDER_EXPORT PrimitiveAttributes
{
	////
	// Types

	/// Pending operation or committed source kind.
	enum class Kind
	{
		/// Retain the previously committed source.
		Unchanged,

		/// Remove the source.
		Absent,

		/// Use the descriptor's constant components.
		Constant,

		/// Stage the borrowed host bytes.
		Array,

		/// Borrow a typed GPU view.
		View
	};

	/// Erased source description; only pending arrays borrow host bytes.
	struct Descriptor
	{
		/// Source or pending operation kind.
		Kind kind = Kind::Unchanged;

		/// Array bytes, borrowed only during an update.
		std::span<const std::byte> bytes;

		/// Constant components in shader order.
		glm::vec4 constant{};

		/// Borrowed GPU owner for a view.
		const Buffer *buffer = nullptr;

		/// Byte offset of the first element.
		std::size_t offset = 0;

		/// Byte stride between elements.
		std::size_t stride = 0;

		/// Logical element count.
		std::size_t count = 0;

		/// Invalid host byte count detected before constructing a byte span.
		bool overflow = false;
	};

	/// Private GPU owners and upload staging.
	struct State;


public:

	////
	// Types

	/// Scoped mutation proxy; cannot be copied, moved, or constructed outside an update.
	///
	/// Do not retain a reference or pointer to this proxy after its callback returns.
	class Update
	{
		////
		// Friend declarations

		/// The collection controls proxy lifetime and commits its descriptors.
		friend class PrimitiveAttributes;


	public:

		////
		// Object construction/destruction

		/// Proxies cannot be copied.
		Update(const Update&) = delete;

		/// Proxies cannot be assigned.
		auto operator= (const Update&) -> Update& = delete;


		////
		// Methods

		/// Replace an attribute with a full array, copying directly into staging after the callback.
		///
		/// \tparam A Logical attribute. \tparam T Matching value type. \tparam N Span extent.
		///
		/// \param values Borrowed values that remain valid and unchanged until the enclosing update returns.
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

		/// Supply a constant; this overload is unavailable for position.
		///
		/// \tparam A Attribute supporting constants.
		///
		/// \param value The constant copied into the pending descriptor.
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

		/// Borrow an existing typed GPU array without uploading.
		///
		/// \tparam A The logical attribute.
		///
		/// \param view Buffer and byte layout borrowed while this source remains assigned.
		template <Attribute A> void bind (AttributeBufferView<A> view) {
			descriptors[(std::size_t)A] = {
				.kind = Kind::View, .buffer = view.buffer, .offset = view.offset,
				.stride = view.stride, .count = view.count
			};
		}

		/// Remove a source, retaining any owned GPU capacity for later reuse.
		///
		/// \tparam A The logical attribute.
		template <Attribute A> void clear () {
			descriptors[(std::size_t)A] = {.kind = Kind::Absent};
		}


	private:

		////
		// Object construction/destruction

		/// Construct only for an enclosing collection update.
		Update() = default;


		////
		// Fields

		/// Last assignment per attribute; omitted attributes retain their sources.
		std::array<Descriptor, (std::size_t)Attribute::Count> descriptors{};
	};


	////
	// Object construction/destruction

	/// Borrow a device; initially allocates no GPU storage.
	explicit PrimitiveAttributes(Device &device);

	/// Release GPU owners without waiting for idle.
	~PrimitiveAttributes();

	/// Collections cannot be copied.
	PrimitiveAttributes(const PrimitiveAttributes&) = delete;

	/// Collections cannot be assigned.
	auto operator= (const PrimitiveAttributes&) -> PrimitiveAttributes& = delete;

	/// Collections cannot be moved; borrowed source owners remain stable.
	PrimitiveAttributes(PrimitiveAttributes&&) = delete;

	/// Collections cannot be move-assigned.
	auto operator= (PrimitiveAttributes&&) -> PrimitiveAttributes& = delete;


	////
	// Accessors

	/// Borrow the device supplied at construction.
	[[nodiscard]] auto device () const -> Device&;

	/// Instance count from position; absent or empty position means zero instances.
	[[nodiscard]] auto instanceCount () const -> std::size_t;

	/// Query a typed source; views into owned arrays expire on capacity growth or collection destruction.
	///
	/// \tparam A The logical attribute.
	///
	/// \return Absence, constant, or typed buffer view, including its actual stride.
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

	/// Whether the source can be drawn; failed automatic submission invalidates changed arrays.
	///
	/// \tparam A The logical attribute.
	///
	/// \return Usability restored by a successful replacement or clearing.
	template <Attribute A> [[nodiscard]] auto usable () const -> bool { return isUsable(A); }


	////
	// Methods

	/// Collect final assignments and submit changed arrays in one owned command buffer.
	///
	/// Callback exceptions propagate after cleanup, with no uploads recorded. Recursive updates are rejected.
	///
	/// \tparam Fn Callback invocable with an \c Update reference.
	///
	/// \param fn Callback recording assignments; array storage remains valid until this call returns.
	///
	/// \return Success after submission, or a recoverable error.
	template <class Fn> [[nodiscard]] auto setAttributes (Fn &&fn) -> std::expected<void, RenderError> {
		return performUpdate(nullptr, std::forward<Fn>(fn));
	}

	/// Record all changed arrays in a supplied copy pass without submitting or waiting.
	///
	/// \pre The pass is active on this device. End it before drawing and submit its command buffer.
	///
	/// \note Resupply changed arrays after cancellation or failed submission; recorded writes cannot be rolled back.
	///
	/// \tparam Fn Callback invocable with an \c Update reference.
	///
	/// \param copyPass Active copy pass. \param fn Callback recording final assignments.
	///
	/// \return Recording success or a recoverable error; success does not imply submission.
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

	/// Guard recursion, invoke the callback, and always discard pending staging on exit.
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

	/// Reject recursive access before the callback runs.
	[[nodiscard]] auto beginUpdate () -> std::expected<void, RenderError>;

	/// Validate, allocate, stage, record, and commit final descriptors.
	[[nodiscard]] auto finishUpdate (SDL_GPUCopyPass *pass, const Update &update) -> std::expected<void, RenderError>;

	/// Clear pending staging and the recursion guard.
	void cancelUpdate () noexcept;

	/// Read an erased committed descriptor.
	[[nodiscard]] auto descriptor (Attribute attribute) const -> const Descriptor&;

	/// Read submission usability.
	[[nodiscard]] auto isUsable (Attribute attribute) const -> bool;


	////
	// Fields

	/// Private storage and staging; no CPU array mirrors.
	std::unique_ptr<State> state;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_PRIMITIVE_ATTRIBUTES_H__
