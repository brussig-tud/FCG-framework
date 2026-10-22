
//////
//
// Includes
//

// C++ STL
#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

// Local includes
#include "FCG/Image/pfm.h"
#include "hdr_internal.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Check a PFM identifier and its record delimiter without mistaking integer PNM for PFM.
[[nodiscard]] auto isPFM (std::span<const std::byte> bytes) -> bool {
	if (bytes.size() < 3)
		return false;
	const auto *text = reinterpret_cast<const char*>(bytes.data());
	return text[0] == 'P' && (text[1] == 'F' || text[1] == 'f')
		&& (text[2] == '\n' || text[2] == '\r' || text[2] == ' ' || text[2] == '\t');
}

/// Consume the next nonempty PFM text record, including optional comments before the raster.
[[nodiscard]] auto record (std::string_view &input) -> std::optional<std::string_view>
{
	while (auto line = fcg::detail::imageHeaderLine(input)) {
		const auto text = fcg::detail::trimImageHeader(line->substr(0, line->find('#')));
		if (!text.empty())
			return text;
	}
	return std::nullopt;
}

/// Keep this handler in ordinary static archive links.
const fcg::FormatHandlerRegistration pfmRegistration{"pfm", std::make_unique<fcg::PFMImageFormatHandler>(), -90};

// Anonymous namespace end
}



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

// Private implementation helpers.
namespace detail {

void linkPFMImageHandler () {
	(void) &pfmRegistration;
}

// namespace detail
}



//////
//
// Class implementations
//

////
// PFMImageFormatHandler

auto PFMImageFormatHandler::fileFormats () const -> std::vector<ImageFileFormat> {
	return {{"Portable Float Map", {"pfm"}}};
}

auto PFMImageFormatHandler::accepts (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<bool, ImageError> {
	if (auto normalized = detail::imageHint(bytes, hint); !normalized)
		return std::unexpected(std::move(normalized.error()));
	return isPFM(bytes);
}

auto PFMImageFormatHandler::load (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<Image, ImageError>
{
	static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
	static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
	// Establish format ownership before parsing text or allocating pixel storage.
	if (auto accepted = accepts(bytes, hint); !accepted || !*accepted)
		return accepted ? std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Not a PFM image"})
			: std::unexpected(std::move(accepted.error()));
	std::string_view input(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	// Header records end at their newline; binary raster bytes are never trimmed.
	const auto magic = record(input);
	if (!magic || (*magic != "PF" && *magic != "Pf"))
		return detail::imageDecodeError("Invalid PFM identifier record");
	auto dimensions = record(input);
	if (!dimensions)
		return detail::imageDecodeError("Missing PFM dimensions");
	const auto width = detail::imageDimension(detail::imageHeaderToken(*dimensions));
	const auto height = detail::imageDimension(detail::imageHeaderToken(*dimensions));
	if (!width || !height || !detail::trimImageHeader(*dimensions).empty())
		return detail::imageDecodeError("Invalid PFM dimensions");
	auto scaleText = record(input);
	if (!scaleText)
		return detail::imageDecodeError("Missing PFM scale record");
	if (scaleText->starts_with('+'))
		scaleText->remove_prefix(1);
	float scale = 0;
	const auto parsed = std::from_chars(scaleText->data(), scaleText->data() + scaleText->size(), scale);
	if (parsed.ec != std::errc{} || parsed.ptr != scaleText->data() + scaleText->size()
		|| !std::isfinite(scale) || scale == 0)
		return detail::imageDecodeError("PFM scale must be finite and nonzero");
	const unsigned channels = *magic == "PF" ? 3 : 1;
	// Verify the entire payload length before reserving the expanded RGB surface.
	const auto samples = std::uint64_t(*width) * *height * channels;
	if (samples > std::numeric_limits<std::size_t>::max() / 4 || samples * 4 != input.size())
		return detail::imageDecodeError("PFM raster length does not match its dimensions");
	auto surface = detail::floatImageSurface(*width, *height, false, detail::unidentifiedImageColorspace(false));
	if (!surface)
		return std::unexpected(std::move(surface.error()));
	const bool swap = (scale < 0) != (std::endian::native == std::endian::little);
	const float factor = std::abs(scale);
	// Read wire samples without alignment assumptions and reverse conventional PFM row order.
	for (int y = 0; y < *height; ++y)
	{
		auto *output = reinterpret_cast<float*>(static_cast<std::byte*>((*surface)->pixels)
			+ std::size_t(*height - 1 - y) * (*surface)->pitch);
		for (int x = 0; x < *width; ++x)
		{
			float pixel[3]{};
			for (unsigned c = 0; c < channels; ++c) {
				std::uint32_t bits;
				std::memcpy(&bits, input.data(), 4);
				input.remove_prefix(4);
				pixel[c] = std::bit_cast<float>(swap ? std::byteswap(bits) : bits) * factor;
			}
			if (channels == 1)
				pixel[1] = pixel[2] = pixel[0];
			std::memcpy(output + std::size_t(x) * 3, pixel, sizeof(pixel));
		}
	}
	return detail::adoptImageSurface(std::move(*surface));
}



//////
//
// Module namespace close
//

// namespace fcg
}
