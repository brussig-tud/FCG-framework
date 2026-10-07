
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

// Local includes
#include "FCG/buffer.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

// Convenience shorthands
using fcg::BufferError;
using fcg::BufferErrorCode;

/// Construct a validation error without modifying SDL's thread-local diagnostic.
auto error (BufferErrorCode code, const char *message) -> std::unexpected<BufferError> {
	return std::unexpected(BufferError{code, message});
}

/// Capture SDL's diagnostic before any cleanup calls can overwrite it.
auto sdlError (const char *operation) -> std::unexpected<BufferError> {
	return std::unexpected(BufferError{BufferErrorCode::SDLFailure, std::string(operation) + ": " + SDL_GetError()});
}

/// Check a range without overflowing `offset + size`; an empty range may start at the end.
auto fits (std::size_t capacity, std::size_t offset, std::size_t size) -> bool {
	return offset <= capacity && size <= capacity - offset;
}

/// Validate a live buffer and logical byte range before narrowing offsets for SDL.
auto range (const fcg::Buffer &buffer, std::size_t offset, std::size_t size) -> std::expected<void, BufferError> {
	if (!buffer.handle())
		return error(BufferErrorCode::InvalidState, "Buffer is empty");
	if (!fits(buffer.size(), offset, size))
		return error(BufferErrorCode::InvalidArgument, "Buffer byte range is out of bounds");
	return {};
}

/// Check a binding's resource usage before encoding any SDL commands.
auto requiresUsage (const fcg::Buffer &buffer, SDL_GPUBufferUsageFlags usage) -> std::expected<void, BufferError> {
	if (!buffer.handle())
		return error(BufferErrorCode::InvalidState, "Buffer is empty");
	if (!(buffer.usage() & usage))
		return error(BufferErrorCode::InvalidArgument, "Buffer was not created for this binding usage");
	return {};
}

/// Upper limit of all SDL allocation and transfer size fields.
constexpr auto maxSize = std::size_t(std::numeric_limits<Uint32>::max());

// Anonymous namespace end
}



//////
//
// Module namespace open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// TransferBuffer

auto TransferBuffer::create (
	Device &device, std::size_t size, SDL_GPUTransferBufferUsage usage, SDL_PropertiesID properties
) -> std::expected<TransferBuffer, BufferError>
{
	if (!size || size > maxSize)
		return error(BufferErrorCode::InvalidArgument, "Transfer allocation size must be in [1, UINT32_MAX]");
	if (usage != SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD && usage != SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD)
		return error(BufferErrorCode::InvalidArgument, "Invalid transfer buffer direction");
	const SDL_GPUTransferBufferCreateInfo info {
		.usage = usage, .size = static_cast<Uint32>(size), .props = properties
	};
	auto *handle = SDL_CreateGPUTransferBuffer(device.handle(), &info);
	if (!handle)
		return sdlError("Creating transfer buffer");
	return TransferBuffer(device, handle, size, usage);
}

TransferBuffer::~TransferBuffer () {
	assert(!m_mapped);
	if (m_handle)
		SDL_ReleaseGPUTransferBuffer(m_device->handle(), m_handle);
}

TransferBuffer::TransferBuffer (TransferBuffer &&other) noexcept {
	*this = std::move(other);
}

auto TransferBuffer::operator= (TransferBuffer &&other) noexcept -> TransferBuffer&
{
	if (this != &other) {
		assert(!m_mapped && !other.m_mapped);
		if (m_handle)
			SDL_ReleaseGPUTransferBuffer(m_device->handle(), m_handle);
		m_device = std::exchange(other.m_device, nullptr);
		m_handle = std::exchange(other.m_handle, nullptr);
		m_size = std::exchange(other.m_size, 0);
		m_usage = other.m_usage;
	}
	return *this;
}

