
#ifndef __FCG_IMAGE_H__
#define __FCG_IMAGE_H__


//////
//
// Library documentation
//

/// \page fcg_image_overview Image library overview
///
/// The Image library decodes image files into CPU pixel data that you can inspect, process, or hand to the GPU. It is
/// supports Radiance HDR and Portable Float Map, optional OpenEXR, and the formats its SDL_image build provides
/// (BMP, GIF, JPG, PNG, TGA, WebP, and more).
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
/// - Loading floating-point HDR pixels: \ref fcg_radiance_hdr, \ref fcg_pfm, and \ref fcg_openexr.
///
/// The library provides explicit format conversion and scoped CPU pixel processing. For these tasks, start with
/// \ref fcg_pixel_formats and \ref fcg_pixel_access. Saving and animation sequences are outside its scope.
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
/// Radiance HDR and PFM are always available. Enable OpenEXR before adding the framework with
/// <tt>set(FCG_IMAGE_OPENEXR ON CACHE BOOL "Enable OpenEXR decoding")</tt>.



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
/// preserved, and a row can contain padding. Follow \ref fcg_pixel_formats to inspect, analyze and explicitly convert
/// a representation, then use \ref fcg_pixel_access for checked pixels and scoped native row access.
///
/// \section fcg_images_usage Getting started
/// \snippet image_examples.cpp includes
/// Load a file synchronously and use the image, e.g. by uploading it to the GPU:
/// \snippet image_examples.cpp file
///
/// \section fcg_images_workflows Common tasks
/// \subsection fcg_images_workflows_pixels Working with the pixels
/// Inspect and explicitly convert before accessing native samples:
/// \snippet pixel_examples.cpp conversion
/// \snippet pixel_examples.cpp access
/// For external SDL processing, adopt the resulting surface:
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
#include <functional>
#include <string>

// SDL3 library
#include <SDL3/SDL_pixels.h>

// Local includes
#include "FCG/Image/export.h"
#include "FCG/Image/pixel_format.h"
#include "FCG/Image/pixel_access.h"
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
// Classes
//

