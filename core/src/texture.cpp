
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>

// SDL3 library
#include <SDL3/SDL.h>

// Framework
#include <FCG/texture.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Local shorthand for texture failure categories.
using Code = fcg::TextureErrorCode;

/// Construct an owned validation diagnostic.
auto error (Code code, std::string message) -> std::unexpected<fcg::TextureError> {
	return std::unexpected(fcg::TextureError{code, std::move(message)});
}

/// Capture SDL errors before cleanup.
auto sdlError (const char *operation) -> std::unexpected<fcg::TextureError> {
	return error(Code::SDLFailure, std::string(operation) + ": " + SDL_GetError());
}

/// Translate the staging facility's owned error.
auto transferError (const fcg::BufferError &source) -> std::unexpected<fcg::TextureError> {
	return error(source.code == fcg::BufferErrorCode::SDLFailure ? Code::SDLFailure : Code::InvalidState, source.message);
}

/// Effective byte geometry bounded by SDL's 32-bit allocation size.
struct Layout {

	/// Effective pitches.
	fcg::TextureTransferLayout bytes;

	/// Last exclusive byte of the texel region.
	std::size_t size;
};

/// Resolve pitches, validate divisibility and capacity, and reject all arithmetic overflow.
auto layoutFor (
	SDL_GPUTextureFormat format,
	glm::uvec3 extent,
	fcg::TextureTransferLayout layout,
	std::size_t capacity=std::numeric_limits<Uint32>::max(),
	bool gpuLayout=true
) -> std::expected<Layout, fcg::TextureError>
{
	auto texel = fcg::textureTexelSize(format);
	if (!texel)
		return std::unexpected(texel.error());
	constexpr auto limit = std::numeric_limits<Uint32>::max();
	if (!extent.x || !extent.y || !extent.z || layout.offset > limit || layout.rowPitch > limit || layout.slicePitch > limit)
		return error(Code::InvalidArgument, "Texture layout dimensions or byte counts are invalid");
	const uint64_t rowBytes = uint64_t(extent.x) * *texel;
	if (rowBytes > limit)
		return error(Code::InvalidArgument, "Texture row size exceeds SDL's byte limit");
	if (!layout.rowPitch)
		layout.rowPitch = (std::size_t)rowBytes;
	const uint64_t sliceBytes = uint64_t(layout.rowPitch) * extent.y;
	if (sliceBytes > limit)
		return error(Code::InvalidArgument, "Texture slice size exceeds SDL's byte limit");
	if (!layout.slicePitch)
		layout.slicePitch = (std::size_t)sliceBytes;
	if (layout.rowPitch < rowBytes || layout.slicePitch < sliceBytes
		|| (gpuLayout && (layout.rowPitch % *texel || layout.offset % *texel || layout.slicePitch % layout.rowPitch)))
		return error(Code::InvalidArgument, "Texture pitches or offset do not fit texel geometry");
	const uint64_t end = uint64_t(layout.offset) + uint64_t(extent.z - 1) * layout.slicePitch
		+ uint64_t(extent.y - 1) * layout.rowPitch + rowBytes;
	if (end > limit || end > capacity)
		return error(Code::InvalidArgument, "Texture layout exceeds allocation capacity or SDL's byte limit");
	return Layout{layout, (std::size_t)end};
}

/// Select 256-byte-aligned automatic staging rows.
auto alignedLayout (SDL_GPUTextureFormat format, glm::uvec3 extent) -> std::expected<Layout, fcg::TextureError> {
	auto texel = fcg::textureTexelSize(format);
	if (!texel)
		return std::unexpected(texel.error());
	const uint64_t row = (uint64_t(extent.x) * *texel + 255) & ~uint64_t(255);
	return layoutFor(format, extent, {0, (std::size_t)row, 0});
}