auto TransferBuffer::map (bool cycle) -> std::expected<Mapping, BufferError>
{
	if (!m_handle || m_mapped)
		return error(BufferErrorCode::InvalidState, "Transfer buffer is empty or already mapped");
	if (cycle && m_usage == SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD)
		return error(BufferErrorCode::InvalidArgument, "Cycling a download mapping would discard its result");
	auto *data = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(m_device->handle(), m_handle, cycle));
	if (!data)
		return sdlError("Mapping transfer buffer");
	m_mapped = true;
	return Mapping(*this, {data, m_size});
}


////
// TransferBuffer::Mapping

TransferBuffer::Mapping::~Mapping () { unmap(); }

TransferBuffer::Mapping::Mapping (Mapping &&other) noexcept
	: owner(std::exchange(other.owner, nullptr)), m_data(std::exchange(other.m_data, {}))
{}

auto TransferBuffer::Mapping::operator= (Mapping &&other) noexcept -> Mapping&
{
	if (this != &other) {
		unmap();
		owner = std::exchange(other.owner, nullptr);
		m_data = std::exchange(other.m_data, {});
	}
	return *this;
}

void TransferBuffer::Mapping::unmap ()
{
	if (owner) {
		SDL_UnmapGPUTransferBuffer(owner->m_device->handle(), owner->m_handle);
		owner->m_mapped = false;
		owner = nullptr;
		m_data = {};
	}
}


////
// BufferReadback

BufferReadback::BufferReadback (TransferBuffer &&storage, std::unique_ptr<Device::RetiredFence> fence)
	: m_storage(std::move(storage)), m_fence(std::move(fence))
{}

BufferReadback::~BufferReadback () {
	if (m_fence)
		m_storage.device()->retireFence(std::move(m_fence));
}

BufferReadback::BufferReadback (BufferReadback &&other) noexcept
	: m_storage(std::move(other.m_storage)), m_fence(std::move(other.m_fence))
{}

auto BufferReadback::operator= (BufferReadback &&other) noexcept -> BufferReadback&
{
	if (this != &other) {
		assert(!m_storage.mapped() && !other.m_storage.mapped());
		if (m_fence)
			m_storage.device()->retireFence(std::move(m_fence));
		m_storage = std::move(other.m_storage);
		m_fence = std::move(other.m_fence);
	}
	return *this;
}

auto BufferReadback::ready () const -> bool {
	return m_fence && SDL_QueryGPUFence(m_storage.device()->handle(), m_fence->handle);
}

auto BufferReadback::wait () const -> std::expected<void, BufferError> {
	if (!m_fence)
		return error(BufferErrorCode::InvalidState, "Readback ticket is empty");
	if (!SDL_WaitForGPUFences(m_storage.device()->handle(), true, &m_fence->handle, 1))
		return sdlError("Waiting for buffer readback");
	return {};
}

auto BufferReadback::map () -> std::expected<TransferBuffer::Mapping, BufferError> {
	if (!m_fence)
		return error(BufferErrorCode::InvalidState, "Readback ticket is empty");
	if (!ready())
		return error(BufferErrorCode::NotReady, "Buffer readback has not completed");
	return m_storage.map();
}


////
// Buffer

auto Buffer::create (
	Device &device, std::size_t size, SDL_GPUBufferUsageFlags usage, SDL_PropertiesID properties
) -> std::expected<Buffer, BufferError>
{
	if (!size || size > maxSize)
		return error(BufferErrorCode::InvalidArgument, "GPU allocation size must be in [1, UINT32_MAX]");
	if (!usage || ((usage & SDL_GPU_BUFFERUSAGE_VERTEX) && (usage & SDL_GPU_BUFFERUSAGE_INDEX)))
		return error(BufferErrorCode::InvalidArgument, "Buffer needs nonzero usage; VERTEX and INDEX cannot be combined");
	const SDL_GPUBufferCreateInfo info {
		.usage = usage, .size = static_cast<Uint32>(std::max(size, std::size_t(4))), .props = properties
	};
	auto *handle = SDL_CreateGPUBuffer(device.handle(), &info);
	if (!handle)
		return sdlError("Creating GPU buffer");
	return Buffer(device, handle, size, usage);
}

