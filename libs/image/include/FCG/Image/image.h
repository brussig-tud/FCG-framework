#ifndef __FCG_IMAGE_H__
#define __FCG_IMAGE_H__


//////
//
// Module documentation
//

/// \defgroup fcg_images CPU images
/// \ingroup fcg_components
/// \brief Owning decoded images with native SDL surface representation.
///
/// \section images_model Resource model and pixel representation
/// \snippet image_examples.cpp includes
///
/// A <code>\ref fcg::Image</code> owns one \c SDL_Surface in CPU memory. Obtain it from
/// <code>\ref fcg::ImageLoader</code> or an image format handler; custom handlers can adopt a newly created surface
/// through <code>\ref fcg::Image::adopt</code>. <code>\ref fcg::Image "Image"</code> does not own a device, renderer,
/// window, or GPU texture. Loading is synchronous; the result is independent of the encoded input. See
/// \ref fcg_image_loading for dispatch and \ref fcg_sdl_image for the shipped decoder.
///
/// Native pixel representation is preserved. Pixel format, row pitch, palette, color key, and transparency metadata are
/// not normalized. A row can contain padding; pitch is a byte count, not width times four. Indexed surfaces need their
/// palette. Consult SDL surface/pixel APIs and lock surfaces when \c SDL_MUSTLOCK requires it before direct pixel
/// access. Convert explicitly with \c SDL_ConvertSurface when a particular layout is required.
/// <code>\ref fcg::Image "Image"</code> makes no promise of RGBA8, color-space conversion, premultiplied alpha, or a
/// particular image orientation beyond SDL's output.
/// \snippet image_examples.cpp surface
///
/// \section images_lifetime Ownership, moves, and absence
/// Images have no default constructor and cannot be copied. A successful factory creates a valid owner; moving
/// transfers its surface and leaves a null handle, zero dimensions/pitch, and \c SDL_PIXELFORMAT_UNKNOWN in the source.
/// Moving an already empty owner transfers that empty state. Self move-assignment is a no-op. The destructor calls
/// \c SDL_DestroySurface; it neither waits for GPU work nor submits anything. Borrowed surface pointers must not be
/// destroyed by clients and must not survive the owning <code>\ref fcg::Image "Image"</code>. Any surface storage
/// borrowed by an adopted surface must independently outlive the <code>\ref fcg::Image "Image"</code>;
/// <code>\ref fcg::Image::adopt "adopt"</code> does not deep-copy external pixels.
///
/// Use <code>std::optional&lt;\ref fcg::Image "Image"&gt;</code> for delayed creation. Moving out of an optional does
/// not disengage it: reset it explicitly when it should represent absence. Keep borrowed pixel/surface views within the
/// owner's lifetime and externally serialize access to each image. Surface operations that change metadata remain
/// visible through the accessors.
/// \snippet image_examples.cpp optional
///
/// \section images_errors Factories and errors
/// <code>\ref fcg::Image::adopt "Image::adopt"</code> rejects null surfaces, nonpositive dimensions/pitch, and missing
/// pixel storage. Ownership transfers only on success; the caller retains a rejected non-null surface and must release
/// it. Loaders and handlers clean up their temporary surfaces on all failure paths. Factories return
/// <code>std::expected&lt;\ref fcg::Image "Image",\ref fcg::ImageError "ImageError"&gt;</code>; messages own their text
/// and survive subsequent SDL calls. Standard allocation exceptions can still propagate. Nothing logs automatically.
/// See \ref fcg_image_loading for unsupported formats and fallback diagnostics.



//////
//
// Includes
//

// C++ STL
#include <expected>
#include <string>

// SDL3 library
#include <SDL3/SDL_pixels.h>

// Local includes
#include "FCG/Image/export.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_Surface;



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Structs & enums
//

/// Categories of recoverable image and registry failures. \ingroup fcg_images
enum class ImageErrorCode
{
	/// Empty bytes, malformed hint/path, invalid surface, null handler, or empty handler ID.
	InvalidArgument,

	/// An image file could not be opened or read.
	IOFailure,

	/// No registered handler accepted the input.
	UnsupportedFormat,

	/// An accepting handler could not decode the image; may contain ordered fallback diagnostics.
	DecodeFailure,

	/// SDL stream creation or another backend resource operation failed.
	SDLFailure,

	/// The registry already contains the requested ID.
	DuplicateHandler
};

/// An owned error, independent of the lifetime of input bytes and SDL diagnostics. \ingroup fcg_images
struct ImageError
{
	/// Machine-readable failure category.
	ImageErrorCode code;

	/// Human-readable context; loader failures include the IDs of failed handlers in attempt order.
	std::string message;
};



//////
//
// Classes
//

/// \brief Unique owner of a decoded SDL surface, preserving native representation. \ingroup fcg_images
class FCG_IMAGE_EXPORT Image
{
public:

	////
	// Object construction/destruction

	/// Adopt a surface without copying its pixels. Ownership transfers only on success.
	///
	/// \param surface Live surface with positive dimensions/pitch and pixel storage; retained by caller on failure.
	///
	/// \return A unique owner or `InvalidArgument`. Externally borrowed pixel storage is not made owning.
	[[nodiscard]] static auto adopt (SDL_Surface *surface) -> std::expected<Image, ImageError>;

	/// Destroy the owned surface. Borrowed surface/pixel references must end first.
	~Image();

	/// `Image` objects have unique ownership and cannot be copied.
	Image (const Image&) = delete;

	/// Surfaces cannot be copy-assigned.
	auto operator= (const Image&) -> Image& = delete;

	/// Transfer ownership, leaving the source empty.
	///
	/// \param other Source owner; may already be empty.
	Image (Image &&other) noexcept;

	/// Release the current surface and transfer ownership; self-assignment is a no-op.
	///
	/// \param other Source owner, left empty. \returns A reference to this owner.
	auto operator= (Image &&other) noexcept -> Image&;


	////
	// Accessors

	/// Borrow the mutable SDL surface; do not destroy it. Null after move.
	///
	/// \return The owned surface, with ownership retained by this `Image`.
	[[nodiscard]] auto handle () -> SDL_Surface* { return m_handle; }

	/// Borrow the SDL surface for inspection. Null after move.
	///
	/// \return A `const` surface pointer; no lifetime extension occurs.
	[[nodiscard]] auto handle () const -> const SDL_Surface* { return m_handle; }

	/// Width in pixels, or zero after move.
	///
	/// \return The current surface width.
	[[nodiscard]] auto width () const -> int;

	/// Height in pixels, or zero after move.
	///
	/// \return The current surface height.
	[[nodiscard]] auto height () const -> int;

	/// Row stride in bytes, including any padding; zero after move.
	///
	/// \return The current pitch.
	[[nodiscard]] auto pitch () const -> int;

	/// Native SDL pixel format, or \c UNKNOWN after move.
	///
	/// \return The current surface format.
	[[nodiscard]] auto format () const -> SDL_PixelFormat;


private:

	////
	// Object construction/destruction

	/// Adopt a validated surface internally; use `adopt()` from client code.
	///
	/// \param surface Non-null validated surface whose ownership transfers here.
	explicit Image (SDL_Surface *surface) noexcept : m_handle(surface) {}


	////
	// Fields

	/// Owned SDL surface; null after move.
	SDL_Surface *m_handle = nullptr;
};



//////
//
// Namespaces close
//

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_H__
