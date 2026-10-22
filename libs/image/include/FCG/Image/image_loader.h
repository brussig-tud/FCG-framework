
#ifndef __FCG_IMAGE_LOADER_H__
#define __FCG_IMAGE_LOADER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_image_loading Image loading and format handlers
/// \ingroup fcg_modules
/// \brief Format-independent loading of image files and memory blocks through a registry of handlers.
///
/// <tt>\ref fcg::ImageLoader "ImageLoader"</tt> decodes image files into <tt>\ref fcg::Image "Images"</tt> without
/// the caller having to know the format: it asks its registered <tt>\ref fcg::ImageFormatHandler
/// "ImageFormatHandler"</tt>s in order of descending priority until one succeeds. The shipped SDL_image handler
/// covers the formats the SDL_image build provides. Register your own handlers to load formats SDL_image does not
/// know, or to route loading through custom code.
/// Radiance HDR, PFM and OpenEXR files use the built-in handlers in \ref fcg_radiance_hdr, \ref fcg_pfm and, when enabled,
/// \ref fcg_openexr.
///
/// \section fcg_image_loading_usage Getting started
/// \snippet image_examples.cpp includes
/// \snippet image_examples.cpp file
/// Files are read once and dispatched as bytes, with the file extension as a hint for formats that carry no signature
/// (TGA usually needs it; signature-based formats load even when the extension is wrong or missing). The hint is an
/// extension such as \c "png", with an optional leading dot; ASCII case is normalized. Loading from memory works the
/// same way:
/// \snippet image_examples.cpp memory
///
/// \section fcg_image_loading_workflows Common tasks
/// \subsection fcg_image_loading_workflows_registry Choosing a loader instance
/// <tt>\ref fcg::ImageLoader::global "ImageLoader::global()"</tt> is the shared process-wide loader, with the
/// SDL_image, Radiance HDR and PFM handlers pre-registered, plus OpenEXR when enabled. A locally constructed
/// <tt>\ref fcg::ImageLoader "ImageLoader"</tt> starts empty and is useful when a module needs its own set of handlers:
/// \snippet image_examples.cpp independent
/// <tt>\ref fcg::ImageLoader::fileFormats "fileFormats"</tt> lists which formats a loader advertises, e.g. for building
/// file-dialog filters with <tt>\ref fcg::extra::imageFileFilters "imageFileFilters"</tt>.
///
/// \subsection fcg_image_loading_workflows_guessing Loading files with missing color metadata
/// <tt>\ref fcg::ImageLoader::loadAndGuess "ImageLoader::loadAndGuess"</tt> asks the successful handler to infer
/// missing primaries, transfer and range. The inherited <tt>\ref fcg::ImageFormatHandler::loadAndGuess
/// "ImageFormatHandler::loadAndGuess"</tt> provides the same operation on every concrete handler. Each handler uses
/// the input and its format's conventions to choose the values; consult its module guide for the particular policy.
/// The general loading interface prescribes no encoding, and best-effort results can remain incomplete or unsupported
/// by color-managed upload. Ordinary loads leave guessing disabled. Select individual components with
/// <tt>\ref fcg::ImageLoaderOptions "ImageLoaderOptions"</tt>:
/// \snippet hdr_examples.cpp guessing
/// Guessing fills missing source information rather than replacing declarations. A handler can also convert colors
/// while decoding; in that case the returned pixels and metadata describe the conversion result. For example,
/// \ref fcg_radiance_hdr converts numerically defined unsupported gamuts to linear BT.709.
/// Inspect <tt>\ref fcg::ImagePixelInfo::loadAdjustments "ImagePixelInfo::loadAdjustments"</tt> through
/// <tt>\ref fcg::Image::pixelInfo "Image::pixelInfo"</tt> to see which properties were guessed or adapted.
/// A guessed source property used for adaptation is marked with
/// <tt>\ref fcg::ImageLoadAdjustment::GuessedAndAdapted "ImageLoadAdjustment::GuessedAndAdapted"</tt>.
/// See \ref fcg_pixel_formats_workflows_loading for an example and the annotation lifetime rules.
///
/// \subsection fcg_image_loading_workflows_handlers Adding a format handler
/// Implement <tt>\ref fcg::ImageFormatHandler "ImageFormatHandler"</tt> and register it on a loader:
/// \snippet image_examples.cpp registration
/// \c accepts decides whether a load attempt is worth trying, and \c load performs the decoding. Registration transfers
/// ownership of the handler to the loader.
///
/// \section fcg_image_loading_lifetime Ownership and lifetime
/// <tt>\ref fcg::ImageLoader::registerHandler "registerHandler"</tt> takes ownership; removing a handler or destroying
/// the loader destroys it. Images do not borrow their decoder. <tt>\ref fcg::ImageLoader::global "global()"</tt> is
/// constructed on first use and lives until process end; its handlers remain registered until removed explicitly. Avoid
/// unloading a shared library while its handler is still registered. Registry operations and loads are not synchronized
/// internally; serialize them externally. Static-archive clients should see the note in
/// <tt>\ref fcg::FormatHandlerRegistration "FormatHandlerRegistration"</tt>.
///
/// \section fcg_image_loading_errors Handling errors
/// Loads return <tt>std::expected&lt;\ref fcg::Image "Image", \ref fcg::ImageError "ImageError"&gt;</tt>:
/// <tt>\ref fcg::ImageErrorCode::UnsupportedFormat "UnsupportedFormat"</tt> means no handler accepted the input, and
/// <tt>\ref fcg::ImageErrorCode::DecodeFailure "DecodeFailure"</tt> aggregates the diagnostics of every handler that
/// tried, in attempt order. Nothing logs automatically. Custom handlers should return recoverable failures as
/// <tt>std::expected</tt>; exceptions escaping a handler abort the load.
///
/// \see \ref fcg_images, \ref fcg_sdl_image



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

