
#ifndef __FCG_IMAGE_PIXEL_ACCESS_H__
#define __FCG_IMAGE_PIXEL_ACCESS_H__


//////
//
// Module documentation
//

/// \defgroup fcg_pixel_access Scoped native pixel access
/// \ingroup fcg_modules
/// \brief Edit checked pixels and run row-based kernels with native precision.
///
/// Inspect and explicitly convert with \ref fcg_pixel_formats before selecting an exact
/// <tt>\ref fcg::PixelFormatTraits "PixelFormatTraits"</tt> format. Logical values use RGB or RGBA order,
/// regardless of the physical channel order. Floating samples retain their color encoding and HDR range. Access
/// retains the image's alpha association; use the same straight or associated convention when writing components.
///
/// \section fcg_pixel_access_usage Getting started
/// \snippet pixel_examples.cpp access
///
/// \section fcg_pixel_access_workflows Common tasks
/// Use <tt>\ref fcg::Image::withPixels "Image::withPixels"</tt> for a callback, or keep the movable guard from
/// <tt>\ref fcg::Image::mapPixels "Image::mapPixels"</tt> in a local variable. Iterate rows to skip padding.
/// Rows provide checked reads and writes and spans of physical bytes, including unused components.
/// Unchecked operations require valid coordinates and an active guard.
/// <tt>\ref fcg::Image::transformPixels "Image::transformPixels"</tt> applies independent pointwise kernels.
/// For neighborhoods, use <tt>\ref fcg::Image::transformPixelsTo "Image::transformPixelsTo"</tt> with independent
/// output storage; choose border behavior explicitly:
/// \snippet pixel_examples.cpp neighborhood
///
/// \section fcg_pixel_access_lifetime Ownership and lifetime
/// A guard borrows its image: keep that image alive and unmoved. Its views, rows, iterators and spans expire when
/// the guard is destroyed or released. Only one session may access each image, including read-only access.
/// Conflicting pixel operations, conversion and upload return an error. Metadata remains available.
/// Serialize access externally; SDL locking does not provide thread safety. Do not perform conflicting external
/// operations through the surface handle during a session. Callback and kernel exceptions release access;
/// kernels may leave partially written output and provide no rollback.



//////
//
// Includes
//

// C++ STL
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <expected>
#include <iterator>
#include <span>
#include <type_traits>
#include <utility>

// SDL3 library
#include <SDL3/SDL_surface.h>

// GLM library
#include <glm/detail/type_half.hpp>

// Local includes
#include "FCG/Image/pixel_format.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_pixel_access
/// @{



//////
//
// Structs and enums
//

/// Logical RGB components in the native representation; alpha-free formats use this value type.
///
/// \tparam Component Native unsigned integer or floating component type.
template<class Component>
struct NativeRGB
{
	/// Red component, in the source color encoding.
	Component r{};

	/// Green component, in the source color encoding.
	Component g{};

	/// Blue component, in the source color encoding.
	Component b{};
};

/// Logical RGBA components in the native representation, retaining the image's alpha association.
///
/// \tparam Component Native unsigned integer or floating component type.
template<class Component>
struct NativeRGBA
{
	/// Red component, in the source color encoding.
	Component r{};

	/// Green component, in the source color encoding.
	Component g{};

	/// Blue component, in the source color encoding.
	Component b{};

	/// Alpha component.
	Component a{};
};



//////
//
// Classes
//

/// IEEE binary16 value with exact storage bits and explicit conversion to or from binary32.
class Half
{
public:
	////
	// Object construction

	/// Construct positive zero.
	Half() = default;

	/// \brief Round a binary32 value to binary16 using GLM's half conversion.
	///
	/// \param value Floating sample; overflow becomes infinity.
	explicit Half(float value) : m_bits(std::bit_cast<Uint16>(glm::detail::toFloat16(value))) {}

