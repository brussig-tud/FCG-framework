
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <expected>

// FCG Framework
#include <FCG/Image/image.h>



//////
//
// Module namespace open
//

// Compiled pixel processing examples.
namespace pixel_examples {



//////
//
// Functions
//

/// Inspect representation and refuse any unapproved potential loss.
//! [conversion]
[[nodiscard]] auto editableCopy (const fcg::Image &source) -> std::expected<fcg::Image, fcg::ImageError>
{
	auto metadata = source.pixelInfo();
	if (!metadata)
		return std::unexpected(metadata.error());
	fcg::PixelConversionOptions options;
	// Choose explicit RGB encoding for YUV input; transformation loss still requires caller permission.
	if (metadata->pixelFormat.storage == fcg::PixelStorage::YUV)
		options.colorspace = SDL_COLORSPACE_SRGB;
	auto analysis = source.conversionInfo(SDL_PIXELFORMAT_RGBA32, options);
	if (!analysis)
		return std::unexpected(analysis.error());
	if (!analysis->supported)
		return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::UnsupportedFormat, analysis->diagnostic});
	if (!analysis->permitted)
		return std::unexpected(fcg::ImageError{fcg::ImageErrorCode::LossyConversion, analysis->diagnostic});
	return source.converted(SDL_PIXELFORMAT_RGBA32, options);
}
//! [conversion]

/// Edit an exact-format image during one scoped access session.
//! [access]
[[nodiscard]] auto invert (fcg::Image &image) -> std::expected<void, fcg::ImageError> {
	return image.withPixels<SDL_PIXELFORMAT_RGBA32>([](auto view) {
		for (auto row : view)
			for (int x = 0; x < row.size(); ++x) {
				auto value = row.readUnchecked(x);
				value.r = 255 - value.r;
				row.writeUnchecked(x, value);
			}
	});
}
//! [access]

/// Compute a horizontal three-sample average with explicit clamped border handling.
//! [neighborhood]
[[nodiscard]] auto horizontalBlur (const fcg::Image &source, fcg::Image &destination)
	-> std::expected<void, fcg::ImageError> {
	return source.transformPixelsTo<SDL_PIXELFORMAT_RGBA128_FLOAT>(destination, [](int x, int y, auto input) {
		const auto left = input.readUnchecked(std::max(0, x - 1), y);
		const auto center = input.readUnchecked(x, y);
		const auto right = input.readUnchecked(std::min(input.width() - 1, x + 1), y);
		return fcg::NativeRGBA<float>{(left.r + center.r + right.r) / 3.f,
			(left.g + center.g + right.g) / 3.f, (left.b + center.b + right.b) / 3.f, center.a};
	});
}
//! [neighborhood]



//////
//
// Module namespace close
//

// namespace pixel_examples
}
