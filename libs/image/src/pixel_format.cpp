
//////
//
// Includes
//

// C++ STL
#include <numeric>

// Local includes
#include "FCG/Image/pixel_access.h"



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Function implementations
//

[[nodiscard]] auto pixelFormatInfo (SDL_PixelFormat format) -> std::expected<PixelFormatInfo, ImageError>
{
	PixelFormatInfo result;
	result.format = format;
	result.name = SDL_GetPixelFormatName(format);
	if (format == SDL_PIXELFORMAT_UNKNOWN || result.name == "SDL_PIXELFORMAT_UNKNOWN")
		return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Unknown SDL pixel format"});
	if (SDL_ISPIXELFORMAT_FOURCC(format))
	{
		switch (format) {
		case SDL_PIXELFORMAT_MJPG: result.storage = PixelStorage::Compressed; break;
		case SDL_PIXELFORMAT_EXTERNAL_OES: result.storage = PixelStorage::External; break;
		case SDL_PIXELFORMAT_YV12: case SDL_PIXELFORMAT_IYUV:
		case SDL_PIXELFORMAT_YUY2: case SDL_PIXELFORMAT_UYVY: case SDL_PIXELFORMAT_YVYU:
		case SDL_PIXELFORMAT_NV12: case SDL_PIXELFORMAT_NV21: case SDL_PIXELFORMAT_P010:
			result.storage = PixelStorage::YUV;
			result.representation = PixelRepresentation::UnsignedInteger;
			result.channelOrder = "YUV";
			result.yuvSampleBits = format == SDL_PIXELFORMAT_P010 ? 10 : 8;
			result.yuvStorageBits = format == SDL_PIXELFORMAT_P010 ? 16 : 8;
			break;
		default:
			return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Unrecognized FOURCC storage"});
		}
		return result;
	}
	const auto *details = SDL_GetPixelFormatDetails(format);
	if (!details)
		return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "SDL cannot describe this pixel format"});
	result.bitsPerPixel = SDL_BITSPERPIXEL(format);
	if (*result.bitsPerPixel >= 8)
		result.bytesPerPixel = details->bytes_per_pixel;
	result.directAccess = detail::nativePixelFormat(format);
	if (SDL_ISPIXELFORMAT_INDEXED(format)) {
		result.storage = PixelStorage::Indexed;
		result.representation = PixelRepresentation::PaletteIndex;
		result.channelBits = {8, 8, 8, 8};
		result.channelPrecision = result.channelBits;
		result.channelPrecisionKnown = true;
		result.hasAlpha = true;
		result.channelOrder = "index";
		return result;
	}
	const auto type = SDL_PIXELTYPE(format);
	const bool array = type == SDL_PIXELTYPE_ARRAYU8 || type == SDL_PIXELTYPE_ARRAYU16
		|| type == SDL_PIXELTYPE_ARRAYF16 || type == SDL_PIXELTYPE_ARRAYF32;
	result.storage = array ? PixelStorage::Interleaved : PixelStorage::Packed;
	result.representation = type == SDL_PIXELTYPE_ARRAYF16 ? PixelRepresentation::Binary16
		: type == SDL_PIXELTYPE_ARRAYF32 ? PixelRepresentation::Binary32 : PixelRepresentation::UnsignedInteger;
	result.hasAlpha = SDL_ISPIXELFORMAT_ALPHA(format);
	if (array)
	{
		const unsigned bits = type == SDL_PIXELTYPE_ARRAYU8 ? 8 : type == SDL_PIXELTYPE_ARRAYF32 ? 32 : 16;
		result.channelBits = {bits, bits, bits, result.hasAlpha ? bits : 0};
		switch (SDL_PIXELORDER(format)) {
		case SDL_ARRAYORDER_RGB: result.channelOrder = "RGB"; break;
		case SDL_ARRAYORDER_BGR: result.channelOrder = "BGR"; break;
		case SDL_ARRAYORDER_RGBA: result.channelOrder = "RGBA"; break;
		case SDL_ARRAYORDER_ARGB: result.channelOrder = "ARGB"; break;
		case SDL_ARRAYORDER_BGRA: result.channelOrder = "BGRA"; break;
		case SDL_ARRAYORDER_ABGR: result.channelOrder = "ABGR"; break;
		default: return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Unknown RGB array order"});
		}
	}
	else
	{
		result.channelBits = {details->Rbits, details->Gbits, details->Bbits, details->Abits};
		switch (SDL_PIXELORDER(format)) {
		case SDL_PACKEDORDER_XRGB: result.channelOrder = "XRGB"; break;
		case SDL_PACKEDORDER_RGBX: result.channelOrder = "RGBX"; break;
		case SDL_PACKEDORDER_ARGB: result.channelOrder = "ARGB"; break;
		case SDL_PACKEDORDER_RGBA: result.channelOrder = "RGBA"; break;
		case SDL_PACKEDORDER_XBGR: result.channelOrder = "XBGR"; break;
		case SDL_PACKEDORDER_BGRX: result.channelOrder = "BGRX"; break;
		case SDL_PACKEDORDER_ABGR: result.channelOrder = "ABGR"; break;
		case SDL_PACKEDORDER_BGRA: result.channelOrder = "BGRA"; break;
		default: return std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Unknown RGB packed order"});
		}
	}
	result.channelPrecisionKnown = true;
	result.channelPrecision = result.channelBits;
	if (SDL_ISPIXELFORMAT_FLOAT(format))
		for (auto &precision : result.channelPrecision)
			if (precision)
				precision = type == SDL_PIXELTYPE_ARRAYF16 ? 11 : 24;
	result.unusedBits = *result.bytesPerPixel * 8 - std::accumulate(result.channelBits.begin(), result.channelBits.end(), 0u);
	return result;
}



//////
//
// Module namespace close
//

// namespace fcg
}
