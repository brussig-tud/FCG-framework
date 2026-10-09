
#ifndef __FCG_IMAGE_LOADER_H__
#define __FCG_IMAGE_LOADER_H__


//////
//
// Module documentation
//

/// \page fcg_image_guide Image library guide
///
/// Link the Image CMake target for the ability to load image files:
/// \verbatim FCG-framework::Image \endverbatim
/// Include `<FCG/Image/image_loader.h>` for the generic loader and
/// `<FCG/Image/sdl_image.h>` for direct backend use. This library publicly links Core. CPU loading requires no applet,
/// SDL video initialization, or GPU device. The \ref fcg_images, \ref fcg_image_loading, and \ref fcg_sdl_image
/// guides describe its components. Core provides textures and rendering; <tt>\ref fcg::Image::upload</tt> explicitly converts and uploads decoded images. Core lives in `core/` and exposes headers under `FCG/`;
/// Image lives in `libs/image/` and exposes headers under `FCG/Image/`.
///
/// \section image_library_build Build and dependencies
/// \c FCG_SHARED_LIBS selects framework linkage; \c FCG_USE_SHARED_SDL selects SDL linkage, defaulting to the framework
/// choice. Source-built SDL_image follows the latter because a shared SDL_image requires shared SDL3. SDL3_image
/// 3.4.4 is the pinned source fallback. Compatible system packages and previously provided \c SDL3_image::SDL3_image
/// targets are reused as-is; source configuration cannot change their codecs or linkage. As with Core, installed
/// targets currently defer full third-party dependency exports. CPM/`add_subdirectory` consumers receive build-time
/// dependency targets transitively.
///
/// The source fallback enables built-in formats and avoids external codec dependencies by default. AVIF, JPEG XL,
/// TIFF, WebP, libpng, and codec vendoring default to `OFF`. PNG uses SDL's built-in decoder and JPEG uses SDL_image's
/// stb backend. Change the upstream \c SDLIMAGE_* variables before adding the framework; FCG respects normal variables
/// and cache entries and introduces no bespoke codec switches. To decode WebP with installed codec dependencies:
/// \code{.cmake}
/// set(SDLIMAGE_WEBP ON CACHE BOOL "Enable WebP decoding")
/// set(SDLIMAGE_STRICT ON CACHE BOOL "Require requested codec dependencies")
/// CPMAddPackage(NAME FCG SOURCE_DIR "/path/to/framework")
/// target_link_libraries(my_app PRIVATE FCG-framework::Image)
/// \endcode
/// Replace the source path with your framework checkout, or use your pinned `GITHUB_REPOSITORY`/`GIT_TAG` in CPM.
/// To enable AVIF loading, set \c SDLIMAGE_AVIF=ON before adding the framework. For a source-built, non-vendored SDL_image,
/// CPM reuses a supplied \c avif target or a compatible installed libavif package (minimum version 1.0); otherwise it
/// fetches libavif 1.4.2. Explicit \c AVIF_CODEC_* settings are respected. Automatic decoder selection reuses an installed
/// libaom, dav1d, or libgav1, or builds libaom locally when none is available. Without an x86 assembler, local libaom
/// uses its portable implementation unless \c AOM_TARGET_CPU is explicitly selected. Local libaom requires Perl;
/// Git for Windows' bundled Perl is discovered automatically. AVIF saving, libavif tools, tests,
/// examples, and optional libyuv default to `OFF`. Enable \c SDLIMAGE_AVIF_SAVE to also request an encoder. Codec linkage
/// defaults to static with position-independent code; explicit \c SDLIMAGE_AVIF_SHARED or \c SDLIMAGE_DEPS_SHARED choices
/// are respected. Supplied or installed SDL_image backends retain their existing capabilities and do not trigger this
/// codec resolution. Normal variables, cache entries, and standard CPM source/download overrides remain supported.
/// Set \c SDLIMAGE_VENDORED=ON to use SDL_image's own codec submodules instead; enabled submodules are fetched with the
/// pinned SDL_image checkout. Enable \c SDLIMAGE_JXL, \c SDLIMAGE_TIF, or \c SDLIMAGE_PNG_LIBPNG similarly.
/// \c SDLIMAGE_STRICT makes missing dependencies a configuration error rather than silently disabling a requested codec.
/// Vendored codecs link statically by default (\c SDLIMAGE_DEPS_SHARED=OFF), keeping them inside SDL_image.
/// Options also work with `-D` on the command line. Prebuilt packages may support more or fewer formats.
/// Resolved source/package capabilities determine the shipped handler's advertised formats. If a supplied target or
/// package has no trustworthy metadata, set \c FCG_SDL_IMAGE_FORMATS to an explicit list of SDL codec identifiers:
/// `ANI;AVIF;BMP;GIF;JPG;JXL;LBM;PCX;PNG;PNM;QOI;SVG;TGA;TIF;WEBP;XCF;XPM;XV`. An explicitly empty list is valid;
/// unknown identifiers and missing declarations fail configuration. This declaration describes the existing backend,
/// rather than changing its codec configuration. Configured dynamic codecs can still fail when runtime libraries are
/// unavailable. ANI is animation-only and is omitted from still-image metadata; BMP also advertises ICO and CUR.
/// Adding the virtual metadata method preserves existing handler source compatibility; binary consumers must rebuild.
///
/// \section image_library_start Starting points
/// Use <code>\ref fcg::ImageLoader::global "ImageLoader::global()"</code> for the shipped automatic registry, a local
/// <code>\ref fcg::ImageLoader "ImageLoader"</code> for explicit control, or
/// <code>\ref fcg::SDLImageFormatHandler "SDLImageFormatHandler"</code> directly when backend choice is intentional.
/// All loads are synchronous and return `std::expected`. No initialization call, logger, GPU allocation, cache, or
/// background task is introduced. Registration exists for loaded translation units; see \ref image_loading_registration
/// for static-archive retention. Saving, animation sequences, resizing, automatic conversion, and GPU upload are
/// outside this library's loading API. SDL_image's still-image API supplies one surface even for formats that can
/// contain multiple frames.



