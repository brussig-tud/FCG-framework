
#ifndef __FCG_IMAGE_H__
#define __FCG_IMAGE_H__


//////
//
// Library documentation
//

/// \page fcg_image_overview Image library overview
///
/// The Image library decodes image files into CPU pixel data that you can inspect, process, or hand to the GPU. It is
/// built on SDL_image and supports the formats that its build provides (BMP, GIF, JPG, PNG, TGA, WebP, and more).
///
/// Link your application against \verbatim FCG-Framework::Image \endverbatim and include
/// <tt>\#include &lt;FCG/Image/image_loader.h&gt;</tt> for the generic loader, or
/// <tt>\#include &lt;FCG/Image/sdl_image.h&gt;</tt> to work with the SDL_image backend directly. Loading runs
/// synchronously on the CPU; it needs no GPU device, no applet, and no SDL video initialization.
///
/// \section fcg_image_start Where to start
/// - Decode a file and display it: \ref fcg_images for the CPU side, then upload the pixels with
///   <tt>\ref fcg::Image::upload "Image::upload"</tt> into a \ref fcg_textures "texture".
/// - Loading by file extension or by content: \ref fcg_image_loading.
/// - Using SDL_image directly, or teaching the loader additional formats: \ref fcg_sdl_image.
///
/// The library only loads. Saving, animation sequences, resizing, and automatic format conversion are out of its
/// scope.
///
/// \section fcg_image_formats Supporting more image formats
/// Formats with external codec dependencies such as WebP or AVIF are disabled by default in source builds. Enable
/// them before adding the framework:
/// \code{.cmake}
/// set(SDLIMAGE_WEBP ON CACHE BOOL "Enable WebP decoding")
/// CPMAddPackage(NAME FCG SOURCE_DIR "/path/to/framework")
/// target_link_libraries(my_app PRIVATE FCG-Framework::Image)
/// \endcode
/// The full build and codec reference is part of the framework's maintainer documentation.



//////
//
// Module documentation
//

/// \defgroup fcg_images CPU images
/// \ingroup fcg_modules
/// \brief Owning decoded images with their native SDL surface representation.
///
/// A <tt>\ref fcg::Image "Image"</tt> owns one \c SDL_Surface in CPU memory. Obtain one from the
/// <tt>\ref fcg::ImageLoader "ImageLoader"</tt>; custom format handlers can create one with
/// <tt>\ref fcg::Image::adopt "adopt"</tt>. The image keeps the decoded pixel format as the loader produced it:
/// pixel format, row pitch (a byte count, not width times four), palette, color key and transparency metadata are
/// preserved, and a row can contain padding. Inspect and manipulate the surface with SDL's surface and pixel APIs;
/// lock the surface first when \c SDL_MUSTLOCK requires it.
///
/// \section fcg_images_usage Getting started
/// \snippet image_examples.cpp includes
/// Load a file synchronously and use the image, e.g. by uploading it to the GPU:
/// \snippet image_examples.cpp file
///
/// \section fcg_images_workflows Common tasks
/// \subsection fcg_images_workflows_pixels Working with the pixels
/// The surface can be processed with SDL's conversion and pixel routines; adopt the result as a new image:
/// \snippet image_examples.cpp surface
///
/// \subsection fcg_images_workflows_gpu Handing pixels to the GPU
/// <tt>\ref fcg::Image::upload "Image::upload"</tt> creates a sampled 2D texture from the image: row order and
/// decoded precision are preserved, shaders read linear color values, and the CPU image is left unchanged. See
/// \ref fcg_textures for the texture that comes out.
///
/// \subsection fcg_images_workflows_delayed Images created later
/// There is no default constructor and images cannot be copied, so use <tt>std::optional&lt;\ref fcg::Image
/// "Image"&gt;</tt> for delayed creation or release:
/// \snippet image_examples.cpp optional
///
/// \section fcg_images_lifetime Ownership and lifetime
/// Moving an image transfers its surface and leaves the source empty. Borrowed \c SDL_Surface pointers from the
/// accessors must not outlive the image. For adopted surfaces whose pixel storage is external, that storage must
/// outlive the image in turn; <tt>\ref fcg::Image::adopt "adopt"</tt> does not copy external pixels. Serialize access
/// to an image externally; surfaces borrowed while another thread mutates the image are not safe.
///
/// \section fcg_images_errors Handling errors
/// Factories return <tt>std::expected&lt;\ref fcg::Image "Image", \ref fcg::ImageError "ImageError"&gt;</tt> with the
/// reason in the error's message. Nothing logs automatically, so you decide how to report failures.
/// <tt>\ref fcg::Image::adopt "adopt"</tt> takes ownership of a surface only on success; the caller keeps and must
/// release a rejected surface. Standard allocation exceptions can still propagate.
///
/// \see \ref fcg_image_loading, \ref fcg_sdl_image, \ref fcg_textures



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
#include <FCG/texture.h>



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

