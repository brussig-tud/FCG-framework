
#ifndef __FCG_SDL_IMAGE_H__
#define __FCG_SDL_IMAGE_H__


//////
//
// Module documentation
//

/// \defgroup fcg_sdl_image SDL_image backend
/// \ingroup fcg_modules
/// \brief Decodes the still-image formats enabled in SDL3_image.
///
/// <tt>\ref fcg::SDLImageFormatHandler "SDLImageFormatHandler"</tt> implements
/// <tt>\ref fcg::ImageFormatHandler "ImageFormatHandler"</tt> for every still-image format that the linked SDL_image
/// build provides — typically PNG, JPEG, BMP, GIF, TGA and QOI; <tt>\ref fcg::ImageFormatHandler::fileFormats
/// "fileFormats"</tt> lists them. It is registered on <tt>\ref fcg::ImageLoader::global
/// "ImageLoader::global"</tt>; use the handler directly when the backend choice should be explicit.
/// Radiance HDR, PFM and OpenEXR use the handlers described in \ref fcg_radiance_hdr,
/// \ref fcg_pfm and \ref fcg_openexr.
///
/// \section fcg_sdl_image_usage Getting started
/// \snippet image_examples.cpp direct
/// From files containing several frames, such as animated GIFs, a single image is decoded.
///
/// \section fcg_sdl_image_workflows Common tasks
/// \subsection fcg_sdl_image_workflows_formats Knowing which formats are available
/// Format detection uses SDL_image's content probes, so signature-based formats load even with a wrong filename.
/// Extension-only formats such as TGA need a hint. Probes and hints are not a guarantee that decoding succeeds:
/// optional codecs can be disabled in the linked SDL_image build. Inspect
/// <tt>\ref fcg::ImageFormatHandler::fileFormats "fileFormats"</tt> for the advertised formats.
///
/// \subsection fcg_sdl_image_workflows_enabling Enabling additional formats
/// Source builds provide common formats such as PNG and JPEG by default. Formats needing external codecs, such as
/// WebP, AVIF and TIFF, are disabled by default. Enable them with SDL_image's CMake options before adding the
/// framework. For example, to load WebP with installed codec dependencies:
/// \code{.cmake}
/// set(SDLIMAGE_WEBP ON CACHE BOOL "Enable WebP decoding")
/// set(SDLIMAGE_STRICT ON CACHE BOOL "Require requested codec dependencies")
/// CPMAddPackage(NAME FCG SOURCE_DIR "/path/to/framework")
/// target_link_libraries(my_app PRIVATE FCG-Framework::Image)
/// \endcode
/// Replace the source path with your framework checkout, or use your project's pinned CPM repository and revision.
/// Choose the option for the format you need; each option below defaults to OFF in source builds:
///
/// | Format or decoder | CMake option |
/// | --- | --- |
/// | WebP | <tt>SDLIMAGE_WEBP</tt> |
/// | AVIF | <tt>SDLIMAGE_AVIF</tt> |
/// | TIFF | <tt>SDLIMAGE_TIF</tt> |
/// | JPEG XL | <tt>SDLIMAGE_JXL</tt> |
/// | PNG through libpng | <tt>SDLIMAGE_PNG_LIBPNG</tt> |
///
/// PNG decoding is already available without libpng. <tt>SDLIMAGE_STRICT</tt> makes missing codec dependencies a
/// configuration error. To build enabled codec dependencies from SDL_image's bundled sources, also set
/// <tt>SDLIMAGE_VENDORED</tt> to ON. These options can be normal CMake variables, cache entries, or command-line
/// <tt>-D</tt> settings.
///
/// Supplied or installed SDL_image packages retain the formats they were built with; the options above apply to
/// source builds. <tt>\ref fcg::ImageFormatHandler::fileFormats "fileFormats"</tt> lists the resulting capabilities.
/// For advanced build configuration, see the
/// <a href="https://github.com/brussig-tud/FCG-framework/blob/master/cmake/DEPENDENCIES.md#sdl_image-codec-configuration">dependency reference</a>.
///
/// \subsection fcg_sdl_image_workflows_registering Registering the handler on other loaders
/// The global loader comes with an instance of this handler registered under the ID <tt>"sdl_image"</tt> with
/// priority -100. Independent <tt>\ref fcg::ImageLoader "ImageLoader"</tt>s start empty; register your own instance
/// of the handler on them as with any other handler.
///
/// Missing RGB metadata can be filled with the inherited
/// <tt>\ref fcg::ImageFormatHandler::load "ImageFormatHandler::load"</tt>, or selected individually
/// through <tt>\ref fcg::ImageLoaderOptions "ImageLoaderOptions"</tt>. Every component supplied by SDL_image is
/// retained, including its default labels. For missing primaries, an identified PQ or HLG transfer suggests BT.2020;
/// otherwise this handler chooses BT.709. Missing transfer uses a QOI header's linear/sRGB flag or the BT.709 curve
/// for PGM/PPM; remaining formats use linear for floating output and sRGB for integer or palette output. Missing range
/// is filled with full RGB range. Guessing retains the decoded samples.
/// <tt>\ref fcg::ImagePixelInfo::provenance "ImagePixelInfo::provenance"</tt> records SDL_image's supplied color and
/// brightness values, including its defaults, as declarations. Candidates taken from inspected QOI headers are also
/// declared; PGM/PPM transfer is format-defined.
/// Other completed components are assumptions. Eligible linear float output also receives an assumed missing SDR
/// white level and measured missing headroom. Supplied brightness properties are retained. Use
/// <tt>\ref fcg::ImageLoaderOptions::conservative "ImageLoaderOptions::conservative"</tt> to disable optional
/// assumptions and scans while retaining available source and backend information.
///
/// \section fcg_sdl_image_lifetime Ownership and lifetime
/// The handler owns no initialization state, cache, or GPU resource, and no SDL video initialization is needed for
/// these CPU loads. Construction does no decoding and is safe during static registration. Images stay usable after
/// the handler or the input bytes are gone. Serialize calls that share one instance; independent instances can be
/// used independently.
///
/// \section fcg_sdl_image_errors Handling errors
/// Empty encoded input and embedded NULs in hints produce
/// <tt>\ref fcg::ImageErrorCode::InvalidArgument "InvalidArgument"</tt>, stream infrastructure failures produce
/// <tt>\ref fcg::ImageErrorCode::SDLFailure "SDLFailure"</tt>, and undecodable data produces
/// <tt>\ref fcg::ImageErrorCode::DecodeFailure "DecodeFailure"</tt> with SDL's diagnostics in the message. A
/// malformed image may pass detection and then fail decoding. Nothing logs.
///
/// \see \ref fcg_images, \ref fcg_image_loading