/// \addtogroup fcg_image_loading
/// @{



//////
//
// Structs and enums
//

/// \brief Requests best-effort inference of missing RGB metadata; every request is disabled by default.
///
/// The handler that successfully decodes the input chooses values using available source information and conventions
/// of its format. The values are not prescribed by this type and need not be known before loading. Enabled fields
/// concern only unknown or unspecified source components on RGB and palette images; identified components, including
/// custom encodings, take precedence. A handler may leave a component unidentified if it has no defensible inference.
/// YUV interpretation is not guessed. These requests do not guarantee an encoding supported by color-managed upload.
///
/// Handlers may convert colors while decoding and report the resulting encoding; consult the handler's contract for
/// sample preservation. \c ImageFormatHandler::loadAndGuess and \c ImageLoader::loadAndGuess enable every field.
struct ImageLoaderOptions
{
	/// Ask the successful handler to infer unidentified source primaries from the input and format conventions.
	bool guessColorPrimaries = false;

	/// Ask the successful handler to infer unidentified source transfer from the input and format conventions.
	bool guessTransferCharacteristics = false;

	/// Ask the successful handler to infer unidentified source range from the input and format conventions.
	bool guessColorRange = false;
};

/// \brief
/// 	Describes an image file format that a handler advertises.
///
/// Use these records to build file-dialog filters, e.g. with \c fcg::extra::imageFileFilters from the Extras
/// library. An advertised format is not a guarantee that every file of that format can be decoded.
struct ImageFileFormat
{
	/// Human-readable name of the format, e.g. <tt>"PNG"</tt>.
	std::string name;

	/// \brief
	/// 	File name extensions of the format, lowercase and without a leading dot, e.g. <tt>"jpg"</tt>, <tt>"jpeg"</tt>.
	///
	/// A single \c "*" may be used to provide a catch-all file-dialog filter.
	std::vector<std::string> extensions;
};



//////
//
// Classes
//

/// \brief
/// 	Interface for decoders that turn encoded image files into an \c Image.
///
/// An \c ImageLoader tries its registered handlers in turn; a handler can also be used on its own. To add support for
/// a file format:
/// 1. Derive from \c ImageFormatHandler.
/// 2. Implement \c accepts to check cheaply, e.g. from a file signature or the hint, whether decoding should be
///    attempted.
/// 3. Implement the byte overload of \c load to decode the data into a new \c SDL_Surface, wrap it with
///    \c Image::adopt, then optionally apply handler-chosen guesses with \c finishLoading after extracting metadata.
///    Document any color conversion performed during decoding and report the resulting encoding. Record
///    completed guessing and adaptation actions with the fourth argument of \c finishLoading. Add
///    <tt>using ImageFormatHandler::load;</tt> to your class to keep the file overload of \c load available.
/// 4. Optionally override \c fileFormats to advertise the formats for file dialogs.
/// 5. Register an instance with \c ImageLoader::registerHandler, or at namespace scope with
///    \c FormatHandlerRegistration.
///
/// Both \c accepts and \c load are synchronous. They receive the encoded bytes and the hint only for the duration of
/// the call and must not keep references to them. Report recoverable failures through the returned
/// \c std::expected; exceptions propagate to the caller of \c ImageLoader::load unchanged. A handler must not register
/// or remove handlers of the loader that is currently calling it. Calls through one loader are serialized by its
/// users, but a handler that shares state between several loaders must synchronize that state itself.
class FCG_IMAGE_EXPORT ImageFormatHandler
{
public:

