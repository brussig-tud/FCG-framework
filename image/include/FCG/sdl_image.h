#ifndef __FCG_SDL_IMAGE_H__
#define __FCG_SDL_IMAGE_H__


//////
//
// Module documentation
//

/// \defgroup fcg_sdl_image SDL_image backend
/// \ingroup fcg_components
/// \brief The framework's sole concrete image format handler, backed by SDL3_image.
///
/// \section sdl_image_workflows Direct and registry-based loading
/// SDLImageFormatHandler implements \ref fcg::ImageFormatHandler for all formats provided by the linked SDL_image.
/// It is a small stateless wrapper: it borrows encoded bytes through SDL_IOFromConstMem, delegates decoding to
/// IMG_LoadTyped_IO, and adopts the returned surface. The SDL_image header is private to the implementation;
/// clients need only SDL3 surface headers. Use the wrapper directly or add an instance to a local loader.
/// \snippet image_examples.cpp direct
///
/// \section sdl_image_formats Detection and codec availability
/// Acceptance uses SDL_image's IMG_is* probes on a seekable memory stream. Each probe starts at offset zero; no
/// input bytes are copied or retained. Formats with signatures ignore misleading hints. A recognized extension also
/// permits an attempt, particularly TGA, which has no reliable signature probe. A hint is advisory rather than a
/// forced codec selection. Probes and hinted acceptance are not a guarantee that the linked build can decode the
/// data: optional codecs can be disabled or unavailable at runtime. Consult \ref image_library_build to enable them.
/// Built-in formats include PNG, JPEG, BMP, GIF, TGA, QOI, and several other SDL_image codecs. External codec formats
/// are disabled by default in fetched builds; system packages keep their own capabilities. BMP support is supplied
/// by this same handler; the framework ships no additional handler based on SDL_LoadBMP or SDL_LoadPNG.
///
/// \section sdl_image_lifetime Ownership and initialization
/// The wrapper owns no SDL initialization state, cache, GPU resource, or borrowed input. No IMG_Init/IMG_Quit calls
/// exist in SDL_image 3. Construction does no decoding and is safe during static registration. Destruction releases
/// no library-global state. Every operation owns its temporary stream, closes it exactly once, and keeps returned
/// surfaces in RAII owners until adoption succeeds. Images remain usable after the handler or input bytes die.
/// Externally serialize calls when sharing an instance; independently constructed wrappers can be used independently,
/// subject to the linked codecs' concurrency contracts. No video initialization is needed for these CPU loads.
///
/// \section sdl_image_registration Shipped registration
/// A namespace-scope helper in the backend's translation unit registers ID "sdl_image" with priority -100 in
/// ImageLoader::global(). The singleton implementation references that translation unit through a link anchor.
/// Merely linking Image and using global() retains automatic registration in static and shared builds. Independent
/// loaders remain empty until explicit registration. The usual static-initialization ordering restrictions apply;
/// see \ref image_loading_registration before loading from a global constructor.
///
/// \section sdl_image_errors Backend failures
/// Empty encoded input and embedded NULs in hints produce InvalidArgument. Stream allocation/seeking/probing
/// infrastructure failures produce SDLFailure. IMG_LoadTyped_IO failures produce DecodeFailure with SDL_GetError
/// captured before stream/surface cleanup. A malformed image may pass detection and fail decoding. Through
/// ImageLoader, these failures permit later handlers to run and contribute to the aggregate error if none succeeds.
/// Direct load(path) shares the same file reader as ImageLoader and adds its input/I/O errors. No failures are logged.


//////
//
// Includes
//

// Local includes
#include "FCG/image_loader.h"


//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {


//////
//
// Classes
//

/// \brief Stateless SDL3_image decoder preserving its returned surface representation. \ingroup fcg_sdl_image
class FCG_IMAGE_EXPORT SDLImageFormatHandler final : public ImageFormatHandler
{
public:

	////
	// Object construction/destruction

	/// Construct without initializing SDL or allocating codec resources.
	SDLImageFormatHandler () = default;


	////
	// Methods

	/// Retain the base filesystem convenience overload alongside the byte decoder.
	using ImageFormatHandler::load;

	/// Probe signatures or a recognized hint; a positive result is permission to try decoding.
	/// \param bytes Encoded image, borrowed only during this call. \param hint Optional extension; case-insensitive.
	/// \returns Acceptance or an input/SDL stream error; never retains input or a decoded surface.
	[[nodiscard]] auto accepts (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<bool, ImageError> override;

	/// Decode to an owned SDL surface with native format, pitch, and transparency metadata.
	/// \param bytes Nonempty encoded data. \param hint Optional extension, particularly for TGA; never retained.
	/// \returns An Image or an input/SDL/decode failure. Input may be released on return.
	[[nodiscard]] auto load (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<Image, ImageError> override;
};


//////
//
// Namespaces close
//

// namespace fcg
}


#endif // ifndef __FCG_SDL_IMAGE_H__
