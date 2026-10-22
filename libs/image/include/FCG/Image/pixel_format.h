
#ifndef __FCG_IMAGE_PIXEL_FORMAT_H__
#define __FCG_IMAGE_PIXEL_FORMAT_H__


//////
//
// Module documentation
//

/// \defgroup fcg_pixel_formats Pixel representations and conversion
/// \ingroup fcg_modules
/// \brief Inspect native storage and approve explicit conversions.
///
/// Begin with <tt>\ref fcg::Image::pixelInfo "Image::pixelInfo"</tt> to discover storage and color encoding.
/// Analyze a requested representation with <tt>\ref fcg::Image::conversionInfo "Image::conversionInfo"</tt>.
/// Reports describe potential loss without inspecting samples. Choose an RGB destination, then call
/// <tt>\ref fcg::Image::converted "Image::converted"</tt> before using \ref fcg_pixel_access.
///
/// \section fcg_pixel_formats_usage Getting started
/// \snippet pixel_examples.cpp conversion
///
/// \section fcg_pixel_formats_workflows Common tasks
/// Preserve supported source encoding by omitting the destination colorspace for RGB inputs. Missing or unsupported
/// encoding still prevents conversion, including layout-only requests. YUV inputs require an explicit RGB destination
/// colorspace. Palette expansion and channel reordering are informational changes. Reduced sample
/// precision, narrower range, discarded transparency and encoding transformations require an explicit
/// <tt>\ref fcg::PixelLossPolicy::Allow "PixelLossPolicy::Allow"</tt>.
/// Floating output retains HDR values without tone mapping. Color keys become alpha by default; choose an
/// alpha-capable destination. Sub-byte, planar and compressed descriptions leave ordinary byte size unavailable.
/// Conversion requires supported color encoding. When loading files that omit it, use
/// <tt>\ref fcg::ImageLoader::loadAndGuess "ImageLoader::loadAndGuess"</tt> or the inherited handler operation
/// <tt>\ref fcg::ImageFormatHandler::loadAndGuess "ImageFormatHandler::loadAndGuess"</tt>. To upload numeric samples
/// without interpreting color, see <tt>\ref fcg::Image::uploadStoredSamples "Image::uploadStoredSamples"</tt>.



//////
//
// Includes
//

// C++ STL
#include <array>
#include <expected>
#include <optional>
#include <string>

// SDL3 library
#include <SDL3/SDL_pixels.h>

// Local includes
#include "FCG/Image/error.h"
#include "FCG/Image/export.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_pixel_formats
/// @{



//////
//
// Structs and enums
//

/// Physical organization of pixel samples.
enum class PixelStorage
{
	/// Byte-addressable RGB components.
	Interleaved,

	/// RGB components packed into integers.
	Packed,

	/// Palette indices, possibly smaller than a byte.
	Indexed,

	/// Packed or planar YUV samples.
	YUV,

	/// Compressed image data.
	Compressed,

	/// Pixels stored outside ordinary CPU memory.
	External
};

/// Numeric interpretation of color components.
enum class PixelRepresentation
{
	/// Unsigned normalized integer samples.
	UnsignedInteger,

	/// IEEE binary16 floating samples.
	Binary16,

	/// IEEE binary32 floating samples.
	Binary32,

	/// Indices into a separate RGBA palette.
	PaletteIndex,

	/// Component interpretation is unavailable.
	Unknown
};

/// Permission for conservative metadata-based loss risks.
enum class PixelLossPolicy
{
	/// Reject potential precision, range, channel or encoding loss.
	Reject,

	/// Permit all reported potential loss.
	Allow
};

/// Treatment of separate color-key transparency.
enum class PixelColorKeyPolicy
{
	/// Bake matching pixels into destination alpha.
	Bake,

	/// Discard the color key; requires permission for loss.
	Discard
};

/// An owning snapshot of a recognized native SDL storage format.
struct PixelFormatInfo
{
	/// Native SDL identifier; endian aliases have the same identifier.
	SDL_PixelFormat format = SDL_PIXELFORMAT_UNKNOWN;

	/// Readable SDL format name.
	std::string name;

	/// Storage organization.
	PixelStorage storage = PixelStorage::Packed;

	/// Component interpretation.
	PixelRepresentation representation = PixelRepresentation::Unknown;

	/// Array order in memory, or packed order from most to least significant bits.
	std::string channelOrder;

	/// Sample bits in logical red, green, blue and alpha order; palette components use eight bits.
	std::array<unsigned, 4> channelBits{};