	////
	// Object construction/destruction

	/// Destroys the handler. Handlers are owned and destroyed through \c ImageFormatHandler pointers.
	virtual ~ImageFormatHandler () = default;


	////
	// Methods

	/// \brief
	/// 	Returns the image file formats this handler decodes, for display in file dialogs.
	///
	/// The default implementation returns an empty list.
	///
	/// \note
	/// 	Advertising a format does not guarantee that decoding succeeds, e.g. when a codec library is unavailable at
	/// 	runtime.
	///
	/// \return The advertised formats.
	[[nodiscard]] virtual auto fileFormats () const -> std::vector<ImageFileFormat> { return {}; }

	/// \brief
	/// 	Checks whether this handler should attempt to decode the given data.
	///
	/// A result of \c true permits a call to \c load but does not guarantee that decoding succeeds. Return an error
	/// only if the check itself could not be carried out; \c ImageLoader records the error and continues with the
	/// next handler.
	///
	/// \note
	/// 	\c ImageLoader always passes a normalized hint: the leading dot is removed and ASCII letters are lowercase,
	/// 	e.g. <tt>"png"</tt> for a file named <tt>photo.PNG</tt>. The hint is empty if the extension is unknown.
	/// 	Direct callers may pass hints that are not normalized.
	///
	/// \param bytes
	/// 	The encoded file contents, valid only during the call.
	/// \param hint
	/// 	The file name extension of the input, or empty if unknown.
	///
	/// \return \c true if decoding should be attempted, \c false if not, or an error if the check failed.
	[[nodiscard]] virtual auto accepts (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<bool, ImageError> = 0;

	/// \brief
	/// 	Decodes encoded file contents into an image.
	///
	/// \c ImageLoader calls this only after \c accepts returned \c true, but direct callers, including the file
	/// overload of \c load, may call it without calling \c accepts first. The returned image does not depend on
	/// \c bytes after the call.
	/// Implementations extract available metadata before applying \c options. Infer only missing RGB components using
	/// input context and format conventions, for example by passing chosen candidates to \c finishLoading. Identified
	/// source components take precedence. Implementations that convert colors must report the resulting encoding.
	/// Implementations record guesses and loading adaptations in \c ImagePixelInfo::loadAdjustments. The helper
	/// \c finishLoading records guesses it applies and accepts actions already performed by the handler.
	/// The inherited \c loadAndGuess overloads call this operation with every inference request enabled.
	///
	/// \note
	/// 	Implementations return a non-empty image on success; \c ImageLoader treats an empty image as a failure. The
	/// 	hint is normalized as described for \c accepts when the call comes from \c ImageLoader or the file overload
	/// 	of \c load.
	///
	/// \param bytes
	/// 	The encoded file contents, valid only during the call.
	/// \param hint
	/// 	The file name extension of the input, or empty if unknown.
	/// \param options Requests to infer missing RGB metadata; \c loadAndGuess enables every request.
	///
	/// \return The decoded image, or an error describing why decoding failed.
	[[nodiscard]] virtual auto load (
		std::span<const std::byte> bytes, std::string_view hint={}, ImageLoaderOptions options={}
	) const -> std::expected<Image, ImageError> = 0;

	/// \brief
	/// 	Reads a file completely and decodes it with this handler alone.
	///
	/// \anchor image_loading_handler_file_load
	/// The extension of \c path is normalized and passed to the byte overload of \c load as the hint. \c accepts is not
	/// consulted.
	///
	/// \param path
	/// 	The path of the image file.
	/// \param options Requests to infer missing RGB metadata; \c loadAndGuess enables every request.
	///
	/// \return
	/// 	The decoded image, \c ImageErrorCode::InvalidArgument if the path is empty or contains a NUL character
	/// 	or the file is empty, \c ImageErrorCode::IOFailure if the file cannot be opened or read, or the error
	/// 	returned by the byte overload of \c load.
	[[nodiscard]] auto load (const std::filesystem::path &path, ImageLoaderOptions options={}) const
		-> std::expected<Image, ImageError>;

	/// \brief Decode memory while enabling every missing-metadata inference request in \c ImageLoaderOptions.
	///
	/// Identified source components take precedence over guesses. The handler may convert colors as described by
	/// its loading contract. See \c load for errors and ownership.
	///
	/// \param bytes Encoded input borrowed during this call.
	/// \param hint Optional file extension hint.
	///
	/// \return The decoded image with best-effort metadata, or an error from \c load.
	[[nodiscard]] virtual auto loadAndGuess (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<Image, ImageError> final;

	/// \brief Read a file with every missing-metadata inference request enabled; see \c loadAndGuess for memory input.
	///
	/// \param path The image file path.
	///
	/// \return The independently owned decoded image, or an error from \c load.
	[[nodiscard]] virtual auto loadAndGuess (const std::filesystem::path &path) const
		-> std::expected<Image, ImageError> final;


protected:

	////
	// Methods

	/// \brief Apply handler-chosen candidates to requested missing components after decoding.
	///
	/// Only unknown or unspecified RGB components enabled in \p options are filled. Identified components, including
	/// custom encodings, and YUV surfaces are retained. Pixel samples are retained. The caller chooses candidates from
	/// the input and format conventions; this helper supplies no default guesses or color conversion.
	/// Newly filled components are annotated as \c ImageLoadAdjustment::Guessed. Previously recorded matching actions
	/// are retained and combined with \p adjustments. Report both actions when an adaptation used guessed source data.
	///
	/// \param image Decoding result whose image is consumed on success; errors pass through unchanged.
	/// \param options Inference requests to apply only to missing RGB components.
	/// \param guesses RGB encoding containing the handler's candidates; unresolved components may remain unspecified.
	/// \param adjustments Actions already completed by this handler, describing the image's returned color properties.
	///
	/// \return
	/// 	The image with updated metadata, the decoding error, or \c ImageErrorCode::SDLFailure if metadata cannot
	/// 	be read or updated.
	[[nodiscard]] static auto finishLoading (
		std::expected<Image, ImageError> image, ImageLoaderOptions options, SDL_Colorspace guesses,
		ImageLoadAdjustments adjustments={}
	) -> std::expected<Image, ImageError>;
};

/// \brief
/// 	A registry of image format handlers that loads image files by trying the handlers in order.
///
/// Most applications use the process-wide loader returned by \c global, which can decode the common image formats:
/// \snippet image_examples.cpp quick_load
/// Construct your own \c ImageLoader to control exactly which handlers are used; a new loader has no handlers.
///
/// Handlers are tried in order of descending priority, with equal priorities ordered by ascending ID. For each
/// handler, the loader calls \c ImageFormatHandler::accepts and, if it returns \c true, \c ImageFormatHandler::load.
/// The first successfully decoded image is returned. Failed checks and failed decoding attempts are recorded, and
/// the loader continues with the next handler. Every call starts afresh; nothing is cached.
///
/// An \c ImageLoader is not thread-safe: all operations on one loader, including loading, must be externally
/// serialized. Handlers must not register or remove handlers of the loader that calls them. Exceptions thrown by
/// handlers propagate to the caller.
class FCG_IMAGE_EXPORT ImageLoader
{
public:

	////
	// Object construction/destruction

	/// Creates a loader without any handlers.
	ImageLoader () = default;

	/// \brief
	/// 	Destroys the loader and all its handlers.
	///
	/// \pre No other operation on this loader is in progress.
	~ImageLoader () = default;

	/// Not copyable.
	ImageLoader (const ImageLoader&) = delete;

	/// Not copy-assignable.
	auto operator= (const ImageLoader&) -> ImageLoader& = delete;

	/// Not movable.
	ImageLoader (ImageLoader&&) = delete;

	/// Not move-assignable.
	auto operator= (ImageLoader&&) -> ImageLoader& = delete;


	////
	// Registry

	/// \brief
	/// 	Returns the process-wide loader.
	///
	/// The loader comes with an \c SDLImageFormatHandler registered under the ID <tt>"sdl_image"</tt> with priority
	/// -100, so handlers registered with the default priority 0 are tried first. Handlers registered through
	/// \c FormatHandlerRegistration are added to this loader as well. The loader is created on the first call and is
	/// never destroyed, so it stays usable until the process ends.
	/// The \c RadianceHDRFormatHandler and \c PFMImageFormatHandler use IDs <tt>"radiance_hdr"</tt> and
	/// <tt>"pfm"</tt> with priority -90. When enabled, \c OpenEXRImageFormatHandler uses <tt>"openexr"</tt>
	/// at priority -90. Signature-based HDR handlers precede the SDL_image handler.
	///
	/// Creating the loader on first use is thread-safe. All operations on the returned loader must be externally
	/// serialized like for any other \c ImageLoader.
	///
	/// \note
	/// 	When called during static initialization, e.g. from the constructor of a global object, handlers registered
	/// 	by other translation units, including the SDL_image handler, may not have been registered yet.
	///
	/// \return The process-wide loader.
	[[nodiscard]] static auto global () -> ImageLoader&;

	/// \brief
	/// 	Adds a handler to this loader and takes ownership of it.
	///
	/// The handler is placed according to its priority and ID (see the class description). It is destroyed when it is
	/// removed or when the loader is destroyed. The handler is consumed even if registration fails. If an exception,
	/// e.g. \c std::bad_alloc, is thrown, the registry remains unchanged.
	///
	/// \pre Not called from within a handler call made by this loader.
	///
	/// \param id
	/// 	Unique, non-empty name of the handler without NUL characters. It identifies the handler in error messages
	/// 	and for \c removeHandler.
	/// \param handler
	/// 	The handler; must not be null.
	/// \param priority
	/// 	Handlers with higher priority are tried first.
	///
	/// \return
	/// 	Nothing on success, \c ImageErrorCode::InvalidArgument if \c id is empty or contains a NUL character or
	/// 	\c handler is null, or \c ImageErrorCode::DuplicateHandler if a handler with this ID is already registered.
	[[nodiscard]] auto registerHandler (std::string id, std::unique_ptr<ImageFormatHandler> handler, int priority=0)
		-> std::expected<void, ImageError>;

	/// \brief
	/// 	Removes the handler registered under the given ID and destroys it.
	///
	/// \pre Not called from within a handler call made by this loader.
	///
	/// \param id
	/// 	The ID the handler was registered with.
	///
	/// \return \c true if a handler was removed, \c false if no handler has this ID.
	auto removeHandler (std::string_view id) -> bool;

	/// Returns the number of registered handlers.
	[[nodiscard]] auto handlerCount () const -> std::size_t { return m_handlers.size(); }

	/// \brief
	/// 	Returns the formats advertised by all registered handlers.
	///
	/// The result concatenates the \c ImageFormatHandler::fileFormats lists of all handlers in the order in which the
	/// handlers are tried. Formats advertised by several handlers appear repeatedly.
	///
	/// \return The advertised formats.
	[[nodiscard]] auto fileFormats () const -> std::vector<ImageFileFormat>;


	////
	// Loading

	/// \brief
	/// 	Reads an image file completely and decodes it with the registered handlers.
	///
	/// The file's extension is passed to the handlers as the hint, as described for the byte overload of \c load.
	///
	/// \param path
	/// 	The path of the image file.
	/// \param options Requests to infer missing RGB metadata; \c loadAndGuess enables every request.
	///
	/// \return
	/// 	The decoded image, \c ImageErrorCode::InvalidArgument if the path is empty or contains a NUL character
	/// 	or the file is empty, \c ImageErrorCode::IOFailure if the file cannot be opened or read, or any error of
	/// 	the byte overload of \c load.
	[[nodiscard]] auto load (const std::filesystem::path &path, ImageLoaderOptions options={}) const
		-> std::expected<Image, ImageError>;

	/// \brief
	/// 	Decodes an image from encoded file contents in memory with the registered handlers.
	///
	/// Handlers are tried as described in the class description. The input is not modified or retained, and the
	/// decoded image is returned in the pixel format produced by the handler.
	///
	/// \param bytes
	/// 	The encoded file contents, e.g. read from a file or an embedded resource. They only need to stay valid
	/// 	during the call.
	/// \param hint
	/// 	The file name extension of the input, e.g. <tt>"png"</tt>, or empty if unknown. A leading dot and ASCII
	/// 	letter case are ignored. Formats with a file signature load without a hint; the SDL_image handler needs it
	/// 	for TGA files.
	/// \param options Requests to infer missing RGB metadata; \c loadAndGuess enables every request.
	///
	/// \return
	/// 	The decoded image, or an error:
	/// 	- \c ImageErrorCode::InvalidArgument if \c bytes is empty or \c hint contains a NUL character;
	/// 	- \c ImageErrorCode::UnsupportedFormat if no handler accepted the input, including when no handler is
	/// 	  registered;
	/// 	- \c ImageErrorCode::DecodeFailure if at least one handler failed and none succeeded. The message contains
	/// 	  one line per failed handler in the order they were tried, formatted as <tt>id: message</tt>, or
	/// 	  <tt>id (probe): message</tt> if the handler's \c accepts check failed.
	[[nodiscard]] auto load (
		std::span<const std::byte> bytes, std::string_view hint={}, ImageLoaderOptions options={}
	) const -> std::expected<Image, ImageError>;

	/// \brief
	/// 	Read a file with every missing-metadata inference request enabled; see \c ImageFormatHandler::loadAndGuess.
	///
	/// \param path The image file path.
	///
	/// \return The decoded image with best-effort metadata, or an error from \c load.
	[[nodiscard]] auto loadAndGuess (const std::filesystem::path &path) const -> std::expected<Image, ImageError>;

	/// \brief
	/// 	Decode memory with every missing-metadata inference request enabled; see \c ImageFormatHandler::loadAndGuess.
	///
	/// \param bytes Encoded input borrowed during this call.
	/// \param hint Optional file extension hint.
	///
	/// \return The decoded image with best-effort metadata, or an error from \c load.
	[[nodiscard]] auto loadAndGuess (std::span<const std::byte> bytes, std::string_view hint={}) const -> std::expected<Image, ImageError>;


private:

	////
	// Types

	/// One registered handler together with the key that determines its position in the dispatch order.
	struct Entry
	{
		/// Unique ID, used in diagnostics and for ordering handlers of equal priority.
		std::string id;

		/// The owned handler.
		std::unique_ptr<ImageFormatHandler> handler;

		/// Dispatch priority; higher values are tried first.
		int priority;
	};


	////
	// Fields

	/// Registered handlers in dispatch order. Access is externally serialized.
	std::vector<Entry> m_handlers;
};

/// \brief
/// 	Registers a handler with \c ImageLoader::global from a namespace-scope variable.
///
/// Define a registration object at namespace scope in the translation unit that implements the handler. The handler
/// is registered during static initialization, before \c main runs:
/// \snippet image_examples.cpp registration
/// The registration is permanent: destroying the object does not remove the handler.
///
/// \note
/// 	If the translation unit is part of a static library and nothing else in it is referenced by the program, the
/// 	linker may leave it out, and the handler is never registered. Reference a symbol from that translation unit,
/// 	link it as an object library, or use whole-archive linking.
class FCG_IMAGE_EXPORT FormatHandlerRegistration
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Registers a handler with \c ImageLoader::global.
	///
	/// An exception thrown from the initializer of a namespace-scope variable terminates the program.
	///
	/// \param id
	/// 	Unique, non-empty name of the handler without NUL characters.
	/// \param handler
	/// 	The handler, whose ownership is transferred to the global loader; must not be null.
	/// \param priority
	/// 	Handlers with higher priority are tried first.
	///
	/// \throws std::invalid_argument
	/// 	If \c id is empty, contains a NUL character or is already registered, or if \c handler is null. The
	/// 	exception message describes the problem.
	FormatHandlerRegistration (std::string id, std::unique_ptr<ImageFormatHandler> handler, int priority=0);

	/// Destroys the registration object; the handler stays registered.
	~FormatHandlerRegistration () = default;

	/// Not copyable.
	FormatHandlerRegistration (const FormatHandlerRegistration&) = delete;

	/// Not copy-assignable.
	auto operator= (const FormatHandlerRegistration&) -> FormatHandlerRegistration& = delete;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_LOADER_H__