/// SDL descriptor with already validated byte pitches.
auto transferInfo (const fcg::TransferBuffer &storage, const Layout &layout, std::size_t texel)
	-> SDL_GPUTextureTransferInfo
{
	return {storage.handle(), (Uint32)layout.bytes.offset, (Uint32)(layout.bytes.rowPitch / texel),
		(Uint32)(layout.bytes.slicePitch / layout.bytes.rowPitch)};
}

/// Borrow one validated texture region for SDL.
auto regionInfo (SDL_GPUTexture *handle, const fcg::TextureRegion &region) -> SDL_GPUTextureRegion {
	return {handle, region.mipLevel, region.layer, region.offset.x, region.offset.y, region.offset.z,
		region.extent.x, region.extent.y, region.extent.z};
}

/// Validate staging state before encoding any command.
auto staging (const fcg::Texture &texture, const fcg::TransferBuffer &storage, SDL_GPUTransferBufferUsage usage)
	-> std::expected<void, fcg::TextureError>
{
	if (!storage.handle() || storage.mapped())
		return error(Code::InvalidState, "Texture transfers require live unmapped staging");
	if (texture.device() != storage.device() || storage.usage() != usage)
		return error(Code::InvalidArgument, "Texture transfer device or direction does not match");
	return {};
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

namespace fcg {



//////
//
// Function implementations
//

auto textureTexelSize (SDL_GPUTextureFormat format) -> std::expected<std::size_t, TextureError>
{
	if ((format >= SDL_GPU_TEXTUREFORMAT_A8_UNORM && format <= SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM)
		|| (format >= SDL_GPU_TEXTUREFORMAT_R8_SNORM && format <= SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
		|| format == SDL_GPU_TEXTUREFORMAT_D32_FLOAT)
		return SDL_GPUTextureFormatTexelBlockSize(format);
	return error(Code::UnsupportedFormat, "Texture transfers support uncompressed color and D32_FLOAT only");
}



//////
//
// Class implementations
//

////
// Texture

auto Texture::create (Device &device, const SDL_GPUTextureCreateInfo &info) -> std::expected<Texture, TextureError>
{
	constexpr auto allUsage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
		| SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ
		| SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE
		| SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE;
	const bool volume = info.type == SDL_GPU_TEXTURETYPE_3D;
	const bool cube = info.type == SDL_GPU_TEXTURETYPE_CUBE || info.type == SDL_GPU_TEXTURETYPE_CUBE_ARRAY;
	const bool depth = info.format >= SDL_GPU_TEXTUREFORMAT_D16_UNORM && info.format <= SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
	const bool integer = info.format >= SDL_GPU_TEXTUREFORMAT_R8_UINT && info.format <= SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT;
	const auto maxDim = volume ? 2048u : 16384u;
	if (info.type < SDL_GPU_TEXTURETYPE_2D || info.type > SDL_GPU_TEXTURETYPE_CUBE_ARRAY
		|| info.format <= SDL_GPU_TEXTUREFORMAT_INVALID || info.format > SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT
		|| !info.width || !info.height || !info.layer_count_or_depth || !info.num_levels
		|| info.width > maxDim || info.height > maxDim || (volume && info.layer_count_or_depth > maxDim)
		|| (!volume && info.layer_count_or_depth > 2048)
		|| !info.usage || (info.usage & ~allUsage)
		|| info.sample_count < SDL_GPU_SAMPLECOUNT_1 || info.sample_count > SDL_GPU_SAMPLECOUNT_8
		|| (info.type == SDL_GPU_TEXTURETYPE_2D && info.layer_count_or_depth != 1)
		|| (cube && (info.width != info.height || info.layer_count_or_depth % 6))
		|| (info.type == SDL_GPU_TEXTURETYPE_CUBE && info.layer_count_or_depth != 6)
		|| info.num_levels > std::bit_width(std::max({info.width, info.height, volume ? info.layer_count_or_depth : 1u})))
		return error(Code::InvalidArgument, "Texture creation dimensions, type, levels, format, or usage are invalid");
	if (((info.usage & SDL_GPU_TEXTUREUSAGE_SAMPLER) && (info.usage & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ))
		|| (depth && (info.usage & ~(SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)))
		|| (!depth && (info.usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
		|| (integer && (info.usage & SDL_GPU_TEXTUREUSAGE_SAMPLER))
		|| (volume && (info.usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
		|| (info.sample_count != SDL_GPU_SAMPLECOUNT_1 && (info.type != SDL_GPU_TEXTURETYPE_2D || info.num_levels != 1
			|| (info.usage & ~(SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)))))
		return error(Code::InvalidArgument, "Texture usage and sample configuration are incompatible");
	if (!SDL_GPUTextureSupportsFormat(device.handle(), info.format, info.type, info.usage)
		|| !SDL_GPUTextureSupportsSampleCount(device.handle(), info.format, info.sample_count))
		return error(Code::UnsupportedFormat, "Texture format, usage, or sample count is unsupported by the device");
	auto *handle = SDL_CreateGPUTexture(device.handle(), &info);
	if (!handle)
		return sdlError("Creating texture");
	return Texture(device, handle, info);
}

Texture::~Texture() {
	if (m_handle)
		SDL_ReleaseGPUTexture(m_device->handle(), m_handle);
}

Texture::Texture(Texture &&other) noexcept
	: m_device(std::exchange(other.m_device, nullptr)), m_handle(std::exchange(other.m_handle, nullptr)),
	  m_info(std::exchange(other.m_info, {}))
{}

auto Texture::operator= (Texture &&other) noexcept -> Texture&
{
	if (this != &other) {
		if (m_handle)
			SDL_ReleaseGPUTexture(m_device->handle(), m_handle);
		m_device = std::exchange(other.m_device, nullptr);
		m_handle = std::exchange(other.m_handle, nullptr);
		m_info = std::exchange(other.m_info, {});
	}
	return *this;
}

auto Texture::baseRegion () const -> TextureRegion {
	return {0, 0, glm::uvec3(0), {m_info.width, m_info.height,
		m_info.type == SDL_GPU_TEXTURETYPE_3D ? m_info.layer_count_or_depth : 1u}};
}

auto Texture::validateRegion (const TextureRegion &region) const -> std::expected<void, TextureError>
{
	if (!m_handle)
		return error(Code::InvalidState, "Texture is empty");
	if (m_info.sample_count != SDL_GPU_SAMPLECOUNT_1)
		return error(Code::UnsupportedFormat, "Multisampled texture transfers are unsupported");
	if (auto size = textureTexelSize(m_info.format); !size)
		return std::unexpected(size.error());
	if (region.mipLevel >= m_info.num_levels)
		return error(Code::InvalidArgument, "Texture mip level is out of bounds");
	const bool volume = m_info.type == SDL_GPU_TEXTURETYPE_3D;
	const glm::uvec3 bounds{std::max(1u, m_info.width >> region.mipLevel),
		std::max(1u, m_info.height >> region.mipLevel),
		volume ? std::max(1u, m_info.layer_count_or_depth >> region.mipLevel) : 1u};
	if (region.layer >= (volume ? 1u : m_info.layer_count_or_depth)
		|| glm::any(glm::equal(region.extent, glm::uvec3(0)))
		|| glm::any(glm::greaterThan(region.offset, bounds))
		|| glm::any(glm::greaterThan(region.extent, bounds - region.offset)))
		return error(Code::InvalidArgument, "Texture region is out of bounds");
	return {};
}

auto Texture::uploadFrom (
	SDL_GPUCopyPass *pass,
	const TransferBuffer &source,
	const TextureRegion &region,
	TextureTransferLayout layout,
	bool cycle
) -> std::expected<void, TextureError>
{
	if (auto valid = validateRegion(region); !valid)
		return valid;
	if (!pass)
		return error(Code::InvalidArgument, "Texture upload requires a copy pass");
	if (auto valid = staging(*this, source, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD); !valid)
		return valid;
	auto resolved = layoutFor(m_info.format, region.extent, layout, source.size());
	if (!resolved)
		return std::unexpected(resolved.error());
	const auto from = transferInfo(source, *resolved, *textureTexelSize(m_info.format));
	const auto to = regionInfo(m_handle, region);
	SDL_UploadToGPUTexture(pass, &from, &to, cycle);
	return {};
}

auto Texture::downloadTo (
	SDL_GPUCopyPass *pass,
	const TransferBuffer &destination,
	const TextureRegion &region,
	TextureTransferLayout layout
) const -> std::expected<void, TextureError>
{
	if (auto valid = validateRegion(region); !valid)
		return valid;
	if (!pass)
		return error(Code::InvalidArgument, "Texture download requires a copy pass");
	if (auto valid = staging(*this, destination, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD); !valid)
		return valid;
	auto resolved = layoutFor(m_info.format, region.extent, layout, destination.size());
	if (!resolved)
		return std::unexpected(resolved.error());
	const auto from = regionInfo(m_handle, region);
	const auto to = transferInfo(destination, *resolved, *textureTexelSize(m_info.format));
	SDL_DownloadFromGPUTexture(pass, &from, &to);
	return {};
}

auto Texture::upload (
	std::span<const std::byte> data,
	const TextureRegion &region,
	TextureTransferLayout layout,
	bool cycle
) -> std::expected<void, TextureError>
{
	if (auto valid = validateRegion(region); !valid)
		return valid;
	auto input = layoutFor(m_info.format, region.extent, layout, data.size(), false);
	if (!input)
		return std::unexpected(input.error());
	auto output = alignedLayout(m_info.format, region.extent);
	if (!output)
		return std::unexpected(output.error());
	auto storage = TransferBuffer::create(*m_device, output->size, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
	if (!storage)
		return transferError(storage.error());
	auto mapping = storage->map();
	if (!mapping)
		return transferError(mapping.error());
	const auto rowBytes = region.extent.x * *textureTexelSize(m_info.format);
	for (Uint32 z = 0; z < region.extent.z; ++z)
		for (Uint32 y = 0; y < region.extent.y; ++y)
			std::memcpy(mapping->data().data() + z * output->bytes.slicePitch + y * output->bytes.rowPitch,
				data.data() + input->bytes.offset + z * input->bytes.slicePitch + y * input->bytes.rowPitch, rowBytes);
	mapping->unmap();
	auto *command = SDL_AcquireGPUCommandBuffer(m_device->handle());
	if (!command)
		return sdlError("Acquiring texture upload commands");
	auto *pass = SDL_BeginGPUCopyPass(command);
	if (!pass) {
		auto failure = sdlError("Beginning texture upload pass");
		SDL_CancelGPUCommandBuffer(command);
		return failure;
	}
	auto recorded = uploadFrom(pass, *storage, region, output->bytes, cycle);
	SDL_EndGPUCopyPass(pass);
	if (!recorded) {
		SDL_CancelGPUCommandBuffer(command);
		return recorded;
	}
	if (!SDL_SubmitGPUCommandBuffer(command))
		return sdlError("Submitting texture upload");
	return {};
}

auto Texture::copyTo (
	SDL_GPUCopyPass *pass,
	Texture &destination,
	const TextureRegion &sourceRegion,
	const TextureRegion &destinationRegion,
	bool cycle
) const -> std::expected<void, TextureError>
{
	if (auto valid = validateRegion(sourceRegion); !valid)
		return valid;
	if (auto valid = destination.validateRegion(destinationRegion); !valid)
		return valid;
	if (!pass || m_device != destination.m_device || m_info.format != destination.m_info.format
		|| sourceRegion.extent != destinationRegion.extent)
		return error(Code::InvalidArgument, "Texture copy needs a pass, matching devices, formats, and extents");
	if (m_handle == destination.m_handle && sourceRegion.mipLevel == destinationRegion.mipLevel
		&& sourceRegion.layer == destinationRegion.layer)
		return error(Code::InvalidArgument, "Texture self-copy within a subresource is unsupported");
	const SDL_GPUTextureLocation from{m_handle, sourceRegion.mipLevel, sourceRegion.layer,
		sourceRegion.offset.x, sourceRegion.offset.y, sourceRegion.offset.z};
	const SDL_GPUTextureLocation to{destination.m_handle, destinationRegion.mipLevel, destinationRegion.layer,
		destinationRegion.offset.x, destinationRegion.offset.y, destinationRegion.offset.z};
	SDL_CopyGPUTextureToTexture(pass, &from, &to, sourceRegion.extent.x, sourceRegion.extent.y, sourceRegion.extent.z, cycle);
	return {};
}

auto Texture::readback (const TextureRegion &region) const -> std::expected<TextureReadback, TextureError> {
	if (auto valid = validateRegion(region); !valid)
		return std::unexpected(valid.error());
	return TextureReadback::create(*m_device, regionInfo(m_handle, region), m_info.format);
}


////
// TextureReadback

TextureReadback::TextureReadback(
	TransferBuffer &&storage,
	std::unique_ptr<Device::RetiredFence> fence,
	SDL_GPUTextureFormat format,
	TextureRegion region,
	TextureTransferLayout layout
)
	: m_storage(std::move(storage)), m_fence(std::move(fence)), m_format(format), m_region(region), m_layout(layout)
{}

auto TextureReadback::create (Device &device, const SDL_GPUTextureRegion &source, SDL_GPUTextureFormat format)
	-> std::expected<TextureReadback, TextureError>
{
	if (!source.texture || !source.w || !source.h || !source.d
		|| source.x > UINT32_MAX - source.w || source.y > UINT32_MAX - source.h || source.z > UINT32_MAX - source.d)
		return error(Code::InvalidArgument, "Texture readback source is invalid");
	const glm::uvec3 extent{source.w, source.h, source.d};
	auto layout = alignedLayout(format, extent);
	if (!layout)
		return std::unexpected(layout.error());
	device.collectRetiredFences();
	auto fence = std::make_unique<Device::RetiredFence>();
	auto storage = TransferBuffer::create(device, layout->size, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
	if (!storage)
		return transferError(storage.error());
	auto *command = SDL_AcquireGPUCommandBuffer(device.handle());
	if (!command)
		return sdlError("Acquiring texture readback commands");
	auto *pass = SDL_BeginGPUCopyPass(command);
	if (!pass) {
		auto failure = sdlError("Beginning texture readback pass");
		SDL_CancelGPUCommandBuffer(command);
		return failure;
	}
	const auto destination = transferInfo(*storage, *layout, *textureTexelSize(format));
	SDL_DownloadFromGPUTexture(pass, &source, &destination);
	SDL_EndGPUCopyPass(pass);
	fence->handle = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
	if (!fence->handle)
		return sdlError("Submitting texture readback");
	return TextureReadback(std::move(*storage), std::move(fence), format,
		{source.mip_level, source.layer, {source.x, source.y, source.z}, extent}, layout->bytes);
}

TextureReadback::~TextureReadback() {
	if (m_fence)
		m_storage.device()->retireFence(std::move(m_fence));
}

TextureReadback::TextureReadback(TextureReadback &&other) noexcept
	: m_storage(std::move(other.m_storage)), m_fence(std::move(other.m_fence)), m_format(other.m_format),
	  m_region(other.m_region), m_layout(other.m_layout)
{}

auto TextureReadback::operator= (TextureReadback &&other) noexcept -> TextureReadback&
{
	if (this != &other) {
		assert(!m_storage.mapped() && !other.m_storage.mapped());
		if (m_fence)
			m_storage.device()->retireFence(std::move(m_fence));
		m_storage = std::move(other.m_storage);
		m_fence = std::move(other.m_fence);
		m_format = other.m_format;
		m_region = other.m_region;
		m_layout = other.m_layout;
	}
	return *this;
}

auto TextureReadback::ready () const -> bool {
	return m_fence && SDL_QueryGPUFence(m_storage.device()->handle(), m_fence->handle);
}

auto TextureReadback::wait () const -> std::expected<void, TextureError> {
	if (!m_fence)
		return error(Code::InvalidState, "Texture readback is empty");
	if (!SDL_WaitForGPUFences(m_storage.device()->handle(), true, &m_fence->handle, 1))
		return sdlError("Waiting for texture readback");
	return {};
}

auto TextureReadback::map () -> std::expected<Mapping, TextureError>
{
	if (!m_fence)
		return error(Code::InvalidState, "Texture readback is empty");
	if (!ready())
		return error(Code::NotReady, "Texture readback has not completed");
	auto mapping = m_storage.map();
	if (!mapping)
		return transferError(mapping.error());
	return Mapping(std::move(*mapping), m_format, m_region.extent, m_layout, *textureTexelSize(m_format));
}


////
// Sampler

auto Sampler::create (Device &device, const SDL_GPUSamplerCreateInfo &info) -> std::expected<Sampler, TextureError>
{
	if (info.min_filter < SDL_GPU_FILTER_NEAREST || info.min_filter > SDL_GPU_FILTER_LINEAR
		|| info.mag_filter < SDL_GPU_FILTER_NEAREST || info.mag_filter > SDL_GPU_FILTER_LINEAR
		|| info.mipmap_mode < SDL_GPU_SAMPLERMIPMAPMODE_NEAREST || info.mipmap_mode > SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
		|| info.address_mode_u < SDL_GPU_SAMPLERADDRESSMODE_REPEAT || info.address_mode_u > SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
		|| info.address_mode_v < SDL_GPU_SAMPLERADDRESSMODE_REPEAT || info.address_mode_v > SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
		|| info.address_mode_w < SDL_GPU_SAMPLERADDRESSMODE_REPEAT || info.address_mode_w > SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
		|| !std::isfinite(info.min_lod) || !std::isfinite(info.max_lod) || !std::isfinite(info.mip_lod_bias)
		|| info.min_lod < 0.f || info.max_lod < info.min_lod
		|| (info.enable_anisotropy && (!std::isfinite(info.max_anisotropy) || info.max_anisotropy < 1.f || info.max_anisotropy > 16.f))
		|| (info.enable_compare && (info.compare_op < SDL_GPU_COMPAREOP_NEVER || info.compare_op > SDL_GPU_COMPAREOP_ALWAYS)))
		return error(Code::InvalidArgument, "Sampler descriptor is invalid");
	auto *handle = SDL_CreateGPUSampler(device.handle(), &info);
	if (!handle)
		return sdlError("Creating sampler");
	return Sampler(device, handle);
}

Sampler::~Sampler() {
	if (m_handle)
		SDL_ReleaseGPUSampler(m_device->handle(), m_handle);
}

Sampler::Sampler(Sampler &&other) noexcept
	: m_device(std::exchange(other.m_device, nullptr)), m_handle(std::exchange(other.m_handle, nullptr))
{}

auto Sampler::operator= (Sampler &&other) noexcept -> Sampler&
{
	if (this != &other) {
		if (m_handle)
			SDL_ReleaseGPUSampler(m_device->handle(), m_handle);
		m_device = std::exchange(other.m_device, nullptr);
		m_handle = std::exchange(other.m_handle, nullptr);
	}
	return *this;
}



//////
//
// Module namespace close
//

} // namespace fcg