//////
//
// Includes
//

// C++ STL
/* nothing here yet */

// Local includes
#include "FCG/Image/image_loader.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_sdl_image
/// @{



//////
//
// Classes
//

/// \brief
/// 	Image format handler that decodes images with SDL3_image.
///
/// The handler decodes every still-image format enabled in the linked SDL_image build, typically including PNG,
/// JPEG, BMP, GIF, TGA and QOI; \c fileFormats lists them. From files containing several frames, such as animated
/// GIFs, a single image is decoded. Decoded images keep the pixel format, pitch, palette and color key produced by
/// SDL_image.
///
/// \c ImageLoader::global comes with an instance of this handler registered under the ID <tt>"sdl_image"</tt> with
/// priority -100. You can also call the handler directly or register your own instance with a local
/// \c ImageLoader. Loading does not require SDL video initialization. Calls on a shared instance must
/// be externally synchronized.
class FCG_IMAGE_EXPORT SDLImageFormatHandler final : public ImageFormatHandler
{
public:

	////
	// Object construction/destruction

	/// Creates the handler. This is cheap and safe during static initialization.
	SDLImageFormatHandler () = default;


	////
	// Interface: ImageFormatHandler

	/// Makes the file overload \c ImageFormatHandler::load available alongside the byte overload below.
	using ImageFormatHandler::load;

	/// \brief
	/// 	Returns the still-image formats enabled in the linked SDL_image build.
	///
	/// \note
	/// 	A listed format can still fail to decode if SDL_image loads its codec library at runtime and that library is
	/// 	unavailable.
	///
	/// \return The supported formats with their names and lowercase extensions.
	[[nodiscard]] auto fileFormats () const -> std::vector<ImageFileFormat> override;

	/// \brief
	/// 	Checks whether the data starts with the signature of a format known to SDL_image, or whether the hint names
	/// 	such a format.
	///
	/// The recognized hints are \c avif, \c bmp, \c cur, \c gif, \c ico, \c iff, \c jpeg, \c jpg, \c jxl, \c lbm,
	/// \c pbm, \c pcx, \c pgm, \c png, \c pnm, \c ppm, \c qoi, \c svg, \c tga, \c tif, \c tiff, \c webp, \c xcf,
	/// \c xpm and \c xv. A recognized hint is accepted even if the linked SDL_image build cannot decode that format.
	///
	/// \param bytes
	/// 	The encoded file contents, valid only during the call; must not be empty.
	/// \param hint
	/// 	The file name extension of the input, or empty if unknown. A leading dot and ASCII letter case are ignored.
	///
	/// \return
	/// 	Whether decoding should be attempted, \c ImageErrorCode::InvalidArgument if \c bytes is empty or \c hint
	/// 	contains a NUL character, or \c ImageErrorCode::SDLFailure if reading the data through an SDL memory stream
	/// 	failed.
	[[nodiscard]] auto accepts (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<bool, ImageError> override;


protected:

	////
	// Interface: ImageFormatHandler

	/// \brief
	/// 	Decodes encoded file contents with SDL_image.
	///
	/// If the data starts with a recognized file signature, it is decoded as that format, whatever the hint says.
	/// Otherwise, SDL_image uses the hint only for formats that have no signature; in particular, TGA files can only
	/// be decoded with the hint \c tga.
	///
	/// \param bytes
	/// 	The encoded file contents, valid only during the call; must not be empty.
	/// \param hint
	/// 	The file name extension of the input, or empty if unknown. A leading dot and ASCII letter case are ignored.
	/// \param options Controls optional assumptions and metadata completion; all requests are enabled by default.
	///
	/// \return
	/// 	The decoded image, or an error:
	/// 	- \c ImageErrorCode::InvalidArgument if \c bytes is empty or \c hint contains a NUL character;
	/// 	- \c ImageErrorCode::SDLFailure if reading the data through an SDL memory stream failed;
	/// 	- \c ImageErrorCode::DecodeFailure if SDL_image could not decode the data, including when the format is
	/// 	  not recognized or not supported by the build. The message contains SDL's error description.
	[[nodiscard]] auto decode (
		std::span<const std::byte> bytes, std::string_view hint, ImageLoaderOptions options
	) const -> std::expected<Image, ImageError> override;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_SDL_IMAGE_H__
