
//////
//
// Includes
//

// C++ STL
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

// OpenEXR library
#include <Iex.h>
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfIO.h>
#include <ImfInputFile.h>
#include <ImfStandardAttributes.h>
#include <ImfVersion.h>

// Local includes
#include "FCG/Image/openexr.h"
#include "hdr_internal.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// OpenEXR's configured public namespace, independent of its versioned ABI namespace.
namespace exr = OPENEXR_IMF_NAMESPACE;

/// A bounded synchronous stream borrowing the encoded bytes for one decode operation.
class MemoryStream final : public exr::IStream
{
public:

	////
	// Object construction

	/// Borrow the input without exposing it as writable mapped memory.
	explicit MemoryStream(std::span<const std::byte> bytes) : exr::IStream("<memory>"), m_bytes(bytes) {}


	////
	// Interface: exr::IStream

	/// Retain the base stateless overload alongside the sequential overload.
	using exr::IStream::read;

	/// Copy a complete read or report truncated input; exact-end reads return false.
	[[nodiscard]] auto read (char *buffer, int count) -> bool override
	{
		if (count < 0 || std::size_t(count) > m_bytes.size() - m_position)
			throw IEX_NAMESPACE::InputExc("Truncated OpenEXR memory input");
		std::memcpy(buffer, m_bytes.data() + m_position, std::size_t(count));
		m_position += std::size_t(count);
		return m_position < m_bytes.size();
	}

	/// Return the current byte offset.
	[[nodiscard]] auto tellg () -> std::uint64_t override { return m_position; }

	/// Reject seeks beyond the encoded input.
	void seekg (std::uint64_t position) override {
		if (position > m_bytes.size())
			throw IEX_NAMESPACE::InputExc("OpenEXR seek exceeds memory input");
		m_position = static_cast<std::size_t>(position);
	}

	/// Supply the actual input size to enable OpenEXR consistency checks.
	[[nodiscard]] auto size () -> std::int64_t override { return static_cast<std::int64_t>(m_bytes.size()); }


private:

	////
	// Fields

	/// Immutable bytes that outlive this stream.
	std::span<const std::byte> m_bytes;

	/// Next sequential read position, bounded by the input size.
	std::size_t m_position = 0;
};

/// Recognize the OpenEXR magic without parsing the file.
[[nodiscard]] auto isEXR (std::span<const std::byte> bytes) -> bool {
	return bytes.size() >= 4 && exr::isImfMagic(reinterpret_cast<const char*>(bytes.data()));
}

/// Preserve established color metadata, recognizing linear Rec.709 and otherwise retaining unknown primaries.
[[nodiscard]] auto colorEncoding (const exr::Header &header) -> SDL_Colorspace
{
	if (exr::hasColorInteropID(header))
		return exr::colorInteropID(header) == "lin_rec709_scene" ? SDL_COLORSPACE_SRGB_LINEAR
			: fcg::detail::unidentifiedImageColorspace(false);
	if (exr::hasChromaticities(header))
	{
		const auto &actual = exr::chromaticities(header);
		const exr::Chromaticities expected;
		const auto near = [] (const auto &a, const auto &b) {
			return std::abs(a.x - b.x) <= .0001f && std::abs(a.y - b.y) <= .0001f;
		};
		if (near(actual.red, expected.red) && near(actual.green, expected.green)
			&& near(actual.blue, expected.blue) && near(actual.white, expected.white))
			return SDL_COLORSPACE_SRGB_LINEAR;
	}
	return fcg::detail::unidentifiedImageColorspace(true);
}