//////
//
// Component documentation
//

/// \defgroup fcg_image_loading Image loading and format handlers
/// \ingroup fcg_components
/// \brief Format-independent dispatch over an owned registry, with optional automatic global registration.
///
/// \section image_loading_workflows File and memory workflows
/// \snippet image_examples.cpp file
/// Files are read once, then dispatched as bytes with their extension as a hint. No handler reopens the path, and
/// every fallback attempt sees identical bytes. Paths use `std::filesystem::path`, including native Unicode paths.
/// Memory inputs are `std::span<const std::byte>` of encoded file data, not decoded pixel data. They are borrowed only
/// during the call and may be released immediately after it returns. An optional hint is an extension such as `"png"`
/// or `"TGA"`, with an optional leading dot; ASCII case is normalized. Embedded NULs are rejected. Hints are advisory:
/// signature-based formats can still load when a filename is extensionless or misleading. TGA usually needs a hint.
/// \snippet image_examples.cpp memory
///
/// \section image_loading_dispatch Acceptance, ordering, and fallback
/// <code>\ref fcg::ImageFormatHandler::accepts "ImageFormatHandler::accepts"</code> decides whether an input is worth
/// attempting without retaining or modifying it. A positive answer does not guarantee decoding or codec availability.
/// <code>\ref fcg::ImageLoader "ImageLoader"</code> knows no format names or magic bytes. Its registry owns handlers,
/// sorted by descending integer priority then ascending ID for equal priorities. IDs are unique, nonempty owned
/// strings. Client priorities default to zero; the shipped SDL_image entry uses `-100`. A handler may return a probe
/// error or a decoding error; both are collected and fallback continues. The first successful decoder wins.
/// Registration/removal during a callback into the same loader is forbidden. Each call starts dispatch afresh; neither
/// successful nor failed input is cached.
/// \snippet image_examples.cpp independent
///
/// \section image_loading_lifetime Registry ownership and synchronization
/// <tt>\ref fcg::ImageFormatHandler::fileFormats</tt> defaults to an empty list.
/// <tt>\ref fcg::ImageLoader::fileFormats</tt> collects owning snapshots of <tt>\ref fcg::ImageFileFormat</tt> records
/// in registry order, retaining overlaps. Metadata advertises formats without guaranteeing decode success. Snapshots
/// remain valid after registration, removal, and registry destruction. The viewer queries the singleton for each dialog.
///
/// Independent loaders start empty and are neither copyable nor movable.
/// <code>\ref fcg::ImageLoader::registerHandler "registerHandler"</code> takes unique ownership; removal or loader
/// destruction destroys that handler. Images do not borrow their decoder or registry. Borrowed bytes/hints must not be
/// retained by implementations. Externally serialize registration, removal, inspection, and loading through each
/// loader. If a handler shares state across loaders, that state needs its own synchronization. Initialization of
/// <code>\ref fcg::ImageLoader::global "global()"</code> is thread-safe; registry operations have no internal locking.
///
/// \section image_loading_registration Automatic registration and global lifetime
/// <code>\ref fcg::ImageLoader::global "ImageLoader::global()"</code> is an exported, out-of-line accessor to one
/// process-lifetime registry per linked Image library instance. It is constructed on first use and is intentionally not
/// destroyed at program shutdown, avoiding static destruction-order dependencies. Its owned handlers likewise remain
/// until explicitly removed. Registration helpers never unregister on destruction. Avoid unloading a module while its
/// handler remains registered.
/// \snippet image_examples.cpp registration
///
/// A namespace-scope <code>\ref fcg::FormatHandlerRegistration "FormatHandlerRegistration"</code> transfers a
/// constructed handler to this registry during dynamic static initialization. It needs no function-scope bootstrap
/// call. Invalid/duplicate registrations throw `std::invalid_argument`; an uncaught exception in a static initializer
/// terminates startup. Allocation exceptions can also propagate. Singleton access is safe during static initialization,
/// but loaders in unrelated global constructors cannot assume all translation units' registrars have already run. The
/// full automatic registry is available after those initializers. The shipped handler has a link anchor referenced by
/// <code>\ref fcg::ImageLoader::global "global()"</code>, so ordinary static linking retains it. Client registrar-only
/// translation units in static archives require an anchor, object-library linkage, or whole-archive linkage: the helper
/// cannot register code that the linker never includes. A client anchor must reference the registrar's translation unit
/// from reachable code. Registration only constructs CPU-side objects, never GPU resources.
///
/// \section image_loading_errors Error handling
/// Empty inputs and invalid hints/paths produce
/// <code>\ref fcg::ImageErrorCode::InvalidArgument "InvalidArgument"</code>; inaccessible or unreadable files produce
/// <code>\ref fcg::ImageErrorCode::IOFailure "IOFailure"</code>.
/// <code>\ref fcg::ImageErrorCode::UnsupportedFormat "UnsupportedFormat"</code> means every registered handler
/// declined, including an empty registry. If any attempt failed and none succeeded,
/// <code>\ref fcg::ImageErrorCode::DecodeFailure "DecodeFailure"</code> contains handler IDs and their diagnostics in
/// attempt order. Individual handlers can return <code>\ref fcg::ImageErrorCode::SDLFailure "SDLFailure"</code> or
/// another more specific category, retained as text in the aggregate. Errors own their messages; SDL diagnostics are
/// captured immediately. The library does not log. Allocation and implementation exceptions are not caught as decode
/// failures; custom handlers should use `std::expected` for recoverable failures.
/// \snippet image_examples.cpp errors