	/// \brief Construct a value retaining every supplied bit, including NaN payloads.
	///
	/// \param bits IEEE binary16 encoding.
	///
	/// \return The exact-bit value.
	[[nodiscard]] static auto fromBits (Uint16 bits) -> Half {
		Half result;
		result.m_bits = bits;
		return result;
	}


	////
	// Accessors

	/// Return the exact IEEE binary16 encoding.
	[[nodiscard]] auto bits () const -> Uint16 { return m_bits; }

	/// Return the value expanded to binary32 without changing its color encoding.
	[[nodiscard]] explicit operator float () const { return glm::detail::toFloat32(std::bit_cast<glm::detail::hdata>(m_bits)); }


private:
	////
	// Fields

	/// Exact binary16 bits.
	Uint16 m_bits = 0;
};

/// Pixel storage helpers; client code uses the public traits and views.
namespace detail {

/// Whether a format has a known interleaved component layout.
[[nodiscard]] constexpr auto nativePixelFormat (SDL_PixelFormat format) -> bool
{
	switch (format) {
	case SDL_PIXELFORMAT_RGB24: case SDL_PIXELFORMAT_BGR24:
	case SDL_PIXELFORMAT_XRGB8888: case SDL_PIXELFORMAT_RGBX8888:
	case SDL_PIXELFORMAT_XBGR8888: case SDL_PIXELFORMAT_BGRX8888:
	case SDL_PIXELFORMAT_ARGB8888: case SDL_PIXELFORMAT_RGBA8888:
	case SDL_PIXELFORMAT_ABGR8888: case SDL_PIXELFORMAT_BGRA8888:
	case SDL_PIXELFORMAT_RGB48: case SDL_PIXELFORMAT_BGR48:
	case SDL_PIXELFORMAT_RGBA64: case SDL_PIXELFORMAT_ARGB64:
	case SDL_PIXELFORMAT_BGRA64: case SDL_PIXELFORMAT_ABGR64:
	case SDL_PIXELFORMAT_RGB48_FLOAT: case SDL_PIXELFORMAT_BGR48_FLOAT:
	case SDL_PIXELFORMAT_RGBA64_FLOAT: case SDL_PIXELFORMAT_ARGB64_FLOAT:
	case SDL_PIXELFORMAT_BGRA64_FLOAT: case SDL_PIXELFORMAT_ABGR64_FLOAT:
	case SDL_PIXELFORMAT_RGB96_FLOAT: case SDL_PIXELFORMAT_BGR96_FLOAT:
	case SDL_PIXELFORMAT_RGBA128_FLOAT: case SDL_PIXELFORMAT_ARGB128_FLOAT:
	case SDL_PIXELFORMAT_BGRA128_FLOAT: case SDL_PIXELFORMAT_ABGR128_FLOAT:
		return true;
	default:
		return false;
	}
}

/// Component indices in memory for logical red, green, blue and alpha; negative means absent.
[[nodiscard]] constexpr auto nativeOffsets (SDL_PixelFormat format) -> std::array<int, 4>
{
	const int order = SDL_PIXELORDER(format);
	if (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED32)
	{
		std::array<int, 4> offsets{};
		switch (order) {
		case SDL_PACKEDORDER_XRGB: offsets = {1, 2, 3, -1}; break;
		case SDL_PACKEDORDER_RGBX: offsets = {0, 1, 2, -1}; break;
		case SDL_PACKEDORDER_ARGB: offsets = {1, 2, 3, 0}; break;
		case SDL_PACKEDORDER_RGBA: offsets = {0, 1, 2, 3}; break;
		case SDL_PACKEDORDER_XBGR: offsets = {3, 2, 1, -1}; break;
		case SDL_PACKEDORDER_BGRX: offsets = {2, 1, 0, -1}; break;
		case SDL_PACKEDORDER_ABGR: offsets = {3, 2, 1, 0}; break;
		default: offsets = {2, 1, 0, 3}; break;
		}
		if constexpr (SDL_BYTEORDER == SDL_LIL_ENDIAN)
			for (auto &offset : offsets)
				if (offset >= 0)
					offset = 3 - offset;
		return offsets;
	}
	switch (order) {
	case SDL_ARRAYORDER_RGB: return {0, 1, 2, -1};
	case SDL_ARRAYORDER_BGR: return {2, 1, 0, -1};
	case SDL_ARRAYORDER_RGBA: return {0, 1, 2, 3};
	case SDL_ARRAYORDER_ARGB: return {1, 2, 3, 0};
	case SDL_ARRAYORDER_BGRA: return {2, 1, 0, 3};
	default: return {3, 2, 1, 0};
	}
}

/// Release an image access session and any required SDL lock.
FCG_IMAGE_EXPORT void releasePixels (SDL_Surface *surface, bool *active) noexcept;

/// Report a coordinate outside a visible pixel range.
[[nodiscard]] inline auto pixelBoundsError () -> std::unexpected<ImageError> {
	return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Pixel coordinate is outside the visible image"});
}

// namespace detail
}