	/// Whether logical RGB/alpha precision is known; false for YUV, compressed and external storage.
	bool channelPrecisionKnown = false;

	/// Significant precision in logical RGBA order, including the implicit leading bit for floating samples.
	std::array<unsigned, 4> channelPrecision{};

	/// Sample precision per YUV component, if YUV storage is recognized.
	std::optional<unsigned> yuvSampleBits;

	/// Physical bits per YUV component slot; P010 uses sixteen-bit slots for ten-bit samples.
	std::optional<unsigned> yuvStorageBits;

	/// Encoded bits per pixel, unavailable for planar, compressed and external storage.
	std::optional<unsigned> bitsPerPixel;

	/// Physical bytes per pixel, including unused components; unavailable for sub-byte and YUV storage.
	std::optional<unsigned> bytesPerPixel;

	/// Unused physical storage bits; zero when unavailable.
	unsigned unusedBits = 0;

	/// Whether the format or its palette can represent alpha.
	bool hasAlpha = false;

	/// Whether native typed row access is supported.
	bool directAccess = false;
};

/// An owning snapshot of image storage and color metadata.
struct ImagePixelInfo
{
	/// Storage description.
	PixelFormatInfo pixelFormat;

	/// Visible width in pixels.
	int width = 0;

	/// Visible height in pixels.
	int height = 0;

	/// Byte distance between consecutive rows, including padding.
	int pitch = 0;

	/// Color encoding retained by native and convenience pixel access.
	SDL_Colorspace colorspace = SDL_COLORSPACE_UNKNOWN;

	/// Whether a palette is attached.
	bool hasPalette = false;

	/// Attached palette entry count.
	int paletteSize = 0;

	/// Separate transparent sample or index, if present.
	std::optional<Uint32> colorKey;

	/// Explicit SDR white-point property, if present, in the colorspace units.
	std::optional<float> sdrWhitePoint;

	/// Explicit headroom property, if present, relative to SDR white.
	std::optional<float> hdrHeadroom;
};

/// Policies for explicit image conversion; tone mapping is disabled.
struct PixelConversionOptions
{
	/// Potential loss is rejected by default.
	PixelLossPolicy lossPolicy = PixelLossPolicy::Reject;

	/// Destination encoding; omission preserves source RGB encoding. YUV sources require an explicit RGB encoding.
	std::optional<SDL_Colorspace> colorspace;

	/// Separate transparency is baked into alpha by default.
	PixelColorKeyPolicy colorKeyPolicy = PixelColorKeyPolicy::Bake;
};

/// Conservative conversion analysis; flags describe possible effects without inspecting pixel values.
struct PixelConversionInfo
{
	/// Current source metadata.
	ImagePixelInfo source;

	/// Requested destination storage.
	PixelFormatInfo destination;

	/// Resolved destination encoding.
	SDL_Colorspace destinationColorspace = SDL_COLORSPACE_UNKNOWN;

	/// Whether the storage and encoding request is supported.
	bool supported = false;

	/// Whether the request satisfies the selected policies.
	bool permitted = false;

	/// Actionable reason for an unsupported or rejected request.
	std::string diagnostic;

	/// Physical storage organization or size changes.
	bool layoutChange = false;

	/// Logical component order changes.
	bool channelOrderChange = false;

	/// Palette samples become RGB components.
	bool paletteExpansion = false;

	/// Destination adds an alpha channel.
	bool alphaAddition = false;

	/// Destination removes an alpha channel.
	bool alphaRemoval = false;

	/// Separate transparency becomes alpha.
	bool colorKeyBaking = false;

	/// Separate transparency is discarded.
	bool colorKeyDiscarding = false;

	/// Numeric component interpretation changes.
	bool representationChange = false;

	/// Color encoding changes.
	bool colorEncodingChange = false;

	/// Component precision may decrease.
	bool precisionReduction = false;

	/// Representable range may decrease.
	bool rangeClipping = false;

	/// Color transformation may lose information.
	bool colorTransformLoss = false;

	/// Success allocates independent output pixel storage.
	bool allocatesStorage = false;
};



//////
//
// Functions
//

/// \brief Describe a native storage format independently of any image.
///
/// \param format SDL storage identifier.
///
/// \return An owning description, or \c ImageErrorCode::UnsupportedFormat for an unknown identifier.
[[nodiscard]] FCG_IMAGE_EXPORT auto pixelFormatInfo (SDL_PixelFormat format)
	-> std::expected<PixelFormatInfo, ImageError>;



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // ifndef __FCG_IMAGE_PIXEL_FORMAT_H__