auto Buffer::create (
	Device &device, std::span<const std::byte> data, SDL_GPUBufferUsageFlags usage, SDL_PropertiesID properties
) -> std::expected<Buffer, BufferError>
{
	auto result = create(device, data.size(), usage, properties);
	if (!result)
		return result;
	if (auto uploaded = result->upload(data); !uploaded)
		return std::unexpected(uploaded.error());
	return result;
}

Buffer::~Buffer () {
	if (m_handle)
		SDL_ReleaseGPUBuffer(m_device->handle(), m_handle);
}

Buffer::Buffer (Buffer &&other) noexcept {
	*this = std::move(other);
}

auto Buffer::operator= (Buffer &&other) noexcept -> Buffer&
{
	if (this != &other)
	{
		if (m_handle)
			SDL_ReleaseGPUBuffer(m_device->handle(), m_handle);
		m_device = std::exchange(other.m_device, nullptr);
		m_handle = std::exchange(other.m_handle, nullptr);
		m_size = std::exchange(other.m_size, 0);
		m_usage = std::exchange(other.m_usage, 0);
	}
	return *this;
}

auto Buffer::upload (std::span<const std::byte> data, std::size_t offset, bool cycle)
	-> std::expected<void, BufferError>
{
	if (auto checked = range(*this, offset, data.size()); !checked)
		return checked;
	UploadBatch batch(*m_device);
	if (auto staged = batch.upload(*this, data, offset, cycle); !staged)
		return staged;
	return batch.submit();
}

auto Buffer::uploadFrom (
	SDL_GPUCopyPass *pass, const TransferBuffer &source, std::size_t size, std::size_t sourceOffset,
	std::size_t destinationOffset, bool cycle
) -> std::expected<void, BufferError>
{
	if (auto checked = range(*this, destinationOffset, size); !checked)
		return checked;
	if (!source.handle() || source.mapped())
		return error(BufferErrorCode::InvalidState, "Upload staging must be live and unmapped");
	if (!pass || source.device() != m_device || source.usage() != SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
		|| !fits(source.size(), sourceOffset, size))
		return error(BufferErrorCode::InvalidArgument, "Invalid upload pass, device, direction, or source range");
	if (!size)
		return {};
	const SDL_GPUTransferBufferLocation src {source.handle(), static_cast<Uint32>(sourceOffset)};
	const SDL_GPUBufferRegion dst {m_handle, static_cast<Uint32>(destinationOffset), static_cast<Uint32>(size)};
	SDL_UploadToGPUBuffer(pass, &src, &dst, cycle);
	return {};
}

auto Buffer::downloadTo (
	SDL_GPUCopyPass *pass, TransferBuffer &destination, std::size_t size, std::size_t sourceOffset,
	std::size_t destinationOffset
) const -> std::expected<void, BufferError>
{
	if (auto checked = range(*this, sourceOffset, size); !checked)
		return checked;
	if (!destination.handle() || destination.mapped())
		return error(BufferErrorCode::InvalidState, "Download staging must be live and unmapped");
	if (!pass || !size || destination.device() != m_device
		|| destination.usage() != SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD
		|| !fits(destination.size(), destinationOffset, size))
		return error(BufferErrorCode::InvalidArgument, "Invalid download pass, device, direction, or destination range");
	const SDL_GPUBufferRegion src {m_handle, static_cast<Uint32>(sourceOffset), static_cast<Uint32>(size)};
	const SDL_GPUTransferBufferLocation dst {destination.handle(), static_cast<Uint32>(destinationOffset)};
	SDL_DownloadFromGPUBuffer(pass, &src, &dst);
	return {};
}