/// \addtogroup fcg_images
/// @{



//////
//
// Structs and enums
//

/// Categories of image loading, upload and handler registry failures, reported in \c ImageError::code.
enum class ImageErrorCode
{
	/// \brief
	/// 	An argument was rejected: empty encoded bytes, an empty file, an empty path, a path or hint containing a NUL
	/// 	character, a surface not acceptable to \c Image::adopt, or an empty or NUL-containing handler ID or a null
	/// 	handler passed to \c ImageLoader::registerHandler.
	InvalidArgument,

	/// An image file could not be opened, sized or read.
	IOFailure,

	/// \brief
	/// 	No registered handler accepted the input (this includes an empty registry), or \c Image::upload was called
	/// 	on an image that has a color key and a pixel format with more than 32 bits per pixel.
	UnsupportedFormat,

	/// \brief
	/// 	Decoding failed. When reported by \c ImageLoader::load, the message lists the ID and error message of every
	/// 	handler that failed, one per line, in the order the handlers were tried.
	DecodeFailure,

	/// An SDL operation, such as creating a memory stream or converting a surface, failed.
	SDLFailure,

	/// \c ImageLoader::registerHandler was given an ID that is already registered.
	DuplicateHandler,

	/// The operation was called on a moved-from, empty \c Image.
	InvalidState,

	/// The device cannot sample a suitable texture format, or creating or uploading the texture failed.
	GPUFailure
};

/// \brief
/// 	Error returned by image loading, upload and registry operations: a category plus a human-readable message
/// 	(including SDL's message where relevant).
struct ImageError
{
	/// Failure category.
	ImageErrorCode code;

	/// Human-readable description of the failure.
	std::string message;
};



//////
//
// Classes
//