/// Native component type and canonical RGB or RGBA value for an exact SDL format.
///
/// \tparam Format SDL storage identifier, including endian aliases.
template<SDL_PixelFormat Format>
struct PixelFormatTraits
{
	/// Whether direct native access is available; unsupported formats require explicit conversion.
	static constexpr bool supported = detail::nativePixelFormat(Format);

	/// Native sample representation; binary16 uses exact-bit \c Half values.
	using Component = std::conditional_t<SDL_PIXELTYPE(Format) == SDL_PIXELTYPE_ARRAYF32, float,
		std::conditional_t<SDL_PIXELTYPE(Format) == SDL_PIXELTYPE_ARRAYF16, Half,
		std::conditional_t<SDL_PIXELTYPE(Format) == SDL_PIXELTYPE_ARRAYU16, Uint16, Uint8>>>;

	/// Whether logical values contain alpha.
	static constexpr bool hasAlpha = SDL_ISPIXELFORMAT_ALPHA(Format);

	/// Logical value copied to or from storage; unused components are excluded.
	using Value = std::conditional_t<hasAlpha, NativeRGBA<Component>, NativeRGB<Component>>;

	/// Physical pixel size including unused components, in bytes.
	static constexpr std::size_t bytesPerPixel = SDL_BYTESPERPIXEL(Format);

	/// Logical channel positions in physical component order.
	static constexpr auto offsets = detail::nativeOffsets(Format);
};

/// Borrowed visible row with byte-backed native reads and writes; expires with its access guard.
///
/// \tparam Format Exact supported SDL storage format.
/// \tparam Writable Whether byte spans and pixel writes are mutable.
template<SDL_PixelFormat Format, bool Writable>
class PixelRow
{
	static_assert(PixelFormatTraits<Format>::supported);

public:
	////
	// Types

	/// Logical native sample value.
	using Value = typename PixelFormatTraits<Format>::Value;

	/// Mutable bytes only for writable rows.
	using Byte = std::conditional_t<Writable, std::byte, const std::byte>;


	////
	// Object construction

	/// \brief Borrow a visible row from an existing access view.
	///
	/// \pre Storage must remain valid and locked for the entire row lifetime.
	///
	/// \param bytes Start of row storage, possibly unaligned.
	/// \param width Visible pixels; storage must contain at least width times physical pixel size bytes.
	PixelRow(Byte *bytes, int width) : m_bytes(bytes), m_width(width) {}


	////
	// Accessors

	/// Return the visible pixel count.
	[[nodiscard]] auto size () const -> int { return m_width; }

	/// Return physical bytes of visible pixels, excluding trailing row padding; expires with the guard.
	[[nodiscard]] auto bytes () const -> std::span<Byte> {
		return {m_bytes, std::size_t(m_width) * PixelFormatTraits<Format>::bytesPerPixel};
	}