auto Buffer::copyTo (
	SDL_GPUCopyPass *pass, Buffer &destination, std::size_t size, std::size_t sourceOffset,
	std::size_t destinationOffset, bool cycle
) const -> std::expected<void, BufferError>
{
	if (auto checked = range(*this, sourceOffset, size); !checked)
		return checked;
	if (auto checked = range(destination, destinationOffset, size); !checked)
		return checked;
	if (!pass || destination.device() != m_device)
		return error(BufferErrorCode::InvalidArgument, "Copy requires a pass and buffers on the same device");
	if (!size)
		return {};
	if (m_handle == destination.handle()
		&& (cycle || (sourceOffset < destinationOffset + size && destinationOffset < sourceOffset + size)))
		return error(BufferErrorCode::InvalidArgument, "Self-copy cannot overlap or cycle its destination");
	const SDL_GPUBufferLocation src {m_handle, static_cast<Uint32>(sourceOffset)};
	const SDL_GPUBufferLocation dst {destination.handle(), static_cast<Uint32>(destinationOffset)};
	SDL_CopyGPUBufferToBuffer(pass, &src, &dst, static_cast<Uint32>(size), cycle);
	return {};
}

auto Buffer::readback (std::size_t offset, std::size_t size) const -> std::expected<BufferReadback, BufferError>
{
	if (auto checked = range(*this, offset, 0); !checked)
		return std::unexpected(checked.error());
	if (size == std::dynamic_extent)
		size = m_size - offset;
	if (auto checked = range(*this, offset, size); !checked)
		return std::unexpected(checked.error());
	if (!size)
		return error(BufferErrorCode::InvalidArgument, "Readback range must not be empty");
	m_device->collectRetiredFences();
	auto fenceOwner = std::make_unique<Device::RetiredFence>();
	auto storage = TransferBuffer::create(*m_device, size, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
	if (!storage)
		return std::unexpected(storage.error());
	auto *command = SDL_AcquireGPUCommandBuffer(m_device->handle());
	if (!command)
		return sdlError("Acquiring readback command buffer");
	auto *pass = SDL_BeginGPUCopyPass(command);
	if (!pass) {
		auto failure = sdlError("Beginning readback copy pass");
		SDL_CancelGPUCommandBuffer(command);
		return failure;
	}
	auto copied = downloadTo(pass, *storage, size, offset);
	SDL_EndGPUCopyPass(pass);
	if (!copied) {
		SDL_CancelGPUCommandBuffer(command);
		return std::unexpected(copied.error());
	}
	auto *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
	if (!fence)
		return sdlError("Submitting buffer readback");
	fenceOwner->handle = fence;
	return BufferReadback(std::move(*storage), std::move(fenceOwner));
}


auto Buffer::binding (std::size_t offset) const -> std::expected<SDL_GPUBufferBinding, BufferError> {
	if (auto checked = requiresUsage(*this, SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_INDEX); !checked)
		return std::unexpected(checked.error());
	if (auto checked = range(*this, offset, 1); !checked)
		return std::unexpected(checked.error());
	return SDL_GPUBufferBinding{m_handle, static_cast<Uint32>(offset)};
}

auto Buffer::readWriteBinding (bool cycle) const -> std::expected<SDL_GPUStorageBufferReadWriteBinding, BufferError> {
	if (auto checked = requiresUsage(*this, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE); !checked)
		return std::unexpected(checked.error());
	return SDL_GPUStorageBufferReadWriteBinding{.buffer = m_handle, .cycle = cycle};
}

auto Buffer::bindVertex (SDL_GPURenderPass *pass, Uint32 slot, std::size_t offset) const
	-> std::expected<void, BufferError>
{
	if (auto checked = requiresUsage(*this, SDL_GPU_BUFFERUSAGE_VERTEX); !checked)
		return checked;
	if (!pass)
		return error(BufferErrorCode::InvalidArgument, "Vertex binding needs a render pass");
	auto descriptor = binding(offset);
	if (!descriptor)
		return std::unexpected(descriptor.error());
	SDL_BindGPUVertexBuffers(pass, slot, &*descriptor, 1);
	return {};
}

auto Buffer::bindIndex (SDL_GPURenderPass *pass, SDL_GPUIndexElementSize elementSize, std::size_t offset) const
	-> std::expected<void, BufferError>
{
	if (auto checked = requiresUsage(*this, SDL_GPU_BUFFERUSAGE_INDEX); !checked)
		return checked;
	if (!pass || (elementSize != SDL_GPU_INDEXELEMENTSIZE_16BIT && elementSize != SDL_GPU_INDEXELEMENTSIZE_32BIT))
		return error(BufferErrorCode::InvalidArgument, "Index binding needs a pass and a 16/32-bit element size");
	const std::size_t stride = elementSize == SDL_GPU_INDEXELEMENTSIZE_16BIT ? 2 : 4;
	if (offset % stride || !fits(m_size, offset, stride))
		return error(BufferErrorCode::InvalidArgument, "Index offset is misaligned or has no complete element");
	const SDL_GPUBufferBinding descriptor {m_handle, static_cast<Uint32>(offset)};
	SDL_BindGPUIndexBuffer(pass, &descriptor, elementSize);
	return {};
}

auto Buffer::bindStorage (SDL_GPURenderPass *pass, ShaderStage stage, Uint32 slot) const
	-> std::expected<void, BufferError>
{
	if (auto checked = requiresUsage(*this, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ); !checked)
		return checked;
	if (!pass || (stage != ShaderStage::VERTEX && stage != ShaderStage::FRAGMENT))
		return error(BufferErrorCode::InvalidArgument, "Graphics storage needs a render pass and graphics stage");
	if (stage == ShaderStage::VERTEX)
		SDL_BindGPUVertexStorageBuffers(pass, slot, &m_handle, 1);
	else
		SDL_BindGPUFragmentStorageBuffers(pass, slot, &m_handle, 1);
	return {};
}

auto Buffer::bindStorage (SDL_GPUComputePass *pass, Uint32 slot) const -> std::expected<void, BufferError>
{
	if (auto checked = requiresUsage(*this, SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ); !checked)
		return checked;
	if (!pass)
		return error(BufferErrorCode::InvalidArgument, "Compute storage binding needs a compute pass");
	SDL_BindGPUComputeStorageBuffers(pass, slot, &m_handle, 1);
	return {};
}


////
// UploadBatch

/// One stable-address upload allocation. Member order ensures unmapping precedes storage destruction.
struct UploadBatch::Page
{
	////
	// Object construction/destruction

	/// Construct a page with its required allocation before any mapping can borrow it.
	/// \param buffer Successfully created, unmapped upload storage; ownership is transferred here.
	explicit Page (TransferBuffer &&buffer) noexcept : storage(std::move(buffer)) {}


	////
	// Fields

	/// Owned upload allocation.
	TransferBuffer storage;

	/// Present while accumulating this batch's writes.
	std::optional<TransferBuffer::Mapping> mapping;

	/// Bytes occupied by staged writes, including inter-write padding.
	std::size_t used = 0;
};

UploadBatch::UploadBatch (Device &device) : device(&device) {}

UploadBatch::~UploadBatch () = default;

UploadBatch::UploadBatch (UploadBatch &&other) noexcept
	: device(std::exchange(other.device, nullptr)), pages(std::move(other.pages)),
	  copies(std::move(other.copies))
{}

auto UploadBatch::operator= (UploadBatch &&other) noexcept -> UploadBatch&
{
	if (this != &other) {
		clear();
		pages = std::move(other.pages);
		copies = std::move(other.copies);
		device = std::exchange(other.device, nullptr);
	}
	return *this;
}

auto UploadBatch::upload (Buffer &destination, std::span<const std::byte> data, std::size_t offset, bool cycle)
	-> std::expected<void, BufferError>
{
	if (!device)
		return error(BufferErrorCode::InvalidState, "Upload batch has been moved from");
	if (auto checked = range(destination, offset, data.size()); !checked)
		return checked;
	if (destination.device() != device)
		return error(BufferErrorCode::InvalidArgument, "Upload destination belongs to another device");
	if (data.empty())
		return {};

	// Reuse an existing page first; offsets are aligned for efficient packed transfers.
	std::size_t index = 0;
	std::size_t sourceOffset = 0;
	for (; index < pages.size(); ++index)
	{
		const auto &page = *pages[index];
		const auto padding = (16 - page.used % 16) % 16;
		if (padding <= page.storage.size() - page.used) {
			sourceOffset = page.used + padding;
			if (fits(page.storage.size(), sourceOffset, data.size()))
				break;
		}
	}
	if (index == pages.size())
	{
		const auto previous = pages.empty() ? std::size_t(32768) : pages.back()->storage.size();
		const auto capacity = std::max(data.size(), previous > maxSize / 2 ? maxSize : previous * 2);
		auto storage = TransferBuffer::create(*device, capacity, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
		if (!storage)
			return std::unexpected(storage.error());
		auto page = std::make_unique<Page>(std::move(*storage));
		pages.push_back(std::move(page));
		sourceOffset = 0;
	}
	auto &page = *pages[index];
	if (!page.mapping) {
		auto mapping = page.storage.map(true);
		if (!mapping)
			return std::unexpected(mapping.error());
		page.mapping.emplace(std::move(*mapping));
	}

	// Allocate the descriptor before changing the page cursor; allocation failure leaves earlier uploads intact.
	copies.push_back({index, static_cast<Uint32>(sourceOffset),
		{destination.handle(), static_cast<Uint32>(offset), static_cast<Uint32>(data.size())}, cycle});
	std::memcpy(page.mapping->data().data() + sourceOffset, data.data(), data.size());
	page.used = sourceOffset + data.size();
	return {};
}

auto UploadBatch::record (SDL_GPUCopyPass *pass) -> std::expected<void, BufferError>
{
	if (!device)
		return error(BufferErrorCode::InvalidState, "Upload batch has been moved from");
	if (!pass)
		return error(BufferErrorCode::InvalidArgument, "Recording uploads requires a copy pass");
	for (auto &page : pages)
		page->mapping.reset();
	for (const auto &copy : copies) {
		const SDL_GPUTransferBufferLocation source {pages[copy.page]->storage.handle(), copy.sourceOffset};
		SDL_UploadToGPUBuffer(pass, &source, &copy.destination, copy.cycle);
	}
	clear();
	return {};
}

auto UploadBatch::submit () -> std::expected<void, BufferError>
{
	if (!device)
		return error(BufferErrorCode::InvalidState, "Upload batch has been moved from");
	if (copies.empty())
		return {};
	auto *command = SDL_AcquireGPUCommandBuffer(device->handle());
	if (!command)
		return sdlError("Acquiring upload command buffer");
	auto *pass = SDL_BeginGPUCopyPass(command);
	if (!pass) {
		auto failure = sdlError("Beginning upload copy pass");
		SDL_CancelGPUCommandBuffer(command);
		return failure;
	}
	auto recorded = record(pass);
	SDL_EndGPUCopyPass(pass);
	if (!recorded) {
		SDL_CancelGPUCommandBuffer(command);
		return recorded;
	}
	if (!SDL_SubmitGPUCommandBuffer(command))
		return sdlError("Submitting buffer uploads");
	return {};
}

void UploadBatch::clear () {
	copies.clear();
	for (auto &page : pages) {
		page->mapping.reset();
		page->used = 0;
	}
}



//////
//
// Functions
//

auto pushUniforms (
	SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, Uint32 slot, std::span<const std::byte> data
) -> std::expected<void, BufferError>
{
	if (!commandBuffer || slot >= 4 || data.empty() || data.size() > 4096)
		return error(BufferErrorCode::InvalidArgument, "Uniform push needs a command buffer, slot [0,3], and 1..4096 bytes");
	switch (stage)
	{
		case ShaderStage::VERTEX:
			SDL_PushGPUVertexUniformData(commandBuffer, slot, data.data(), static_cast<Uint32>(data.size()));
			break;
		case ShaderStage::FRAGMENT:
			SDL_PushGPUFragmentUniformData(commandBuffer, slot, data.data(), static_cast<Uint32>(data.size()));
			break;
		case ShaderStage::COMPUTE:
			SDL_PushGPUComputeUniformData(commandBuffer, slot, data.data(), static_cast<Uint32>(data.size()));
			break;
		default:
			return error(BufferErrorCode::InvalidArgument, "Unknown uniform shader stage");
	}
	return {};
}



//////
//
// Module namespace close
//

} // namespace fcg
