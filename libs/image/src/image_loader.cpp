
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

// SDL3 library
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_surface.h>

// Local includes
#include "image_internal.h"



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

// Private implementation helpers shared within the Image library.
namespace detail {

auto readImageFile (const std::filesystem::path &path) -> std::expected<EncodedImage, ImageError>
{
	if (path.empty()
		|| path.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Image path is empty or contains a NUL"});
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
		return std::unexpected(ImageError{ImageErrorCode::IOFailure, "Opening image file failed"});
	const auto size = file.tellg();
	if (size < 0 || static_cast<std::uintmax_t>(size) > std::numeric_limits<std::size_t>::max()
		|| size > std::numeric_limits<std::streamsize>::max())
		return std::unexpected(ImageError{ImageErrorCode::IOFailure, "Image file size cannot be read"});
	if (size == 0)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Image file is empty"});
	EncodedImage input;
	input.bytes.resize(static_cast<std::size_t>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char *>(input.bytes.data()), static_cast<std::streamsize>(size)))
		return std::unexpected(ImageError{ImageErrorCode::IOFailure, "Reading image file failed"});
	const auto extension = path.extension().u8string();
	input.hint.assign(reinterpret_cast<const char *>(extension.data()), extension.size());
	return input;
}

auto imageHint (std::span<const std::byte> bytes, std::string_view hint) -> std::expected<std::string, ImageError>
{
	if (bytes.empty() || hint.find('\0') != std::string_view::npos)
		return std::unexpected(
			ImageError{ImageErrorCode::InvalidArgument, "Image bytes are empty or hint contains a NUL"});
	if (hint.starts_with('.'))
		hint.remove_prefix(1);
	std::string normalized(hint);
	for (auto &c : normalized)
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	return normalized;
}

auto imageSDLError (ImageErrorCode code, const char *operation) -> std::unexpected<ImageError> {
	return std::unexpected(ImageError{code, std::string(operation) + ": " + SDL_GetError()});
}

// namespace detail
}



//////
//
// Class implementations
//

////
// ImageFormatHandler

auto ImageFormatHandler::load (const std::filesystem::path &path, ImageLoaderOptions options) const
	-> std::expected<Image, ImageError>
{
	auto input = detail::readImageFile(path);
	if (!input)
		return std::unexpected(std::move(input.error()));
	auto hint = detail::imageHint(input->bytes, input->hint);
	if (!hint)
		return std::unexpected(std::move(hint.error()));
	return load(input->bytes, *hint, options);
}

auto ImageFormatHandler::load (
	std::span<const std::byte> bytes, std::string_view hint, ImageLoaderOptions options
) const -> std::expected<Image, ImageError>
{
	const auto normalized = detail::imageHint(bytes, hint);
	if (!normalized)
		return std::unexpected(normalized.error());
	auto image = decode(bytes, *normalized, options);
	if (!image)
		return image;
	if (!image->handle())
		return std::unexpected(ImageError{ImageErrorCode::DecodeFailure, "Handler returned an empty image"});
	if (auto completed = detail::refreshImageMetadata(*image, options.hdr, false); !completed)
		return std::unexpected(completed.error());
	return image;
}

auto ImageFormatHandler::resolveMetadata (
	std::expected<Image, ImageError> image, ImageLoaderOptions options, ImageMetadataCandidates candidates,
	ImageMetadataProvenance provenance
) -> std::expected<Image, ImageError>
{
	if (!image || !image->handle())
		return image;
	auto previous = detail::readImageProvenance(*image->handle());
	if (!previous)
		return std::unexpected(previous.error());
	// Preserve forwarded evidence unless this handler supplies a completed interpretation.
	for (const auto member : {&ImageMetadataProvenance::colorPrimaries,
		&ImageMetadataProvenance::transferCharacteristics, &ImageMetadataProvenance::colorRange,
		&ImageMetadataProvenance::sdrWhitePoint, &ImageMetadataProvenance::hdrHeadroom})
		if ((provenance.*member).origin == ImageMetadataOrigin::Unknown
			&& !(provenance.*member).adapted && !(provenance.*member).dependsOnAssumptions
			&& (provenance.*member).explanation.empty())
			provenance.*member = previous.value().*member;

	const auto original = SDL_GetSurfaceColorspace(image->handle());
	if (!SDL_ISPIXELFORMAT_FOURCC(image->format()))
	{
		// Source declarations and format facts can fill missing components even in conservative mode.
		const auto allowed = [](const ImagePropertyProvenance &record, bool assume) {
			return record.origin != ImageMetadataOrigin::Unknown
				&& (record.origin != ImageMetadataOrigin::Assumed || assume);
		};
		const bool complete = allowed(candidates.provenance.colorPrimaries, options.inferColorPrimaries)
			|| allowed(candidates.provenance.transferCharacteristics, options.inferTransferCharacteristics)
			|| allowed(candidates.provenance.colorRange, options.inferColorRange);
		const auto fallback = static_cast<SDL_Colorspace>(SDL_DEFINE_COLORSPACE(
			complete ? SDL_COLORSPACETYPE(candidates.colorspace) : SDL_COLORSPACETYPE(original),
			allowed(candidates.provenance.colorRange, options.inferColorRange)
				? SDL_COLORSPACERANGE(candidates.colorspace) : SDL_COLORSPACERANGE(original),
			allowed(candidates.provenance.colorPrimaries, options.inferColorPrimaries)
				? SDL_COLORSPACEPRIMARIES(candidates.colorspace) : SDL_COLORSPACEPRIMARIES(original),
			allowed(candidates.provenance.transferCharacteristics, options.inferTransferCharacteristics)
				? SDL_COLORSPACETRANSFER(candidates.colorspace) : SDL_COLORSPACETRANSFER(original),
			SDL_MATRIX_COEFFICIENTS_IDENTITY, SDL_CHROMA_LOCATION_NONE));
		const auto encoding = detail::fillImageEncoding(original, fallback);
		if (SDL_COLORSPACEPRIMARIES(original) != SDL_COLORSPACEPRIMARIES(encoding))
			provenance.colorPrimaries = candidates.provenance.colorPrimaries;
		if (SDL_COLORSPACETRANSFER(original) != SDL_COLORSPACETRANSFER(encoding))
			provenance.transferCharacteristics = candidates.provenance.transferCharacteristics;
		if (SDL_COLORSPACERANGE(original) != SDL_COLORSPACERANGE(encoding))
			provenance.colorRange = candidates.provenance.colorRange;
		if (encoding != original && !SDL_SetSurfaceColorspace(image->handle(), encoding))
			return detail::imageSDLError(ImageErrorCode::SDLFailure, "Assigning completed image interpretation");
	}
	if (auto recorded = detail::writeImageProvenance(*image->handle(), provenance); !recorded)
		return std::unexpected(recorded.error());
	return image;
}


