
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
/// Conversion requires supported color encoding. Ordinary loading provides best-effort missing metadata through
/// <tt>\ref fcg::ImageLoader::load "ImageLoader::load"</tt>. To upload numeric samples without interpreting color,
/// see <tt>\ref fcg::Image::uploadStoredSamples "Image::uploadStoredSamples"</tt>.
///
/// \subsection fcg_pixel_formats_workflows_loading Inspecting metadata evidence
/// <tt>\ref fcg::ImagePixelInfo::provenance "ImagePixelInfo::provenance"</tt> describes the origin of primaries,
/// transfer, range, SDR white level and headroom. Each <tt>\ref fcg::ImagePropertyProvenance
/// "ImagePropertyProvenance"</tt> distinguishes source declarations, format definitions, opaque decoder results,
/// assumptions, calculated values and assigned output definitions. Independent fields record sample adaptation and
/// dependence on known assumptions. A calculated headroom can therefore remain dependent on assumed diffuse white:
/// \snippet pixel_examples.cpp loading_adjustments
/// Moving or independently copying an image retains its evidence. Pixel edits invalidate content headroom; encoding
/// changes also invalidate dependent evidence. Explicit conversion records its destination interpretation. Ordinary
/// <tt>\ref fcg::Image::converted "Image::converted"</tt> clears headroom when the RGB peak may change. Call
/// <tt>\ref fcg::Image::refreshMetadata "Image::refreshMetadata"</tt> afterwards or use
/// <tt>\ref fcg::Image::convertedWithMetadata "Image::convertedWithMetadata"</tt> to complete SDR white level,
/// remeasure headroom and update those two properties' provenance. Primaries, transfer and range retain the
/// interpretation established by loading or conversion. Metadata inspection itself does not scan pixels.



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

/// The evidence or operation establishing a current metadata value.
enum class ImageMetadataOrigin
{
	/// No evidence is recorded; this includes externally supplied surface metadata.
	Unknown,

	/// The handler inspected an explicit source declaration.
	Declared,

	/// The file format defines the value independently of optional declarations.
	FormatDefined,

	/// A decoder supplied the value without exposing its evidence or processing.
	DecoderReported,

	/// A fallback interpretation was selected for missing source information.
	Assumed,

	/// The value was calculated from pixels or other metadata.
	Derived,

	/// A caller or operation selected an output definition.
	Assigned
};

/// \brief Evidence and processing associated with one current metadata property.
///
/// These records describe the current interpretation, rather than an operation history. An absent recorded
/// assumption is not a guarantee of color accuracy, particularly for opaque decoder results.
struct ImagePropertyProvenance
{
	/// How the current value was established.
	ImageMetadataOrigin origin = ImageMetadataOrigin::Unknown;

	/// Whether samples were adapted to this interpretation; layout changes alone do not qualify.
	bool adapted = false;

	/// Whether a known assumption contributed to this interpretation or its sample processing.
	bool dependsOnAssumptions = false;

	/// Owned evidence, fallback policy, or calculation suitable for display to users.
	std::string explanation;
};

/// Provenance of the current color interpretation and content brightness metadata.
struct ImageMetadataProvenance
{
	/// Evidence for the primaries and chromatic reference white.
	ImagePropertyProvenance colorPrimaries;

	/// Evidence for the transfer characteristics.
	ImagePropertyProvenance transferCharacteristics;

	/// Evidence for the encoded color range.
	ImagePropertyProvenance colorRange;

	/// Evidence for the SDR diffuse-white brightness level.
	ImagePropertyProvenance sdrWhitePoint;

	/// Evidence for the content headroom relative to SDR white.
	ImagePropertyProvenance hdrHeadroom;
};

/// Candidate RGB encoding and evidence used to complete missing components during decoding.
struct ImageMetadataCandidates
{
	/// Candidate encoding; unknown components supply no candidate.
	SDL_Colorspace colorspace = SDL_COLORSPACE_UNKNOWN;

	/// Evidence for each candidate; assumptions require the corresponding loading option.
	ImageMetadataProvenance provenance;
};

/// Controls brightness metadata completion for supported linear floating-point RGB images.
struct ImageHDRMetadataOptions
{
	/// Fill a missing SDR white level with 1.0 when the image uses linear floating-point RGB.
	bool assumeSDRWhitePoint = true;

	/// Measure the stored RGB peak and derive headroom relative to SDR white.
	bool deriveHDRHeadroom = true;
};

/// Outcome of metadata completion; unavailable analysis is reported without failing the image operation.
struct ImageMetadataRefreshResult
{
	/// Whether this call filled a missing white level with an assumption.
	bool whitePointAssumed = false;

	/// Whether this call published newly measured headroom.
	bool headroomDerived = false;

	/// Why requested analysis was unavailable; empty when it completed or was disabled.
	std::string diagnostic;
};

/// Parts of an image that an external surface edit may change.
enum class ImageSurfaceChanges : unsigned
{
	/// Pixel or palette values may change, invalidating content headroom.
	Pixels = 1,

	/// Color interpretation may change, invalidating provenance and dependent headroom.
	ColorMetadata = 2,

	/// Both pixel data and color interpretation may change.
	All = 3
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

	/// Evidence and processing associated with the current metadata values.
	ImageMetadataProvenance provenance;

	/// Whether a palette is attached.
	bool hasPalette = false;

	/// Attached palette entry count.
	int paletteSize = 0;

	/// Separate transparent sample or index, if present.
	std::optional<Uint32> colorKey;

	/// Explicit SDR diffuse-white brightness level, if present, in the colorspace units.
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

/// \brief Combine categories of external changes.
///
/// \param left First category.
/// \param right Second category.
///
/// \return The combined categories.
[[nodiscard]] constexpr auto operator | (ImageSurfaceChanges left, ImageSurfaceChanges right) noexcept
	-> ImageSurfaceChanges {
	return static_cast<ImageSurfaceChanges>(static_cast<unsigned>(left) | static_cast<unsigned>(right));
}

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