/// \brief
/// 	A decoded image in CPU memory, held as an \c SDL_Surface.
///
/// Obtain images by loading them, typically with <tt>ImageLoader::global().load(path)</tt>, or wrap a surface you
/// created yourself with \c adopt. The surface keeps the pixel format, row pitch, palette and color key produced by
/// the decoder; inspect \c pixelInfo and \c conversionInfo before calling \c converted for a particular layout. Call
/// \c upload to create a GPU texture from the image.
///
/// An \c Image is the sole owner of its surface and destroys it on destruction. It is movable but not copyable. A
/// moved-from \c Image is empty: \c handle returns \c nullptr, \c width, \c height and \c pitch return 0, \c format
/// returns \c SDL_PIXELFORMAT_UNKNOWN. Checked pixel, metadata, conversion and upload operations report
/// \c ImageErrorCode::InvalidState on an empty image. Access to one image must be externally synchronized.
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
	/// \c Image and provide the complete format-specific storage capacity, including row pitch and YUV planes.
	///
	/// \param surface
	/// 	The surface to adopt. It must be non-null, have positive width, height and pitch, and have pixel storage.
	///
	/// \return
	/// 	The image owning \c surface, or \c ImageErrorCode::InvalidArgument if \c surface does not meet the
	/// 	requirements above.
	[[nodiscard]] static auto adopt (SDL_Surface *surface) -> std::expected<Image, ImageError>;

	/// Destroys the surface. Pointers obtained from \c handle become invalid. All access guards must have ended.
	~Image();

	/// Not copyable.
	Image(const Image&) = delete;

	/// Not copy-assignable.
	auto operator= (const Image&) -> Image& = delete;

	/// Takes over the surface of another image, leaving \c other empty.
	///
	/// \param other The image to move from; may already be empty. Neither image may have an active access guard.
	Image(Image &&other) noexcept;

	/// \brief
	/// 	Destroys the current surface and takes over the surface of another image, leaving \c other empty.
	///
	/// Self-assignment has no effect.
	///
	/// \param other
	/// 	The image to move from; may already be empty. Neither image may have an active access guard.
	///
	/// \return A reference to this image.
	auto operator= (Image &&other) noexcept -> Image&;


	////
	// Accessors

	/// \brief
	/// 	Returns the underlying SDL surface for reading or modifying pixels and surface properties.
	///
	/// The surface remains owned by this image; do not destroy it, and do not use the pointer after the image is
	/// destroyed or moved from. Do not perform conflicting external pixel, locking, conversion or destruction
	/// operations during an active pixel access session.
	///
	/// \return The surface, or \c nullptr if the image is empty.
	[[nodiscard]] auto handle () -> SDL_Surface* { return m_handle; }

	/// \brief
	/// 	Returns the underlying SDL surface for read-only inspection.
	///
	/// The surface remains owned by this image; do not use the pointer after the image is destroyed or moved from. Do not perform conflicting external pixel, locking, conversion or destruction
	/// operations during an active pixel access session.
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

	/// \brief Snapshot the current storage, palette, transparency and color encoding.
	///
	/// \return Metadata, or \c ImageErrorCode::InvalidState for an empty image. External metadata edits are reflected.
	[[nodiscard]] auto pixelInfo () const -> std::expected<ImagePixelInfo, ImageError>;

	/// \brief Analyze potential conversion effects without allocating output pixels or scanning sample values.
	///
	/// \param targetFormat Non-indexed RGB destination identifier.
	/// \param options Loss, colorspace and color-key policies.
	///
	/// \return A report, including policy rejections and unsupported requests, or an error for unavailable metadata.
	[[nodiscard]] auto conversionInfo (SDL_PixelFormat targetFormat, PixelConversionOptions options = {}) const
		-> std::expected<PixelConversionInfo, ImageError>;

	/// \brief Allocate an independent converted image, retaining this image's pixels and metadata.
	///
	/// Omitted colorspace preserves RGB encoding and white-point units. A changed colorspace uses its default
	/// white-point units. Relative HDR headroom is retained, and tone mapping is disabled.
	///
	/// \param targetFormat Non-indexed RGB destination identifier.
	/// \param options Potential loss is rejected by default; omitted colorspace preserves source RGB encoding.
	///
	/// \return The image, or \c ImageErrorCode::LossyConversion for rejected loss,
	/// \c ImageErrorCode::InvalidArgument for invalid policies, \c ImageErrorCode::UnsupportedFormat for unsupported
	/// requests, \c ImageErrorCode::InvalidState for unavailable access, or \c ImageErrorCode::SDLFailure on SDL failure.
	[[nodiscard]] auto converted (SDL_PixelFormat targetFormat, PixelConversionOptions options = {}) const
		-> std::expected<Image, ImageError>;

	/// \brief Acquire exclusive writable native access, locking the surface when required.
	///
	/// \pre Keep this image alive and unmoved until the guard releases; externally synchronize access.
	///
	/// \tparam Format Exact supported SDL format.
	///
	/// \return A movable guard, or \c ImageErrorCode::InvalidState for empty, conflicting or invalid storage,
	/// \c ImageErrorCode::UnsupportedFormat for a format mismatch, or \c ImageErrorCode::SDLFailure on lock failure.
	template<SDL_PixelFormat Format> requires PixelFormatTraits<Format>::supported
	[[nodiscard]] auto mapPixels () & -> std::expected<PixelAccessGuard<Format, true>, ImageError> {
		auto surface = acquirePixels(Format);
		if (!surface)
			return std::unexpected(surface.error());
		return PixelAccessGuard<Format, true>(*surface, &m_accessActive);
	}

	/// \brief Acquire exclusive read-only native access, locking the surface when required.
	///
	/// \pre Keep this image alive and unmoved until the guard releases; externally synchronize access.
	///
	/// \tparam Format Exact supported SDL format.
	///
	/// \return A movable read-only guard, or \c ImageErrorCode::InvalidState for empty, conflicting or invalid storage,
	/// \c ImageErrorCode::UnsupportedFormat for a format mismatch, or \c ImageErrorCode::SDLFailure on lock failure.
	template<SDL_PixelFormat Format> requires PixelFormatTraits<Format>::supported
	[[nodiscard]] auto mapPixels () const & -> std::expected<PixelAccessGuard<Format, false>, ImageError> {
		auto surface = acquirePixels(Format);
		if (!surface)
			return std::unexpected(surface.error());
		return PixelAccessGuard<Format, false>(*surface, &m_accessActive);
	}

	/// Acquiring borrowed access from a temporary image is unavailable.
	template<SDL_PixelFormat Format>
	auto mapPixels () && -> std::expected<PixelAccessGuard<Format, true>, ImageError> = delete;

	/// Acquiring borrowed access from a const temporary image is unavailable.
	template<SDL_PixelFormat Format>
	auto mapPixels () const && -> std::expected<PixelAccessGuard<Format, false>, ImageError> = delete;

	/// \brief Read an independent exact native sample; the image format must match.
	///
	/// \tparam Format Exact supported SDL format.
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate; row zero is the top row.
	///
	/// \return Native components, or an acquisition or bounds error. Separate color-key transparency is not applied.
	template<SDL_PixelFormat Format> requires PixelFormatTraits<Format>::supported
	[[nodiscard]] auto readPixel (int x, int y) const -> std::expected<typename PixelFormatTraits<Format>::Value, ImageError> {
		auto guard = mapPixels<Format>();
		if (!guard)
			return std::unexpected(guard.error());
		return guard->view()->read(x, y);
	}

	/// \brief Write exact native components, preserving unused storage and color encoding.
	///
	/// \tparam Format Exact supported SDL format.
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate; row zero is the top row.
	/// \param value Native logical RGB or RGBA components.
	///
	/// \return Nothing on success, or an acquisition or bounds error.
	template<SDL_PixelFormat Format> requires PixelFormatTraits<Format>::supported
	[[nodiscard]] auto writePixel (int x, int y, typename PixelFormatTraits<Format>::Value value)
		-> std::expected<void, ImageError> {
		auto guard = mapPixels<Format>();
		if (!guard)
			return std::unexpected(guard.error());
		return guard->view()->write(x, y, value);
	}

	/// \brief Read RGBA bytes in the source encoding; narrowing clamps to [0,1] and rounds to the nearest byte.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate.
	///
	/// \return A color value, or an access, bounds, unsupported storage or malformed palette error.
	/// Missing alpha is opaque; a separate color key is not applied. Non-finite samples return an invalid argument error.
	[[nodiscard]] auto readColor8 (int x, int y) const -> std::expected<SDL_Color, ImageError>;

	/// \brief Read normalized RGBA floats in the source encoding, retaining floating HDR values.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate.
	///
	/// \return A color value, or an access, bounds, unsupported storage or malformed palette error.
	/// Missing alpha is opaque; a separate color key is not applied. Integer components are divided by their maximum.
	[[nodiscard]] auto readColorFloat (int x, int y) const -> std::expected<SDL_FColor, ImageError>;

	/// \brief Assign RGBA bytes in the source encoding, requiring permission for potential narrowing or alpha loss.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate.
	/// \param value Straight RGBA byte components.
	/// \param lossPolicy Permission for conservative precision and channel loss; rejection is the default.
	///
	/// \return Nothing on success, or an access, bounds, unsupported storage, invalid policy or loss rejection error.
	/// Indexed and YUV storage require explicit image conversion. Allowed narrowing clamps and rounds to nearest.
	[[nodiscard]] auto writeColor8 (int x, int y, SDL_Color value, PixelLossPolicy lossPolicy = PixelLossPolicy::Reject)
		-> std::expected<void, ImageError>;

	/// \brief Assign RGBA floats in the source encoding, requiring permission for potential narrowing or alpha loss.
	///
	/// \param x Zero-based horizontal coordinate.
	/// \param y Zero-based vertical coordinate.
	/// \param value Straight RGBA components; integer output rejects non-finite values.
	/// \param lossPolicy Permission for conservative precision, range and channel loss; rejection is the default.
	///
	/// \return Nothing on success, or an access, bounds, unsupported storage, invalid argument or loss rejection error.
	/// Finite integer narrowing clamps to [0,1] and rounds to nearest; binary16 uses \c Half conversion.
	[[nodiscard]] auto writeColorFloat (int x, int y, SDL_FColor value, PixelLossPolicy lossPolicy = PixelLossPolicy::Reject)
		-> std::expected<void, ImageError>;

	/// \brief Run a callback during one writable session; errors and exceptions release access.
	///
	/// \tparam Format Exact supported SDL format.
	/// \tparam Callback Callable returning void or <tt>std::expected&lt;void, ImageError&gt;</tt>.
	/// \param callback Receives a borrowed writable view that must not escape the session.
	///
	/// \return Nothing on success, or the acquisition or callback error.
	///
	/// \throws ... Any exception from \c callback propagates after releasing access.
	template<SDL_PixelFormat Format, class Callback> requires PixelFormatTraits<Format>::supported
	[[nodiscard]] auto withPixels (Callback &&callback) & -> std::expected<void, ImageError>
	{
		using Result = std::invoke_result_t<Callback, PixelView<Format, true>>;
		static_assert(std::is_void_v<Result> || std::is_same_v<Result, std::expected<void, ImageError>>);
		auto guard = mapPixels<Format>();
		if (!guard)
			return std::unexpected(guard.error());
		if constexpr (std::is_void_v<Result>) {
			std::invoke(std::forward<Callback>(callback), *guard->view());
			return {};
		}
		else
			return std::invoke(std::forward<Callback>(callback), *guard->view());
	}

	/// \brief Run a callback during one read-only session; errors and exceptions release access.
	///
	/// \tparam Format Exact supported SDL format.
	/// \tparam Callback Callable returning void or <tt>std::expected&lt;void, ImageError&gt;</tt>.
	/// \param callback Receives a borrowed read-only view that must not escape the session.
	///
	/// \return Nothing on success, or the acquisition or callback error.
	///
	/// \throws ... Any exception from \c callback propagates after releasing access.
	template<SDL_PixelFormat Format, class Callback> requires PixelFormatTraits<Format>::supported
	[[nodiscard]] auto withPixels (Callback &&callback) const & -> std::expected<void, ImageError>
	{
		using Result = std::invoke_result_t<Callback, PixelView<Format, false>>;
		static_assert(std::is_void_v<Result> || std::is_same_v<Result, std::expected<void, ImageError>>);
		auto guard = mapPixels<Format>();
		if (!guard)
			return std::unexpected(guard.error());
		if constexpr (std::is_void_v<Result>) {
			std::invoke(std::forward<Callback>(callback), *guard->view());
			return {};
		}
		else
			return std::invoke(std::forward<Callback>(callback), *guard->view());
	}

	/// Callback borrowing from a temporary image is unavailable.
	template<SDL_PixelFormat Format, class Callback>
	auto withPixels (Callback&&) && -> std::expected<void, ImageError> = delete;

	/// Callback borrowing from a const temporary image is unavailable.
	template<SDL_PixelFormat Format, class Callback>
	auto withPixels (Callback&&) const && -> std::expected<void, ImageError> = delete;

	/// \brief Apply an independent in-place operation to every visible native pixel in row order.
	///
	/// \tparam Format Exact supported SDL format.
	/// \tparam Kernel Callable accepting and returning the native value type.
	/// \param kernel Pointwise operation; exceptions release access and may leave partial output.
	///
	/// \return Nothing on success, or an acquisition error.
	///
	/// \throws ... Any exception from \c kernel propagates after releasing access.
	template<SDL_PixelFormat Format, class Kernel> requires PixelFormatTraits<Format>::supported
	[[nodiscard]] auto transformPixels (Kernel &&kernel) & -> std::expected<void, ImageError> {
		return withPixels<Format>([&kernel](auto view) {
			for (auto row : view)
				for (int x = 0; x < row.size(); ++x)
					row.writeUnchecked(x, std::invoke(kernel, row.readUnchecked(x)));
		});
	}

	/// \brief Compute independent output pixels using a read-only source view for neighborhood access.
	///
	/// \tparam Format Exact supported SDL format for both images.
	/// \tparam Kernel Callable accepting output x, output y and a read-only source view, returning a native value.
	/// \param destination Matching dimensions and format, with nonoverlapping pixel storage.
	/// \param kernel Operation that chooses border behavior; exceptions may leave partial output.
	///
	/// \return Nothing on success, or an acquisition error, or \c ImageErrorCode::InvalidArgument for dimensions or overlap.
	///
	/// \throws ... Any exception from \c kernel propagates after releasing access.
	template<SDL_PixelFormat Format, class Kernel> requires PixelFormatTraits<Format>::supported
	[[nodiscard]] auto transformPixelsTo (Image &destination, Kernel &&kernel) const & -> std::expected<void, ImageError>
	{
		auto compatible = validateDestination(destination);
		if (!compatible)
			return compatible;
		auto sourceGuard = mapPixels<Format>();
		if (!sourceGuard)
			return std::unexpected(sourceGuard.error());
		auto destinationGuard = destination.mapPixels<Format>();
		if (!destinationGuard)
			return std::unexpected(destinationGuard.error());
		if (auto overlap = validateStorageOverlap(destination); !overlap)
			return overlap;
		const auto source = *sourceGuard->view();
		const auto output = *destinationGuard->view();
		for (int y = 0; y < output.height(); ++y)
			for (int x = 0; x < output.width(); ++x)
				output.writeUnchecked(x, y, std::invoke(kernel, x, y, source));
		return {};
	}

	/// \brief
	/// 	Creates a sampled 2D GPU texture with the contents of this image.
	///
	/// The texture has the size of the image, one mip level, one layer, a sample count of 1 and sampler usage. The top
	/// row of the image becomes the first texture row, i.e. texture coordinate <tt>v = 0</tt>. Alpha association is
	/// preserved, and a color key is turned into alpha: keyed pixels get alpha 0. Shaders always sample linear
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
	/// RGB sources need established primaries and transfer characteristics before uploading; label a known source
	/// encoding with \c SDL_SetSurfaceColorspace when the file does not identify it.
	///
	/// \param device
	/// 	The device that creates the texture. It must outlive the returned texture.
	///
	/// \return
	/// 	The texture, or an error:
	/// 	- \c ImageErrorCode::InvalidState if the image is empty or has an active pixel access session;
	/// 	- \c ImageErrorCode::UnsupportedFormat if RGB encoding is insufficiently described, or the image has a
	/// 	  color key and a pixel format with more than 32 bits per pixel;
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
	explicit Image(SDL_Surface *surface) noexcept : m_handle(surface) {}


	////
	// Methods

	/// Validate storage and acquire exclusive access, with an optional exact native format requirement.
	[[nodiscard]] auto acquirePixels (SDL_PixelFormat requiredFormat) const -> std::expected<SDL_Surface*, ImageError>;

	/// Validate matching dimensions and disjoint physical storage for a neighborhood kernel.
	[[nodiscard]] auto validateDestination (const Image &destination) const -> std::expected<void, ImageError>;

	/// Check byte-range overlap after both kernel images have acquired stable locked storage.
	[[nodiscard]] auto validateStorageOverlap (const Image &destination) const -> std::expected<void, ImageError>;

	/// Store encoding-preserving components with byte or binary32 source precision.
	[[nodiscard]] auto writeColor (int x, int y, SDL_FColor value, PixelLossPolicy policy, bool byteSource)
		-> std::expected<void, ImageError>;


	////
	// Fields

	/// Whether an exclusive image access session currently borrows this image.
	mutable bool m_accessActive = false;

	/// Owned SDL surface; null after move.
	SDL_Surface *m_handle = nullptr;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_H__