	/// \brief Read an exact native value without checking bounds.
	///
	/// \pre The guard must be active and x must be within the visible row.
	///
	/// \param x Zero-based horizontal coordinate.
	///
	/// \return An independent native value, preserving half bits and unused storage.
	[[nodiscard]] auto readUnchecked (int x) const -> Value
	{
		using Traits = PixelFormatTraits<Format>;
		using Component = typename Traits::Component;
		const auto *pixel = m_bytes + std::size_t(x) * Traits::bytesPerPixel;
		const auto load = [pixel](int index) -> Component {
			Component value;
			std::memcpy(&value, pixel + std::size_t(index) * sizeof(Component), sizeof(Component));
			return value;
		};
		Value result{load(Traits::offsets[0]), load(Traits::offsets[1]), load(Traits::offsets[2])};
		if constexpr (Traits::hasAlpha)
			result.a = load(Traits::offsets[3]);
		return result;
	}

	/// \brief Read a native value with visible-row bounds checking.
	///
	/// \param x Zero-based horizontal coordinate.
	///
	/// \return The value, or \c ImageErrorCode::InvalidArgument for an invalid coordinate.
	[[nodiscard]] auto read (int x) const -> std::expected<Value, ImageError> {
		if (x < 0 || x >= m_width)
			return detail::pixelBoundsError();
		return readUnchecked(x);
	}


	////
	// Methods

	/// \brief Store logical native channels, retaining unused storage components.
	///
	/// \pre The guard must be active and x must be within the visible row.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param value Exact native components; no normalization or color transform is performed.
	void writeUnchecked (int x, Value value) const requires Writable
	{
		using Traits = PixelFormatTraits<Format>;
		using Component = typename Traits::Component;
		auto *pixel = m_bytes + std::size_t(x) * Traits::bytesPerPixel;
		const auto store = [pixel](int index, Component component) {
			std::memcpy(pixel + std::size_t(index) * sizeof(Component), &component, sizeof(Component));
		};
		store(Traits::offsets[0], value.r);
		store(Traits::offsets[1], value.g);
		store(Traits::offsets[2], value.b);
		if constexpr (Traits::hasAlpha)
			store(Traits::offsets[3], value.a);
	}

	/// \brief Store a native value with visible-row bounds checking.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param value Native logical channels; unused storage is retained.
	///
	/// \return Nothing on success, or \c ImageErrorCode::InvalidArgument for an invalid coordinate.
	[[nodiscard]] auto write (int x, Value value) const -> std::expected<void, ImageError> requires Writable {
		if (x < 0 || x >= m_width)
			return detail::pixelBoundsError();
		writeUnchecked(x, value);
		return {};
	}


private:
	////
	// Fields

	/// Borrowed row storage, with no alignment requirement.
	Byte *m_bytes;

	/// Visible row width.
	int m_width;
};

/// Borrowed two-dimensional native pixel view; all derived rows and spans expire with the guard.
///
/// \tparam Format Exact supported SDL format.
/// \tparam Writable Whether logical writes and mutable byte spans are available.
template<SDL_PixelFormat Format, bool Writable>
class PixelView
{
	static_assert(PixelFormatTraits<Format>::supported);

public:
	////
	// Types

	/// Native logical sample value.
	using Value = typename PixelFormatTraits<Format>::Value;

	/// Visible borrowed row.
	using Row = PixelRow<Format, Writable>;

	/// Storage byte type according to access permissions.
	using Byte = typename Row::Byte;

	/// Forward iterator over visible rows, yielding row values.
	class Iterator
	{
	public:
		////
		// Types

		/// Iterator distance type.
		using difference_type = std::ptrdiff_t;

		/// Row value type.
		using value_type = Row;

		/// Standard iterator category.
		using iterator_category = std::forward_iterator_tag;


		////
		// Object construction

		/// Construct an empty iterator.
		Iterator() = default;