/// Read selected full-resolution floating channels directly into independently owned surface storage.
[[nodiscard]] auto decodeEXR (std::span<const std::byte> bytes) -> std::expected<fcg::Image, fcg::ImageError>
{
	using namespace fcg::detail;
	// Reject unsupported file kinds before InputFile can select or flatten them implicitly.
	if (bytes.size() < 8 || bytes.size() > std::uint64_t(std::numeric_limits<std::int64_t>::max()))
		return imageDecodeError("Truncated or oversized OpenEXR input");
	std::uint32_t version;
	std::memcpy(&version, bytes.data() + 4, 4);
	if constexpr (std::endian::native == std::endian::big)
		version = std::byteswap(version);
	if (exr::getVersion(version) != exr::EXR_VERSION || !exr::supportsFlags(exr::getFlags(version))
		|| exr::isMultiPart(version) || exr::isNonImage(version))
		return imageDecodeError("OpenEXR requires a supported single-part, non-deep image");
	MemoryStream stream(bytes);
	// Use per-file diagnostics and validation; OpenEXR's default callback writes failures to stderr.
	exr::ContextInitializer initialization;
	initialization.setInputStream(&stream).setErrorHandler([] (exr_const_context_t, exr_result_t, const char*) {})
		.strictHeaderValidation(true).disableChunkReconstruction(true);
	exr::InputFile file("<memory>", initialization, 0);
	const auto &header = file.header();
	const auto &channels = header.channels();
	const auto &window = header.dataWindow();
	const auto width = std::int64_t(window.max.x) - window.min.x + 1;
	const auto height = std::int64_t(window.max.y) - window.min.y + 1;
	// Select a complete color set and validate only its full-resolution floating channels.
	const bool anyRGB = channels.findChannel("R") || channels.findChannel("G") || channels.findChannel("B");
	const bool gray = !anyRGB && channels.findChannel("Y");
	const bool alpha = channels.findChannel("A");
	const std::array<const char*, 4> names = gray ? std::array{"Y", "Y", "Y", "A"}
		: std::array{"R", "G", "B", "A"};
	for (unsigned c = 0; c < (alpha ? 4u : 3u); ++c)
	{
		const auto *channel = channels.findChannel(names[c]);
		if (!channel || (channel->type != exr::HALF && channel->type != exr::FLOAT)
			|| channel->xSampling != 1 || channel->ySampling != 1)
			return imageDecodeError("OpenEXR requires full-resolution HALF/FLOAT RGB or Y, with optional A");
	}
	auto surface = floatImageSurface(width, height, alpha, colorEncoding(header));
	// Rebase absolute EXR coordinates through checked Slice::Make addressing and actual SDL pitch.
	if (!surface)
		return std::unexpected(std::move(surface.error()));
	const auto components = alpha ? 4u : 3u;
	exr::FrameBuffer frame;
	for (unsigned c = 0; c < components; ++c)
	{
		if (gray && c > 0 && c < 3)
			continue;
		frame.insert(names[c], exr::Slice::Make(exr::FLOAT,
			static_cast<std::byte*>((*surface)->pixels) + c * sizeof(float), window,
			components * sizeof(float), (*surface)->pitch));
	}
	file.setFrameBuffer(frame);
	file.readPixels(window.min.y, window.max.y);
	// Expand a luminance plane after the decoder has written it into the first RGB component.
	if (gray)
		for (int y = 0; y < (*surface)->h; ++y)
		{
			auto *row = reinterpret_cast<float*>(static_cast<std::byte*>((*surface)->pixels) + std::size_t(y) * (*surface)->pitch);
			for (int x = 0; x < (*surface)->w; ++x)
				row[std::size_t(x) * components + 1] = row[std::size_t(x) * components + 2] = row[std::size_t(x) * components];
		}
	return adoptImageSurface(std::move(*surface));
}

/// Keep the optional decoder in ordinary static archive links.
const fcg::FormatHandlerRegistration exrRegistration{"openexr", std::make_unique<fcg::OpenEXRImageFormatHandler>(), -90};

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

void linkOpenEXRImageHandler () {
	(void) &exrRegistration;
}

// namespace detail
}



//////
//
// Class implementations
//

////
// OpenEXRImageFormatHandler

auto OpenEXRImageFormatHandler::fileFormats () const -> std::vector<ImageFileFormat> {
	return {{"OpenEXR", {"exr"}}};
}

auto OpenEXRImageFormatHandler::accepts (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<bool, ImageError> {
	if (auto normalized = detail::imageHint(bytes, hint); !normalized)
		return std::unexpected(std::move(normalized.error()));
	return isEXR(bytes);
}

auto OpenEXRImageFormatHandler::load (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<Image, ImageError>
{
	if (auto accepted = accepts(bytes, hint); !accepted || !*accepted)
		return accepted ? std::unexpected(ImageError{ImageErrorCode::UnsupportedFormat, "Not an OpenEXR image"})
			: std::unexpected(std::move(accepted.error()));
	try {
		return decodeEXR(bytes);
	} catch (const IEX_NAMESPACE::BaseExc &error) {
		return detail::imageDecodeError(std::string("Decoding OpenEXR: ") + error.what());
	}
}



//////
//
// Module namespace close
//

// namespace fcg
}