//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Local includes
#include "FCG/Image/image.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Structs and enums
//

/// \brief Owned metadata for an advertised image format; decoding can still fail. \ingroup fcg_image_loading
struct ImageFileFormat
{
	/// Human-readable format label.
	std::string name;

	/// Canonical lowercase extensions without leading dots.
	std::vector<std::string> extensions;
};



//////
//
// Classes
//

/// \brief Extension point for synchronous encoded-byte decoding. \ingroup fcg_image_loading
class FCG_IMAGE_EXPORT ImageFormatHandler
{
public:

	////
	// Object construction/destruction

	/// Allow destruction through an owning base pointer.
	virtual ~ImageFormatHandler () = default;


	////
	// Methods

	/// Advertised formats as an owning snapshot. Existing handlers default to an empty list.
	///
	/// \note Advertising a format does not guarantee successful decoding or runtime codec availability.
	///
	/// \return Format metadata, independent of handler lifetime.
	[[nodiscard]] virtual auto fileFormats () const -> std::vector<ImageFileFormat> { return {}; }

	/// Inspect encoded bytes without retaining or changing them. Acceptance need not imply decode success.
	///
	/// \param bytes Encoded data, borrowed during the call. \param hint Normalized extension, possibly empty.
	///
	/// \returns Whether decoding should be attempted, or a recoverable probe failure.
	[[nodiscard]] virtual auto accepts (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<bool, ImageError> = 0;

	/// Decode borrowed bytes synchronously into an independently owned surface.
	///
	/// \param bytes Encoded data, borrowed during the call. \param hint Extension hint; never retained.
	///
	/// \returns A decoded `Image` or an owned error. Implementations may be called without a preceding `accepts()`.
	[[nodiscard]] virtual auto load (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<Image, ImageError> = 0;

	/// Read a file once, derive its extension hint, and invoke the byte decoder directly.
	/// \anchor image_loading_handler_file_load
	///
	/// \param path Native filesystem path. \returns A decoded image or input/I/O/decoder failure.
	[[nodiscard]] auto load (const std::filesystem::path &path) const -> std::expected<Image, ImageError>;
};

/// \brief Owned format-handler registry with ordered fallback. \ingroup fcg_image_loading
class FCG_IMAGE_EXPORT ImageLoader
{
public:

	////
	// Object construction/destruction

	/// Construct an empty registry; no builtin handlers are installed in independent instances.
	ImageLoader () = default;

	/// Destroy all owned handlers. Must not overlap a load or other registry operation.
	~ImageLoader () = default;

	/// Registries own handlers and cannot be copied.
	ImageLoader (const ImageLoader&) = delete;

	/// Registries cannot be copy-assigned.
	auto operator= (const ImageLoader&) -> ImageLoader& = delete;

	/// Registry identity is stable; loaders cannot be moved.
	ImageLoader (ImageLoader&&) = delete;

	/// Loaders cannot be move-assigned.
	auto operator= (ImageLoader&&) -> ImageLoader& = delete;


	////
	// Registry

	/// Obtain the process-lifetime singleton. Full automatic registration completes after static initialization.
	///
	/// \returns The shared registry; serialize subsequent operations externally.
	[[nodiscard]] static auto global () -> ImageLoader&;

	/// Transfer a handler into this registry, ordered by descending priority and ascending ID.
	///
	/// \param id Unique nonempty ID, copied/owned by the registry; embedded NULs are invalid.
	/// \param handler Non-null owner, consumed even if registration fails. \param priority Higher values run first.
	///
	/// \returns Success, `InvalidArgument`, or `DuplicateHandler`. Allocation exceptions may propagate.
	[[nodiscard]] auto registerHandler (std::string id, std::unique_ptr<ImageFormatHandler> handler, int priority=0)
		-> std::expected<void, ImageError>;

	/// Remove and destroy a handler by ID. Must not be called from this loader's callbacks.
	///
	/// \param id Registered ID. \returns Whether an entry was removed.
	auto removeHandler (std::string_view id) -> bool;

	/// Inspect registry size without exposing borrowed handler objects.
	///
	/// \returns The number of owned handlers.
	[[nodiscard]] auto handlerCount () const -> std::size_t { return m_handlers.size(); }

	/// Collect owning format snapshots in registry order, retaining overlaps and repeated entries.
	///
	/// \note Serialize with other registry operations; handlers must not modify this loader during the call.
	///
	/// \return Advertised formats, independent of subsequent registration or removal.
	[[nodiscard]] auto fileFormats () const -> std::vector<ImageFileFormat>;


	////
	// Loading

	/// Read a file once and dispatch its bytes to accepting handlers in registry order.
	///
	/// \param path Native path; its extension is advisory. \returns A decoded image or input/I/O/dispatch failure.
	[[nodiscard]] auto load (const std::filesystem::path &path) const -> std::expected<Image, ImageError>;

	/// Try accepting handlers until one succeeds. Input is not retained and no implicit conversion occurs.
	///
	/// \param bytes
	/// 	Nonempty encoded image. \param hint Optional extension, with optional leading dot; case-insensitive.
	///
	/// \returns An image, `UnsupportedFormat` if all decline, or an ordered aggregate `DecodeFailure`.
	[[nodiscard]] auto load (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<Image, ImageError>;


private:

	////
	// Types

	/// One owned decoder and its deterministic dispatch key.
	struct Entry
	{
		/// Unique ID for diagnostics and equal-priority ordering.
		std::string id;

		/// Owned implementation.
		std::unique_ptr<ImageFormatHandler> handler;

		/// Higher values precede lower ones.
		int priority;
	};


	////
	// Fields

	/// Owned entries in dispatch order. Access is externally serialized.
	std::vector<Entry> m_handlers;
};

/// \brief Namespace-scope registration of an owned handler with `ImageLoader::global()`. \ingroup fcg_image_loading
class FCG_IMAGE_EXPORT FormatHandlerRegistration
{
public:

	////
	// Object construction/destruction

	/// Register immediately. Invalid or duplicate registration throws `std::invalid_argument`; never logs.
	///
	/// \param id Unique nonempty registry ID. \param handler Owner transferred to the singleton.
	/// \param priority Dispatch priority, higher first. Allocation exceptions may propagate.
	FormatHandlerRegistration (std::string id, std::unique_ptr<ImageFormatHandler> handler, int priority=0);

	/// No deregistration occurs; handler ownership belongs to the singleton.
	~FormatHandlerRegistration () = default;

	/// Registration helpers represent one construction-time action and cannot be copied.
	FormatHandlerRegistration (const FormatHandlerRegistration&) = delete;

	/// Helpers cannot be copy-assigned.
	auto operator= (const FormatHandlerRegistration&) -> FormatHandlerRegistration& = delete;
};



//////
//
// Namespaces close
//

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_LOADER_H__