/// \brief
/// 	A decoded image in CPU memory, held as an \c SDL_Surface.
///
/// Obtain images by loading them, typically with <tt>ImageLoader::global().load(path)</tt>, or wrap a surface you
/// created yourself with \c adopt. The surface keeps the pixel format, row pitch, palette and color key produced by
/// the decoder; use \c format and \c pitch to interpret the pixels, or convert the surface with SDL when you need a
/// particular layout. Call \c upload to create a GPU texture from the image.
///
/// An \c Image is the sole owner of its surface and destroys it on destruction. It is movable but not copyable. A
/// moved-from \c Image is empty: \c handle returns \c nullptr, \c width, \c height and \c pitch return 0, \c format
/// returns \c SDL_PIXELFORMAT_UNKNOWN, and \c upload returns \c ImageErrorCode::InvalidState. Only assignment and
/// destruction are otherwise meaningful on an empty image. Access to one image must be externally synchronized.
class FCG_IMAGE_EXPORT Image
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Takes ownership of an existing SDL surface without copying its pixels.
	///
	/// On success, the returned \c Image destroys the surface with \c SDL_DestroySurface when it is destroyed. On
	/// failure, ownership stays with the caller, who must release the surface. If the surface refers to pixel memory
	/// it does not own (e.g. one created with \c SDL_CreateSurfaceFrom), that memory must outlive the returned
	/// \c Image.
	///
	/// \param surface
	/// 	The surface to adopt. It must be non-null, have positive width, height and pitch, and have pixel storage.
	///
	/// \return
	/// 	The image owning \c surface, or \c ImageErrorCode::InvalidArgument if \c surface does not meet the
	/// 	requirements above.
	[[nodiscard]] static auto adopt (SDL_Surface *surface) -> std::expected<Image, ImageError>;

	/// Destroys the surface. Pointers obtained from \c handle become invalid.
	~Image();

	/// Not copyable.
	Image (const Image&) = delete;

	/// Not copy-assignable.
	auto operator= (const Image&) -> Image& = delete;

	/// Takes over the surface of another image, leaving \c other empty.
	///
	/// \param other The image to move from; may already be empty.
	Image (Image &&other) noexcept;

	/// \brief
	/// 	Destroys the current surface and takes over the surface of another image, leaving \c other empty.
	///
	/// Self-assignment has no effect.
	///
	/// \param other
	/// 	The image to move from; may already be empty.
	///
	/// \return A reference to this image.
	auto operator= (Image &&other) noexcept -> Image&;


	////
	// Accessors

	/// \brief
	/// 	Returns the underlying SDL surface for reading or modifying pixels and surface properties.
	///
	/// The surface remains owned by this image; do not destroy it, and do not use the pointer after the image is
	/// destroyed or moved from.
	///
	/// \return The surface, or \c nullptr if the image is empty.
	[[nodiscard]] auto handle () -> SDL_Surface* { return m_handle; }

	/// \brief
	/// 	Returns the underlying SDL surface for read-only inspection.
	///
	/// The surface remains owned by this image; do not use the pointer after the image is destroyed or moved from.
	///
	/// \return The surface, or \c nullptr if the image is empty.
	[[nodiscard]] auto handle () const -> const SDL_Surface* { return m_handle; }

	/// Returns the width in pixels, or 0 if the image is empty.
	[[nodiscard]] auto width () const -> int;

	/// Returns the height in pixels, or 0 if the image is empty.
	[[nodiscard]] auto height () const -> int;

	/// Returns the distance between the starts of two consecutive rows in bytes, which can include padding, or 0 if
	/// the image is empty.
	[[nodiscard]] auto pitch () const -> int;

	/// Returns the SDL pixel format of the surface, or \c SDL_PIXELFORMAT_UNKNOWN if the image is empty.
	[[nodiscard]] auto format () const -> SDL_PixelFormat;


	////
	// Methods

	/// \brief
	/// 	Creates a sampled 2D GPU texture with the contents of this image.
	///
	/// The texture has the size of the image, one mip level, one layer, a sample count of 1 and sampler usage. The top
	/// row of the image becomes the first texture row, i.e. texture coordinate <tt>v = 0</tt>. Alpha is straight (not
	/// premultiplied), and a color key is turned into alpha: keyed pixels get alpha 0. Shaders always sample linear
	/// color values:
	/// - Images with sRGB colors and at most 8 bits per channel become an 8-bit sRGB texture in RGBA or BGRA order,
	///   which the sampler decodes to linear values.
	/// - Images already stored as linear sRGB in one of the SDL pixel formats \c RGBA32, \c BGRA32, \c RGBA64,
	///   \c RGBA64_FLOAT or \c RGBA128_FLOAT keep their precision in the corresponding UNORM or float texture format.
	/// - All other images, and images whose preferred texture format the device cannot sample, are converted to
	///   linear sRGB in \c SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, without tone mapping.
	///
	/// Query the resulting format with <tt>Texture::info().format</tt>. The upload is submitted without waiting for the
	/// GPU, and the image itself is left unchanged; it may be modified or destroyed as soon as this method returns.
	///
	/// \param device
	/// 	The device that creates the texture. It must outlive the returned texture.
	///
	/// \return
	/// 	The texture, or an error:
	/// 	- \c ImageErrorCode::InvalidState if the image is empty;
	/// 	- \c ImageErrorCode::UnsupportedFormat if the image has a color key and a pixel format with more than
	/// 	  32 bits per pixel;
	/// 	- \c ImageErrorCode::SDLFailure if converting the image, applying its color key or locking a surface
	/// 	  failed;
	/// 	- \c ImageErrorCode::GPUFailure if the device cannot sample the required texture format, or if creating
	/// 	  or uploading the texture failed.
	[[nodiscard]] auto upload (Device &device) const -> std::expected<Texture, ImageError>;


private:

	////
	// Object construction/destruction

	/// Wraps an already validated surface and takes ownership of it. Client code uses \c adopt instead.
	///
	/// \param surface Non-null surface that has passed the checks of \c adopt.
	explicit Image (SDL_Surface *surface) noexcept : m_handle(surface) {}


	////
	// Fields

	/// Owned SDL surface; null after move.
	SDL_Surface *m_handle = nullptr;
};

/// @}



//////
//
// Namespaces close
//

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_H__