////
// ImageLoader

auto ImageLoader::global () -> ImageLoader& {
	// Intentionally process-lifetime: static registrars and teardown code must not observe a destroyed registry.
	static auto *loader = new ImageLoader;
	// Initialize storage before touching the anchor: deferred TU initialization may itself call global().
	detail::linkSDLImageHandler();
	detail::linkRadianceHDRHandler();
	detail::linkPFMImageHandler();
#if FCG_IMAGE_HAS_OPENEXR
	detail::linkOpenEXRImageHandler();
#endif
	return *loader;
}

auto ImageLoader::registerHandler (std::string id, std::unique_ptr<ImageFormatHandler> handler, int priority)
	-> std::expected<void, ImageError>
{
	if (id.empty() || id.find('\0') != std::string::npos || !handler)
		return std::unexpected(ImageError{ImageErrorCode::InvalidArgument, "Registration requires an ID and handler"});
	if (std::ranges::any_of(m_handlers, [&id] (const Entry &entry) { return entry.id == id; }))
		return std::unexpected(ImageError{ImageErrorCode::DuplicateHandler, "Handler ID is already registered: " + id});
	Entry entry{std::move(id), std::move(handler), priority};
	const auto position = std::ranges::find_if(m_handlers, [&entry] (const Entry &other) {
		return entry.priority > other.priority || (entry.priority == other.priority && entry.id < other.id);
	});
	m_handlers.insert(position, std::move(entry));
	return {};
}

auto ImageLoader::removeHandler (std::string_view id) -> bool {
	return std::erase_if(m_handlers, [id] (const Entry &entry) { return entry.id == id; }) != 0;
}

auto ImageLoader::fileFormats () const -> std::vector<ImageFileFormat>
{
	std::vector<ImageFileFormat> formats;
	for (const auto &entry : m_handlers) {
		auto advertised = entry.handler->fileFormats();
		formats.insert(formats.end(), std::make_move_iterator(advertised.begin()),
			std::make_move_iterator(advertised.end()));
	}
	return formats;
}

auto ImageLoader::load (const std::filesystem::path &path, ImageLoaderOptions options) const -> std::expected<Image, ImageError> {
	auto input = detail::readImageFile(path);
	if (!input)
		return std::unexpected(std::move(input.error()));
	return load(input->bytes, input->hint, options);
}

auto ImageLoader::load (
	std::span<const std::byte> bytes, std::string_view hint, ImageLoaderOptions options
) const -> std::expected<Image, ImageError>
{
	auto normalized = detail::imageHint(bytes, hint);
	if (!normalized)
		return std::unexpected(std::move(normalized.error()));
	std::string failures;
	for (const auto &entry : m_handlers) {
		auto accepted = entry.handler->accepts(bytes, *normalized);
		if (accepted && !*accepted)
			continue;
		if (accepted) {
			auto image = entry.handler->load(bytes, *normalized, options);
			if (image && image->handle())
				return image;
			if (!failures.empty())
				failures += '\n';
			failures += entry.id + ": " + (image ? "Handler returned an empty image" : image.error().message);
		} else {
			if (!failures.empty())
				failures += '\n';
			failures += entry.id + " (probe): " + accepted.error().message;
		}
	}
	if (failures.empty())
		return std::unexpected(
			ImageError{ImageErrorCode::UnsupportedFormat, "No image format handler accepted the input"});
	return std::unexpected(ImageError{ImageErrorCode::DecodeFailure, std::move(failures)});
}



////
// FormatHandlerRegistration

FormatHandlerRegistration::FormatHandlerRegistration (
	std::string id, std::unique_ptr<ImageFormatHandler> handler, int priority
){
	auto result = ImageLoader::global().registerHandler(std::move(id), std::move(handler), priority);
	if (!result)
		throw std::invalid_argument(result.error().message);
}



//////
//
// Module namespace close
//

// namespace fcg
}
