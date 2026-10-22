
#ifndef __FCG_HDR_INTERNAL_H__
#define __FCG_HDR_INTERNAL_H__


//////
//
// Includes
//

// C++ STL
#include <cstdint>
#include <memory>
#include <optional>

// Local includes
#include "image_internal.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// Private helpers for floating-point image decoders.
namespace detail {



/// Surface ownership retained until successful adoption by an image.
using ImageSurface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;



//////
//
// Functions
//

/// Remove horizontal ASCII whitespace and carriage returns from a text header field.
[[nodiscard]] auto trimImageHeader (std::string_view text) -> std::string_view;

/// Consume one LF-terminated header line without touching the following raster bytes.
[[nodiscard]] auto imageHeaderLine (std::string_view &input) -> std::optional<std::string_view>;

/// Consume one horizontal-whitespace-separated token from a text header record.
[[nodiscard]] auto imageHeaderToken (std::string_view &line) -> std::string_view;

/// Parse a positive decimal integer that fits an SDL dimension; reject trailing characters.
[[nodiscard]] auto imageDimension (std::string_view token) -> std::optional<int>;

/// Describe full-range RGB with unidentified primaries and optionally established linear transfer.
[[nodiscard]] auto unidentifiedImageColorspace (bool linear) -> SDL_Colorspace;

/// Allocate checked RGB/RGBA binary32 storage with explicit encoding and disabled tone mapping.
[[nodiscard]] auto floatImageSurface (std::int64_t width, std::int64_t height, bool alpha, SDL_Colorspace colorspace)
	-> std::expected<ImageSurface, ImageError>;

/// Transfer owned surface storage to an image only after successful validation.
[[nodiscard]] auto adoptImageSurface (ImageSurface surface) -> std::expected<Image, ImageError>;

/// Construct a recoverable decoder diagnostic without logging.
[[nodiscard]] auto imageDecodeError (std::string message) -> std::unexpected<ImageError>;



//////
//
// Namespaces close
//

// namespace detail
}

// namespace fcg
}


#endif // ifndef __FCG_HDR_INTERNAL_H__