		/// \brief Construct an iterator into a view.
		///
		/// \param view Borrowed view, whose storage must remain valid.
		/// \param y Row index, or height for the end iterator.
		Iterator(PixelView view, int y) : m_bytes(view.m_bytes), m_width(view.m_width), m_pitch(view.m_pitch), m_y(y) {}


		////
		// Methods

		/// Return the current visible row; dereferencing the end iterator is invalid.
		[[nodiscard]] auto operator* () const -> Row { return {m_bytes + std::size_t(m_y) * m_pitch, m_width}; }

		/// Advance to the next row; returns this iterator.
		auto operator++ () -> Iterator& { ++m_y; return *this; }

		/// Advance to the next row; returns the previous iterator.
		auto operator++ (int) -> Iterator { auto previous = *this; ++*this; return previous; }

		/// Compare position and storage identity.
		///
		/// \param other Iterator to compare.
		///
		/// \return Whether both refer to the same row.
		[[nodiscard]] auto operator== (const Iterator &other) const -> bool = default;


	private:
		////
		// Fields

		/// Borrowed image storage.
		Byte *m_bytes = nullptr;

		/// Visible width.
		int m_width = 0;

		/// Byte stride.
		std::size_t m_pitch = 0;

		/// Current row position.
		int m_y = 0;
	};


	////
	// Object construction

	/// \brief Borrow validated storage from an access session.
	///
	/// \pre Storage must remain locked and valid, with positive dimensions and sufficient pitch and capacity.
	///
	/// \param bytes Image storage start.
	/// \param width Visible width in pixels.
	/// \param height Visible row count.
	/// \param pitch Byte stride between rows.
	PixelView(Byte *bytes, int width, int height, std::size_t pitch)
		: m_bytes(bytes), m_width(width), m_height(height), m_pitch(pitch) {}


	////
	// Accessors

	/// Return the visible width in pixels.
	[[nodiscard]] auto width () const -> int { return m_width; }

	/// Return the visible row count.
	[[nodiscard]] auto height () const -> int { return m_height; }

	/// Return the byte stride between rows.
	[[nodiscard]] auto pitch () const -> std::size_t { return m_pitch; }

	/// \brief Return an unchecked visible row.
	///
	/// \pre The guard must be active and y must lie within the view.
	///
	/// \param y Zero-based vertical coordinate.
	///
	/// \return A borrowed row excluding padding.
	[[nodiscard]] auto rowUnchecked (int y) const -> Row { return {m_bytes + std::size_t(y) * m_pitch, m_width}; }

	/// \brief Look up a visible row with bounds checking.
	///
	/// \param y Zero-based vertical coordinate.
	///
	/// \return A borrowed row, or \c ImageErrorCode::InvalidArgument for an invalid coordinate.
	[[nodiscard]] auto row (int y) const -> std::expected<Row, ImageError> {
		if (y < 0 || y >= m_height)
			return detail::pixelBoundsError();
		return rowUnchecked(y);
	}

	/// Return the first row iterator; expires with the guard.
	[[nodiscard]] auto begin () const -> Iterator { return {*this, 0}; }

	/// Return the past-the-end row iterator; expires with the guard.
	[[nodiscard]] auto end () const -> Iterator { return {*this, m_height}; }

	/// \brief Read an independent native pixel value with bounds checking.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate.
	///
	/// \return The value, or \c ImageErrorCode::InvalidArgument for invalid coordinates.
	[[nodiscard]] auto read (int x, int y) const -> std::expected<Value, ImageError> {
		auto selected = row(y);
		if (!selected)
			return std::unexpected(selected.error());
		return selected->read(x);
	}

	/// \brief Read an exact native pixel without bounds checking.
	///
	/// \pre The guard must be active and both coordinates must lie within the view.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate.
	///
	/// \return An independent native value.
	[[nodiscard]] auto readUnchecked (int x, int y) const -> Value { return rowUnchecked(y).readUnchecked(x); }


	////
	// Methods

	/// \brief Store a native value with bounds checking, preserving unused components.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate.
	/// \param value Exact native components.
	///
	/// \return Nothing on success, or \c ImageErrorCode::InvalidArgument for invalid coordinates.
	[[nodiscard]] auto write (int x, int y, Value value) const -> std::expected<void, ImageError> requires Writable {
		auto selected = row(y);
		if (!selected)
			return std::unexpected(selected.error());
		return selected->write(x, value);
	}

	/// \brief Store a native value without bounds checking.
	///
	/// \pre The guard must be active and both coordinates must lie within the view.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate.
	/// \param value Exact native components; unused storage is retained.
	void writeUnchecked (int x, int y, Value value) const requires Writable { rowUnchecked(y).writeUnchecked(x, value); }


private:
	////
	// Fields

	/// Borrowed storage start.
	Byte *m_bytes;

	/// Visible width.
	int m_width;

	/// Visible height.
	int m_height;

	/// Physical byte stride.
	std::size_t m_pitch;
};

/// Movable exclusive image access session. The image must remain alive and unmoved until release.
///
/// \tparam Format Exact supported SDL format.
/// \tparam Writable Whether its view permits writes.
template<SDL_PixelFormat Format, bool Writable>
class PixelAccessGuard
{
	////
	// Friend declarations

	/// The image creates guards only after validating its storage and acquiring exclusive access.
	friend class Image;

public:
	////
	// Types

	/// Borrowed view valid only while this guard remains active.
	using View = PixelView<Format, Writable>;


	////
	// Object construction/destruction

	/// Release the session and SDL lock, if still active.
	~PixelAccessGuard() { release(); }

	/// Access sessions cannot be copied.
	PixelAccessGuard(const PixelAccessGuard&) = delete;

	/// Access sessions cannot be copy-assigned.
	auto operator= (const PixelAccessGuard&) -> PixelAccessGuard& = delete;

	/// \brief Transfer responsibility for releasing a session.
	///
	/// \param other Guard to move from; becomes inactive.
	PixelAccessGuard(PixelAccessGuard &&other) noexcept
		: m_surface(std::exchange(other.m_surface, nullptr)), m_active(std::exchange(other.m_active, nullptr)) {}

	/// \brief Release the current session and transfer another session.
	///
	/// \param other Guard to move from; becomes inactive.
	///
	/// \return This guard.
	auto operator= (PixelAccessGuard &&other) noexcept -> PixelAccessGuard&
	{
		if (this != &other) {
			release();
			m_surface = std::exchange(other.m_surface, nullptr);
			m_active = std::exchange(other.m_active, nullptr);
		}
		return *this;
	}


	////
	// Accessors

	/// \brief Extract a view from an active lvalue guard.
	///
	/// \return A borrowed view, or \c ImageErrorCode::InvalidState after release or move.
	[[nodiscard]] auto view () & -> std::expected<View, ImageError>
	{
		if (!m_surface)
			return std::unexpected(ImageError{ImageErrorCode::InvalidState, "Pixel guard is inactive"});
		return View(static_cast<typename View::Byte*>(m_surface->pixels), m_surface->w, m_surface->h,
			std::size_t(m_surface->pitch));
	}

	/// Borrowing from a temporary guard is unavailable.
	auto view () && -> std::expected<View, ImageError> = delete;


	////
	// Methods

	/// Release access early. All views, rows, iterators and spans from this session expire immediately.
	void release () noexcept {
		if (m_surface)
			detail::releasePixels(std::exchange(m_surface, nullptr), std::exchange(m_active, nullptr));
	}


private:
	////
	// Object construction

	/// Wrap an acquired session; only \c Image may construct it.
	PixelAccessGuard(SDL_Surface *surface, bool *active) : m_surface(surface), m_active(active) {}


	////
	// Fields

	/// Locked surface while active.
	SDL_Surface *m_surface;

	/// Image-owned exclusive-access flag; the image must remain alive and unmoved.
	bool *m_active;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_PIXEL_ACCESS_H__
