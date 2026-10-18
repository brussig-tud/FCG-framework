
#ifndef __FCG_BUFFER_H__
#define __FCG_BUFFER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_buffers Buffer facilities
/// \ingroup fcg_modules
/// \brief GPU storage, host transfers, batched uploads, asynchronous readback, and pushed uniforms.
///
/// This module covers everything that moves bytes between the CPU and the GPU, and the GPU storage itself:
/// <tt>\ref fcg::Buffer "Buffer"</tt> owns a fixed-size GPU allocation for vertices, indices, indirect arguments, or
/// graphics/compute storage; <tt>\ref fcg::TransferBuffer "TransferBuffer"</tt> is host-visible memory for transfers;
/// <tt>\ref fcg::UploadBatch "UploadBatch"</tt> collects several uploads and submits them together;
/// <tt>\ref fcg::BufferReadback "BufferReadback"</tt> reads GPU contents back to the CPU; and
/// <tt>\ref fcg::pushUniforms "pushUniforms"</tt> pushes small uniform blocks directly into a command buffer.
///
/// \section fcg_buffers_usage Getting started
/// Include the module's header:
/// \snippet buffer_examples.cpp includes
/// The typical way to get data onto the GPU is to create a buffer with initial contents and bind it in a render pass:
/// \snippet buffer_examples.cpp geometry
/// All sizes and offsets are in bytes, even in the typed overloads. Data is copied as-is; making it readable in a
/// shader is the caller's responsibility (see \ref fcg_buffers_workflows_layout).
///
/// \section fcg_buffers_workflows Common tasks
/// \subsection fcg_buffers_workflows_uploads Uploading data
/// For occasional data, create the buffer with initial contents or call
/// <tt>\ref fcg::Buffer::upload "Buffer::upload"</tt>: both copy the CPU bytes once and return without waiting for the
/// GPU. Once the call returns, the input CPU data can be released.
/// For repeated or multi-buffer updates, keep an <tt>\ref fcg::UploadBatch "UploadBatch"</tt> outside the frame loop:
/// its <tt>\ref fcg::UploadBatch::upload "upload"</tt> copies CPU data into the batch, and
/// <tt>\ref fcg::UploadBatch::submit "submit"</tt> records and submits all queued uploads in one go. Use
/// <tt>\ref fcg::UploadBatch::record "record"</tt> instead to record the uploads into a copy pass that you manage
/// yourself.
/// \snippet buffer_examples.cpp batch
///
/// \subsection fcg_buffers_workflows_partial Partial updates and cycling
/// The \c cycle flag of an upload decides whether existing buffer contents may be discarded. Leave it \c false for a
/// partial update that must preserve the other bytes, and set it \c true when the upload replaces the whole content;
/// for several uploads that together build one replacement, cycle only the first one.
/// \snippet buffer_examples.cpp partial
///
/// \subsection fcg_buffers_workflows_procedural Writing data directly
/// For data that is produced on the CPU anyway, map a <tt>\ref fcg::TransferBuffer "TransferBuffer"</tt> and write
/// into it directly, then upload it:
/// \snippet buffer_examples.cpp mapped
///
/// \subsection fcg_buffers_workflows_bindings Binding buffers
/// Vertex and index buffers are bound in a render pass by slot. A buffer becomes per-instance data when the pipeline's
/// vertex input description uses \c SDL_GPU_VERTEXINPUTRATE_INSTANCE for its slot; the index element size is supplied
/// when binding, not when allocating the buffer:
/// \snippet buffer_examples.cpp instances
/// Storage bindings address whole buffers; there is no subrange binding.
///
/// \subsection fcg_buffers_workflows_layout Uniforms, storage, and shader layout
/// Uniform blocks are pushed per draw call with <tt>\ref fcg::pushUniforms "pushUniforms"</tt>:
/// \snippet buffer_examples.cpp uniforms
/// Uniform data must follow \c std140 and storage data \c std430. In particular, align \c vec3 and \c vec4 fields to
/// 16 bytes and account for matrix column and array strides; trivially copyable does not mean shader-compatible.
/// \note A uniform push is limited to 4096 bytes; use a storage buffer for larger blocks.
///
/// Graphics shaders that read storage buffers must declare them at pipeline creation; see
/// <tt>\ref fcg::Device::createShader "Device::createShader"</tt> for the binding rules.
/// \snippet buffer_examples.cpp graphics_storage
///
/// In compute passes, read-only buffers are bound inside the pass, while writable buffers are provided when beginning
/// it. A writable buffer can also be read by its shader. End a compute pass before a dispatch that depends on its
/// results: reads and writes within one compute pass are not implicitly synchronized.
/// \snippet buffer_examples.cpp compute
///
/// \subsection fcg_buffers_workflows_indirect Indirect commands
/// Draw and dispatch argument buffers use SDL's argument structs and the \c SDL_GPU_BUFFERUSAGE_INDIRECT usage. Add
/// \c SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE if a compute shader generates the arguments. The indirect draw and
/// dispatch functions assume that the appropriate pipeline is already bound.
/// \snippet buffer_examples.cpp indirect
///
/// \subsection fcg_buffers_workflows_copy Copying on the GPU
/// <tt>\ref fcg::Buffer::copyTo "Buffer::copyTo"</tt> copies between buffers entirely on the GPU, without CPU
/// staging. Both ranges must fit, and overlapping self-copies are rejected.
/// \snippet buffer_examples.cpp copy
///
/// \section fcg_buffers_lifetime Ownership and lifetime
/// The device must outlive every resource created from it. Resources are move-only: moving transfers ownership and
/// leaves the source empty, and operations on an empty source fail with
/// <tt>\ref fcg::BufferErrorCode::InvalidState "InvalidState"</tt>. There are no default constructors; create resources
/// with the \c create factories and keep them directly, or in \c std::optional when creation is delayed or release may
/// happen earlier:
/// \snippet buffer_examples.cpp optional_ownership
/// Destroying a resource asks SDL to release its storage as soon as the GPU no longer uses it, and never waits for GPU
/// idle. Do not move or destroy a transfer buffer while one of its mappings still exists. None of these objects
/// synchronizes internally; use them from the thread that acquired the command buffers involved.
///
/// \section fcg_buffers_errors Handling errors
/// Factories return <tt>std::expected&lt;T, \ref fcg::BufferError "BufferError"&gt;</tt> and operations return
/// <tt>std::expected&lt;void, \ref fcg::BufferError "BufferError"&gt;</tt>. Inspect the error's
/// <tt>\ref fcg::BufferError::code "code"</tt> and <tt>\ref fcg::BufferError::message "message"</tt>; SDL failures are
/// captured with their diagnostics. The facilities do not log, so you decide how to report or recover. Standard
/// allocation exceptions can still propagate from C++ containers. Empty uploads are no-ops on valid buffers;
/// zero-size allocations and downloads are errors.
///
/// \section fcg_buffers_examples More examples
/// Besides the transfer paths above, GPU contents can be read back to the CPU. The standalone form allocates a
/// download buffer, submits its own copy, and the result is then polled, awaited, and mapped:
/// \snippet buffer_examples.cpp readback
/// To download as part of an existing render or compute submission, call
/// <tt>\ref fcg::Buffer::downloadTo "Buffer::downloadTo"</tt> in a copy pass with a download
/// <tt>\ref fcg::TransferBuffer "TransferBuffer"</tt>, then end the pass, submit the command buffer, and wait for its
/// fence before mapping the transfer buffer.
///
/// \see \ref fcg_textures, \ref fcg_devices



//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

// SDL3 library (usage enums and binding descriptors in the public API)
#include <SDL3/SDL_gpu.h>

// Local includes
#include "FCG/export.h"
#include "FCG/device.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPUBuffer;
struct SDL_GPUTransferBuffer;
struct SDL_GPUCommandBuffer;
struct SDL_GPUCopyPass;
struct SDL_GPURenderPass;
struct SDL_GPUComputePass;
struct SDL_GPUFence;

// Framework types
namespace fcg {
	class Buffer;
	class BufferReadback;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_buffers
/// @{



//////
//
// Structs and enums
//

/// Categories of errors reported by buffer operations.
enum class BufferErrorCode
{
	/// An argument is invalid: a size, offset or range is out of bounds, a usage flag, slot or stage is unsuitable, a
	/// pass or command buffer is null, or the resources involved belong to different devices.
	InvalidArgument,

	/// The object is moved-from, or its current state does not permit the operation, e.g. a transfer buffer that is
	/// already mapped.
	InvalidState,

	/// A readback was mapped before the GPU completed it.
	NotReady,

	/// An SDL call failed to create, map, submit or wait.
	SDLFailure
};

/// Error returned by buffer operations: a category plus a human-readable message (including SDL's message where
/// relevant).
struct BufferError
{
	/// The error category.
	BufferErrorCode code;

	/// A human-readable description of the error, including SDL's error message for SDL failures.
	std::string message;
};



//////
//
// Classes
//

/// \brief
/// 	Host-visible staging memory for uploading data to GPU buffers or downloading data from them.
///
/// For an upload, create a transfer buffer with \c SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, write into it through
/// \c map, end the mapping, then record the copy with \c Buffer::uploadFrom in a copy pass. For a download, create it
/// with \c SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, record \c Buffer::downloadTo, submit the command buffer with a fence,
/// wait for the fence, then map the transfer buffer and read the data. \c Buffer::upload, \c UploadBatch and
/// \c Buffer::readback manage the staging memory for you in common cases.
///
/// Only one mapping can be active at a time. A transfer buffer must not be moved or destroyed while it is mapped
/// (checked by an assertion in debug builds). The device must outlive the transfer buffer. Destruction never waits for
/// the GPU. All access must be serialized externally.
///
/// A moved-from transfer buffer is empty: \c handle returns \c nullptr and \c size returns zero. Only assignment and
/// destruction are valid; \c map returns \c InvalidState.
class FCG_FRAMEWORK_EXPORT TransferBuffer
{
public:

	////
	// Types

	// Forward declaration of the mapping guard class (defined outside for readability).
	class Mapping;


	////
	// Object construction/destruction

	/// \brief
	/// 	Create an unmapped transfer buffer.
	///
	/// \param device
	/// 	The device to create the transfer buffer on. Must outlive the transfer buffer.
	/// \param size
	/// 	The size in bytes; between 1 and \c UINT32_MAX.
	/// \param usage
	/// 	The transfer direction: \c SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD or \c SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD.
	/// \param properties
	/// 	SDL properties for the creation, only read during the call; 0 for none.
	///
	/// \return
	/// 	The transfer buffer, or an error: \c InvalidArgument for an invalid size or usage, \c SDLFailure if SDL
	/// 	fails to allocate it.
	[[nodiscard]] static auto create (
		Device &device, std::size_t size, SDL_GPUTransferBufferUsage usage, SDL_PropertiesID properties=0
	) -> std::expected<TransferBuffer, BufferError>;

	/// \brief
	/// 	Release the transfer buffer without waiting for the GPU.
	///
	/// \pre
	/// 	The transfer buffer is not mapped.
	~TransferBuffer ();

	/// Not copyable.
	TransferBuffer (const TransferBuffer&) = delete;

	/// Not copy-assignable.
	auto operator= (const TransferBuffer&) -> TransferBuffer& = delete;

	/// \brief
	/// 	Take over the memory of another transfer buffer.
	///
	/// \pre
	/// 	\c other is not mapped.
	///
	/// \param other
	/// 	The transfer buffer to move from; it is left empty.
	TransferBuffer (TransferBuffer &&other) noexcept;

	/// \brief
	/// 	Release this transfer buffer's memory and take over the memory of another one.
	///
	/// Self-assignment has no effect.
	///
	/// \pre
	/// 	Neither transfer buffer is mapped.
	///
	/// \param other
	/// 	The transfer buffer to move from; it is left empty.
	///
	/// \return
	/// 	A reference to this transfer buffer.
	auto operator= (TransferBuffer &&other) noexcept -> TransferBuffer&;


	////
	// Accessors

	/// \brief
	/// 	Access the underlying SDL transfer buffer.
	///
	/// \return
	/// 	The SDL handle, or \c nullptr if moved-from. The transfer buffer keeps ownership; never release the handle
	/// 	yourself.
	[[nodiscard]] inline auto handle () const -> SDL_GPUTransferBuffer* {
		return m_handle;
	}

	/// The size in bytes, or zero if moved-from.
	[[nodiscard]] inline auto size () const -> std::size_t {
		return m_size;
	}

	/// The transfer direction chosen at creation. Only meaningful if the transfer buffer is not moved-from.
	[[nodiscard]] inline auto usage () const -> SDL_GPUTransferBufferUsage {
		return m_usage;
	}

	/// Whether a mapping of this transfer buffer is currently active.
	[[nodiscard]] auto mapped () const -> bool {
		return m_mapped;
	}

	/// The device the transfer buffer was created on, or \c nullptr if moved-from.
	[[nodiscard]] auto device () const -> Device* {
		return m_device;
	}


	////
	// Methods

	/// \brief
	/// 	Map the whole transfer buffer into CPU memory for reading or writing.
	///
	/// The mapping stays active until the returned \c Mapping is destroyed or unmapped. Never waits for the GPU.
	///
	/// \pre
	/// 	For a download transfer buffer, the GPU has completed the download into it, e.g. its fence has signaled.
	/// 	For an upload transfer buffer mapped without \c cycle, no submitted GPU command still reads the bytes you
	/// 	are going to overwrite. The transfer buffer is neither moved nor destroyed until the mapping ends.
	///
	/// \param cycle
	/// 	Only allowed for upload transfer buffers. If \c true and the transfer buffer is still in use by submitted
	/// 	GPU work, SDL provides fresh memory, so earlier commands keep reading their data; the mapped contents are
	/// 	then undefined. Pass \c true when refilling an upload transfer buffer whose earlier contents may still be in
	/// 	use.
	///
	/// \return
	/// 	The mapping, or an error: \c InvalidState if the transfer buffer is moved-from or already mapped,
	/// 	\c InvalidArgument if \c cycle is \c true for a download transfer buffer, \c SDLFailure if SDL fails to map
	/// 	it. On failure, the mapping state of the transfer buffer is unchanged.
	[[nodiscard]] auto map (bool cycle=false) -> std::expected<Mapping, BufferError>;


private:

	////
	// Object construction/destruction

	/// Construct adopting a successful allocation. For internal use only, use \c create to obtain an instance.
	///
	/// \param device
	/// 	Borrowed creating device; must outlive the allocation.
	/// \param handle
	/// 	Non-null SDL allocation whose ownership is transferred here.
	/// \param size
	/// 	Validated nonzero logical byte count.
	/// \param usage
	/// 	Validated transfer direction.
	TransferBuffer (
		Device &device, SDL_GPUTransferBuffer *handle, std::size_t size, SDL_GPUTransferBufferUsage usage
	) noexcept
		: m_device(&device), m_handle(handle), m_size(size), m_usage(usage)
	{}


	////
	// Fields

	/// Borrow of the device that was used to create the buffer.
	Device *m_device = nullptr;

	/// The SDL staging allocation that we manage.
	SDL_GPUTransferBuffer *m_handle = nullptr;

	/// Logical size of the buffer in bytes.
	std::size_t m_size = 0;

	/// The transfer direction, fixed at creation.
	SDL_GPUTransferBufferUsage m_usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

	/// Whether a mapping is currently active.
	bool m_mapped = false;
};

/// \brief
/// 	Scoped CPU access to the memory of a mapped \c TransferBuffer.
///
/// Obtained from \c TransferBuffer::map or \c BufferReadback::map. The memory is accessible through \c data until the
/// mapping is destroyed or \c unmap is called; then all pointers and spans into it become invalid. The memory belongs
/// to SDL; never free it. Moving a mapping transfers it to the new object, and spans obtained before the move stay
/// valid until the new object ends the mapping. The mapped transfer buffer or readback must stay alive and must not be
/// moved while the mapping is active. Writing to the mapping of a download only changes the CPU copy, not the GPU
/// buffer. Ending a mapping never waits for the GPU.
///
/// A moved-from mapping is empty: \c data returns an empty span.
class FCG_FRAMEWORK_EXPORT TransferBuffer::Mapping
{
	////
	// Friend declarations

	/// Creates mappings in \c TransferBuffer::map.
	friend class TransferBuffer;


public:

	////
	// Object construction/destruction

	/// End the mapping if it is still active. All pointers and spans into it become invalid.
	~Mapping ();

	/// Not copyable.
	Mapping (const Mapping&) = delete;

	/// Not copy-assignable.
	auto operator= (const Mapping&) -> Mapping& = delete;

	/// \brief
	/// 	Take over the active mapping of another guard.
	///
	/// \param other
	/// 	The mapping guard to move from; it is left empty.
	Mapping (Mapping &&other) noexcept;

	/// \brief
	/// 	End this guard's mapping if active, then take over the mapping of another guard.
	///
	/// Self-assignment has no effect.
	///
	/// \param other
	/// 	The mapping guard to move from; it is left empty.
	///
	/// \return
	/// 	A reference to this mapping guard.
	auto operator= (Mapping &&other) noexcept -> Mapping&;


	////
	// Accessors

	/// \brief
	/// 	Access the mapped bytes for reading and writing.
	///
	/// \return
	/// 	A span over the whole mapped memory, valid until the mapping ends, or an empty span if the mapping has
	/// 	ended or was moved from.
	[[nodiscard]] auto data () -> std::span<std::byte> {
		return m_data;
	}

	/// \brief
	/// 	Access the mapped bytes for reading.
	///
	/// \return
	/// 	A span over the whole mapped memory, valid until the mapping ends, or an empty span if the mapping has
	/// 	ended or was moved from.
	[[nodiscard]] auto data () const -> std::span<const std::byte> {
		return m_data;
	}


	////
	// Methods

	/// End the mapping early. All pointers and spans into it become invalid. Calling it again has no effect.
	void unmap ();


private:

	////
	// Object construction/destruction

	/// Construct adopting a successful mapping. For internal use only, use \c TransferBuffer::map to obtain an
	/// instance.
	///
	/// \param owner
	/// 	The transfer buffer that has been mapped.
	/// \param data
	/// 	The driver-owned bytes of the mapping.
	Mapping (TransferBuffer &owner, std::span<std::byte> data) : owner(&owner), m_data(data) {}


	////
	// Fields

	/// The mapped transfer buffer, or null once the mapping has ended. Lets the guard unmap the transfer buffer and
	/// mark it as available again.
	TransferBuffer *owner = nullptr;

	/// Driver-owned bytes, invalid after unmapping.
	std::span<std::byte> m_data;
};

/// \brief
/// 	A pending download of buffer contents to CPU memory, created by \c Buffer::readback.
///
/// Keep the readback after calling \c Buffer::readback, check \c ready in later frames (or call \c wait to block),
/// then \c map it and copy the bytes into your own objects, e.g. with \c std::memcpy. The mapped bytes are a copy of
/// the buffer range at the time the GPU executed the download.
///
/// The source buffer is not needed after \c Buffer::readback returns. A readback can be destroyed at any time, even
/// while the GPU is still working on it; destruction never waits for the GPU. Only one mapping can be active at a time,
/// and the readback must not be moved or destroyed while it is mapped. The device must outlive the readback. All
/// access, including \c ready and \c wait, must be serialized externally.
///
/// A moved-from readback is empty: \c ready returns \c false, and \c wait and \c map return \c InvalidState.
class FCG_FRAMEWORK_EXPORT BufferReadback
{
	////
	// Friend declarations

	/// Creates readbacks in \c Buffer::readback.
	friend class Buffer;


public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Destroy the readback without waiting for the GPU.
	///
	/// \pre
	/// 	The readback is not mapped.
	~BufferReadback ();

	/// Not copyable.
	BufferReadback (const BufferReadback&) = delete;

	/// Not copy-assignable.
	auto operator= (const BufferReadback&) -> BufferReadback& = delete;

	/// \brief
	/// 	Take over another readback, whether pending or completed.
	///
	/// \pre
	/// 	\c other is not mapped.
	///
	/// \param other
	/// 	The readback to move from; it is left empty.
	BufferReadback (BufferReadback &&other) noexcept;

	/// \brief
	/// 	Discard this readback without waiting for the GPU and take over another one.
	///
	/// Self-assignment has no effect.
	///
	/// \pre
	/// 	Neither readback is mapped.
	///
	/// \param other
	/// 	The readback to move from; it is left empty.
	///
	/// \return
	/// 	A reference to this readback.
	auto operator= (BufferReadback &&other) noexcept -> BufferReadback&;


	////
	// Methods

	/// \brief
	/// 	Check whether the GPU has completed the download. Never blocks.
	///
	/// \return
	/// 	\c true once the download has completed, \c false while it is pending or if the readback is moved-from.
	[[nodiscard]] auto ready () const -> bool;

	/// \brief
	/// 	Block until the GPU has completed this download.
	///
	/// Only waits for this readback, not for other GPU work. Returns immediately if the download has already
	/// completed.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if the readback is moved-from, \c SDLFailure if waiting
	/// 	fails.
	[[nodiscard]] auto wait () const -> std::expected<void, BufferError>;

	/// \brief
	/// 	Map the downloaded bytes for reading. Never blocks.
	///
	/// \pre
	/// 	The readback is neither moved nor destroyed until the mapping ends.
	///
	/// \return
	/// 	A mapping containing exactly the requested range of the buffer, or an error: \c NotReady if the GPU has not
	/// 	completed the download, \c InvalidState if the readback is moved-from or already mapped, \c SDLFailure if
	/// 	SDL fails to map the data.
	[[nodiscard]] auto map () -> std::expected<TransferBuffer::Mapping, BufferError>;


private:

	////
	// Object construction/destruction

	/// Construct adopting the given download storage and the fence acquired when submitting the download.
	///
	/// \param storage
	/// 	The download transfer buffer that receives the data.
	/// \param fence
	/// 	Preallocated node holding the successfully acquired completion fence.
	BufferReadback (TransferBuffer &&storage, std::unique_ptr<Device::RetiredFence> fence);


	////
	// Fields

	/// The download allocation, kept alive while the fence is pending.
	TransferBuffer m_storage;

	/// Preallocated fence ownership; handed to the device for retirement if the readback is abandoned.
	std::unique_ptr<Device::RetiredFence> m_fence;
};

/// \brief
/// 	A fixed-size block of GPU memory, e.g. for vertices, indices, indirect draw arguments or shader storage.
///
/// Create a buffer with \c create, choosing the SDL usage flags for everything you want to use it for; size and usage
/// cannot be changed later. To grow a buffer, create a larger one and copy the old contents with \c copyTo. Fill the
/// buffer with \c upload for occasional updates, or use an \c UploadBatch for frequent ones. Bind it in render or
/// compute passes with \c bindVertex, \c bindIndex and \c bindStorage, and read its contents back with \c readback.
/// All offsets and sizes are in bytes.
///
/// The device must outlive the buffer. Destruction never waits for the GPU. All access must be serialized externally.
///
/// A moved-from buffer is empty: \c handle returns \c nullptr, and \c size and \c usage return zero. Only assignment
/// and destruction are valid; other operations return \c InvalidState.
class FCG_FRAMEWORK_EXPORT Buffer
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Create a buffer with uninitialized contents.
	///
	/// \param device
	/// 	The device to create the buffer on. Must outlive the buffer.
	/// \param size
	/// 	The size in bytes; between 1 and \c UINT32_MAX.
	/// \param usage
	/// 	The SDL buffer usage flags; must be non-zero. \c SDL_GPU_BUFFERUSAGE_VERTEX and
	/// 	\c SDL_GPU_BUFFERUSAGE_INDEX cannot be combined.
	/// \param properties
	/// 	SDL properties for the creation, only read during the call; 0 for none.
	///
	/// \return
	/// 	The buffer, or an error: \c InvalidArgument for an invalid size or usage, \c SDLFailure if SDL fails to
	/// 	allocate it.
	[[nodiscard]] static auto create (
		Device &device, std::size_t size, SDL_GPUBufferUsageFlags usage, SDL_PropertiesID properties=0
	) -> std::expected<Buffer, BufferError>;

	/// \brief
	/// 	Create a buffer of the size of \c data and upload \c data into it.
	///
	/// \anchor buffers_create_initial_data
	/// The data is copied before the function returns. The upload is submitted on its own command buffer; the
	/// function does not wait for it to complete.
	///
	/// \param device
	/// 	The device to create the buffer on. Must outlive the buffer.
	/// \param data
	/// 	The initial contents; between 1 and \c UINT32_MAX bytes. Only read during the call.
	/// \param usage
	/// 	The SDL buffer usage flags; must be non-zero. \c SDL_GPU_BUFFERUSAGE_VERTEX and
	/// 	\c SDL_GPU_BUFFERUSAGE_INDEX cannot be combined.
	/// \param properties
	/// 	SDL properties for the creation, only read during the call; 0 for none.
	///
	/// \return
	/// 	The buffer, or an error: \c InvalidArgument for empty or too large data or an invalid usage, \c SDLFailure
	/// 	if SDL fails to allocate the buffer or to stage and submit the upload. On failure, no resources remain
	/// 	allocated.
	[[nodiscard]] static auto create (
		Device &device, std::span<const std::byte> data, SDL_GPUBufferUsageFlags usage, SDL_PropertiesID properties=0
	) -> std::expected<Buffer, BufferError>;

	/// \brief
	/// 	Create a buffer of the size of \c data and upload the elements of \c data into it.
	///
	/// Equivalent to the byte overload called with <tt>std::as_bytes(data)</tt>. The elements are copied bitwise,
	/// without any layout conversion.
	///
	/// \tparam T
	/// 	The element type; must be trivially copyable, and may be \c const.
	/// \tparam Extent
	/// 	The static or dynamic extent of the span.
	/// \param device
	/// 	The device to create the buffer on. Must outlive the buffer.
	/// \param data
	/// 	The initial contents; must not be empty. Only read during the call.
	/// \param usage
	/// 	The SDL buffer usage flags, as for the byte overload.
	/// \param properties
	/// 	SDL properties for the creation, only read during the call; 0 for none.
	///
	/// \return
	/// 	The buffer of <tt>data.size_bytes()</tt> bytes, or the same errors as the byte overload.
	template <class T, std::size_t Extent> requires std::is_trivially_copyable_v<T>
	[[nodiscard]] static auto create (
		Device &device, std::span<T, Extent> data, SDL_GPUBufferUsageFlags usage, SDL_PropertiesID properties=0
	) -> std::expected<Buffer, BufferError> {
		return create(device, std::span<const std::byte>(std::as_bytes(data)), usage, properties);
	}

	/// \brief
	/// 	Release the buffer without waiting for the GPU.
	///
	/// GPU work that was already submitted and uses the buffer is unaffected.
	///
	/// \pre
	/// 	No \c UploadBatch holds queued writes to this buffer that have not been recorded yet.
	~Buffer ();

	/// Not copyable.
	Buffer (const Buffer&) = delete;

	/// Not copy-assignable.
	auto operator= (const Buffer&) -> Buffer& = delete;

	/// \brief
	/// 	Take over the GPU memory of another buffer.
	///
	/// \param other
	/// 	The buffer to move from; it is left empty.
	Buffer (Buffer &&other) noexcept;

	/// \brief
	/// 	Release this buffer's GPU memory without waiting for the GPU and take over the memory of another buffer.
	///
	/// Self-assignment has no effect.
	///
	/// \param other
	/// 	The buffer to move from; it is left empty.
	///
	/// \return
	/// 	A reference to this buffer.
	auto operator= (Buffer &&other) noexcept -> Buffer&;


	////
	// Accessors

	/// \brief
	/// 	Access the underlying SDL buffer, e.g. for indirect draw calls or other direct use of the SDL GPU API.
	///
	/// \return
	/// 	The SDL handle, or \c nullptr if moved-from. The buffer keeps ownership; never release the handle yourself.
	[[nodiscard]] auto handle () const -> SDL_GPUBuffer* { return m_handle; }

	/// The size in bytes (not in elements), or zero if moved-from.
	[[nodiscard]] auto size () const -> std::size_t { return m_size; }

	/// The SDL usage flags chosen at creation, or zero if moved-from.
	[[nodiscard]] auto usage () const -> SDL_GPUBufferUsageFlags { return m_usage; }

	/// The device the buffer was created on, or \c nullptr if moved-from.
	[[nodiscard]] auto device () const -> Device* { return m_device; }


	////
	// Transfers

	/// \brief
	/// 	Copy CPU data into the buffer.
	///
	/// The data is copied into temporary staging memory, and the upload is submitted on its own command buffer before
	/// the function returns. The function does not wait for the upload to complete. For frequent updates, prefer an
	/// \c UploadBatch.
	///
	/// \param data
	/// 	The bytes to write, only read during the call. An empty span does nothing.
	/// \param offset
	/// 	The byte offset in the buffer at which to write. The written range must lie within the buffer.
	/// \param cycle
	/// 	If \c true and the buffer is still in use by submitted GPU work, SDL may give the buffer fresh memory;
	/// 	the bytes outside the written range are then undefined. Pass \c false to preserve the rest of the buffer.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if the buffer is moved-from, \c InvalidArgument if the
	/// 	range does not fit, \c SDLFailure if staging or submitting the upload fails.
	[[nodiscard]] auto upload (std::span<const std::byte> data, std::size_t offset=0, bool cycle=false)
		-> std::expected<void, BufferError>;

	/// \brief
	/// 	Copy the elements of \c data into the buffer.
	///
	/// Equivalent to the byte overload called with <tt>std::as_bytes(data)</tt>. The elements are copied bitwise,
	/// without any layout conversion.
	///
	/// \tparam T
	/// 	The element type; must be trivially copyable, and may be \c const.
	/// \tparam Extent
	/// 	The static or dynamic extent of the span.
	/// \param data
	/// 	The elements to write, only read during the call. An empty span does nothing.
	/// \param offset
	/// 	The byte offset (not element index) in the buffer at which to write.
	/// \param cycle
	/// 	Whether SDL may give the buffer fresh memory, as for the byte overload.
	///
	/// \return
	/// 	The same result as the byte overload.
	template <class T, std::size_t Extent> requires std::is_trivially_copyable_v<T>
	[[nodiscard]] auto upload (std::span<T, Extent> data, std::size_t offset=0, bool cycle=false)
		-> std::expected<void, BufferError> {
		return upload(std::span<const std::byte>(std::as_bytes(data)), offset, cycle);
	}

	/// \brief
	/// 	Record a copy from an upload transfer buffer into this buffer.
	///
	/// The copy executes when the command buffer of \c pass is submitted. Until it has completed on the GPU, refill
	/// \c source only by mapping it with cycling.
	///
	/// \pre
	/// 	\c pass is an active copy pass of a command buffer on this buffer's device, used on the thread that acquired
	/// 	the command buffer. The bytes to copy have been written to \c source.
	///
	/// \param pass
	/// 	The copy pass to record into.
	/// \param source
	/// 	An unmapped upload transfer buffer on the same device.
	/// \param size
	/// 	The number of bytes to copy; zero records nothing.
	/// \param sourceOffset
	/// 	The byte offset in \c source to copy from.
	/// \param destinationOffset
	/// 	The byte offset in this buffer to copy to.
	/// \param cycle
	/// 	If \c true and the buffer is still in use by submitted GPU work, SDL may give the buffer fresh memory;
	/// 	the bytes outside the written range are then undefined. Pass \c false to preserve the rest of the buffer.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if this buffer or \c source is moved-from or \c source is
	/// 	mapped, \c InvalidArgument if \c pass is null, \c source is on another device or is not an upload transfer
	/// 	buffer, or either range does not fit.
	[[nodiscard]] auto uploadFrom (
		SDL_GPUCopyPass *pass, const TransferBuffer &source, std::size_t size, std::size_t sourceOffset=0,
		std::size_t destinationOffset=0, bool cycle=false
	) -> std::expected<void, BufferError>;

	/// \brief
	/// 	Record a copy from this buffer into a download transfer buffer.
	///
	/// To read the data, end the copy pass, submit its command buffer with
	/// \c SDL_SubmitGPUCommandBufferAndAcquireFence, wait for or poll the fence, then map \c destination without
	/// cycling. \c readback does all of this for you.
	///
	/// \pre
	/// 	\c pass is an active copy pass of a command buffer on this buffer's device, used on the thread that acquired
	/// 	the command buffer. No other pending download writes to the same range of \c destination.
	///
	/// \param pass
	/// 	The copy pass to record into.
	/// \param destination
	/// 	An unmapped download transfer buffer on the same device.
	/// \param size
	/// 	The number of bytes to copy; must not be zero.
	/// \param sourceOffset
	/// 	The byte offset in this buffer to copy from.
	/// \param destinationOffset
	/// 	The byte offset in \c destination to copy to.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if this buffer or \c destination is moved-from or
	/// 	\c destination is mapped, \c InvalidArgument if \c pass is null, \c size is zero, \c destination is on
	/// 	another device or is not a download transfer buffer, or either range does not fit.
	[[nodiscard]] auto downloadTo (
		SDL_GPUCopyPass *pass, TransferBuffer &destination, std::size_t size, std::size_t sourceOffset=0,
		std::size_t destinationOffset=0
	) const -> std::expected<void, BufferError>;

	/// \brief
	/// 	Record a copy from this buffer into another buffer, entirely on the GPU.
	///
	/// GPU commands executed after the copy see the copied bytes.
	///
	/// \pre
	/// 	\c pass is an active copy pass of a command buffer on the buffers' device, used on the thread that acquired
	/// 	the command buffer.
	///
	/// \param pass
	/// 	The copy pass to record into.
	/// \param destination
	/// 	The buffer to copy into, on the same device. May be this buffer if the source and destination ranges do not
	/// 	overlap and \c cycle is \c false.
	/// \param size
	/// 	The number of bytes to copy; zero records nothing.
	/// \param sourceOffset
	/// 	The byte offset in this buffer to copy from.
	/// \param destinationOffset
	/// 	The byte offset in \c destination to copy to.
	/// \param cycle
	/// 	If \c true and \c destination is still in use by submitted GPU work, SDL may give it fresh memory; the bytes
	/// 	outside the written range are then undefined. Pass \c false to preserve the rest of \c destination.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if either buffer is moved-from, \c InvalidArgument if
	/// 	\c pass is null, the buffers are on different devices, either range does not fit, or a copy within the same
	/// 	buffer has overlapping ranges or uses cycling.
	[[nodiscard]] auto copyTo (
		SDL_GPUCopyPass *pass, Buffer &destination, std::size_t size, std::size_t sourceOffset=0,
		std::size_t destinationOffset=0, bool cycle=false
	) const -> std::expected<void, BufferError>;

	/// \brief
	/// 	Start downloading a range of this buffer to CPU memory.
	///
	/// The download is submitted on its own command buffer, and the function returns immediately with a
	/// \c BufferReadback that can be polled, waited for and mapped. The downloaded data reflects all GPU work
	/// submitted before this call, but not commands in command buffers that have not been submitted yet.
	///
	/// \param offset
	/// 	The byte offset of the range to download.
	/// \param size
	/// 	The number of bytes to download; \c std::dynamic_extent downloads everything from \c offset to the end of
	/// 	the buffer.
	///
	/// \return
	/// 	The readback, or an error: \c InvalidState if the buffer is moved-from, \c InvalidArgument if the range is
	/// 	empty or does not fit, \c SDLFailure if allocating staging memory or submitting the download fails.
	[[nodiscard]] auto readback (std::size_t offset=0, std::size_t size=std::dynamic_extent) const
		-> std::expected<BufferReadback, BufferError>;


	////
	// Bindings

	/// \brief
	/// 	Get an SDL vertex or index buffer binding for this buffer.
	///
	/// Use the result with \c SDL_BindGPUVertexBuffers or \c SDL_BindGPUIndexBuffer, e.g. to bind several buffers in
	/// one call. The binding refers to the buffer without keeping it alive.
	///
	/// \param offset
	/// 	The byte offset of the first vertex or index; must be less than \c size.
	///
	/// \return
	/// 	The binding, or an error: \c InvalidState if the buffer is moved-from, \c InvalidArgument if it has neither
	/// 	\c SDL_GPU_BUFFERUSAGE_VERTEX nor \c SDL_GPU_BUFFERUSAGE_INDEX usage or \c offset is out of range.
	[[nodiscard]] auto binding (std::size_t offset=0) const -> std::expected<SDL_GPUBufferBinding, BufferError>;

	/// \brief
	/// 	Get an SDL binding for this buffer as writable compute storage, for use with \c SDL_BeginGPUComputePass.
	///
	/// The buffer needs \c SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE usage. The compute shader can also read a
	/// writable storage buffer. The binding refers to the buffer without keeping it alive.
	///
	/// \param cycle
	/// 	If \c true and the buffer is still in use by submitted GPU work, SDL may give the buffer fresh memory when
	/// 	the compute pass begins; its previous contents are then undefined.
	///
	/// \return
	/// 	The binding, or an error: \c InvalidState if the buffer is moved-from, \c InvalidArgument if it lacks
	/// 	\c SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE usage.
	[[nodiscard]] auto readWriteBinding (bool cycle=false) const
		-> std::expected<SDL_GPUStorageBufferReadWriteBinding, BufferError>;

	/// \brief
	/// 	Bind this buffer as a vertex buffer, e.g. for per-vertex or per-instance attributes.
	///
	/// The buffer needs \c SDL_GPU_BUFFERUSAGE_VERTEX usage. The attribute layout, stride and input rate come from the
	/// bound pipeline.
	///
	/// \pre
	/// 	\c pass is an active render pass on this buffer's device.
	///
	/// \param pass
	/// 	The render pass to record into.
	/// \param slot
	/// 	The vertex buffer slot, as declared in the pipeline's vertex input state.
	/// \param offset
	/// 	The byte offset of the first element; must be less than \c size.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if the buffer is moved-from, \c InvalidArgument if it lacks
	/// 	\c SDL_GPU_BUFFERUSAGE_VERTEX usage, \c pass is null, or \c offset is out of range.
	[[nodiscard]] auto bindVertex (SDL_GPURenderPass *pass, Uint32 slot=0, std::size_t offset=0) const
		-> std::expected<void, BufferError>;

	/// \brief
	/// 	Bind this buffer as the index buffer for subsequent indexed draws.
	///
	/// The buffer needs \c SDL_GPU_BUFFERUSAGE_INDEX usage.
	///
	/// \pre
	/// 	\c pass is an active render pass on this buffer's device.
	///
	/// \param pass
	/// 	The render pass to record into.
	/// \param elementSize
	/// 	The index size: \c SDL_GPU_INDEXELEMENTSIZE_16BIT or \c SDL_GPU_INDEXELEMENTSIZE_32BIT.
	/// \param offset
	/// 	The byte offset of the first index; must be a multiple of the index size, with at least one complete index
	/// 	between \c offset and the end of the buffer.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if the buffer is moved-from, \c InvalidArgument if it lacks
	/// 	\c SDL_GPU_BUFFERUSAGE_INDEX usage, \c pass is null, \c elementSize is invalid, or \c offset is misaligned
	/// 	or out of range.
	[[nodiscard]] auto bindIndex (
		SDL_GPURenderPass *pass, SDL_GPUIndexElementSize elementSize, std::size_t offset=0
	) const -> std::expected<void, BufferError>;

	/// \brief
	/// 	Bind the whole buffer as read-only storage for the vertex or fragment shader.
	///
	/// The buffer needs \c SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ usage, and the shader must declare the storage
	/// buffer (see <tt>\ref ShaderResources "ShaderResources"</tt>).
	///
	/// \pre
	/// 	\c pass is an active render pass on this buffer's device.
	///
	/// \param pass
	/// 	The render pass to record into.
	/// \param stage
	/// 	The shader stage to bind to; <tt>ShaderStage::VERTEX</tt> or <tt>ShaderStage::FRAGMENT</tt>.
	/// \param slot
	/// 	The index of the storage buffer among the storage buffers declared by the shader stage, starting at 0 and
	/// 	independent of samplers and storage textures.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if the buffer is moved-from, \c InvalidArgument if it lacks
	/// 	\c SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ usage, \c pass is null, or \c stage is
	/// 	<tt>ShaderStage::COMPUTE</tt>.
	[[nodiscard]] auto bindStorage (SDL_GPURenderPass *pass, ShaderStage stage, Uint32 slot=0) const
		-> std::expected<void, BufferError>;

	/// \brief
	/// 	Bind the whole buffer as read-only storage for the compute shader.
	///
	/// The buffer needs \c SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ usage. Writable storage buffers are passed to
	/// \c SDL_BeginGPUComputePass instead; see \c readWriteBinding.
	///
	/// \pre
	/// 	\c pass is an active compute pass on this buffer's device.
	///
	/// \param pass
	/// 	The compute pass to record into.
	/// \param slot
	/// 	The index of the storage buffer among the read-only storage buffers declared by the compute shader,
	/// 	starting at 0.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if the buffer is moved-from, \c InvalidArgument if it lacks
	/// 	\c SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ usage or \c pass is null.
	[[nodiscard]] auto bindStorage (SDL_GPUComputePass *pass, Uint32 slot=0) const -> std::expected<void, BufferError>;


private:

	////
	// Object construction/destruction

	/// Construct adopting a successful allocation. For internal use only, use \c create to obtain an instance.
	///
	/// \param device
	/// 	Borrowed creating device; must outlive the allocation.
	/// \param handle
	/// 	Non-null SDL allocation whose ownership is transferred here.
	/// \param size
	/// 	Validated nonzero logical byte count.
	/// \param usage
	/// 	Validated SDL usage flags.
	Buffer (Device &device, SDL_GPUBuffer *handle, std::size_t size, SDL_GPUBufferUsageFlags usage) noexcept
		: m_device(&device), m_handle(handle), m_size(size), m_usage(usage)
	{}


	////
	// Fields

	/// Borrowed creating device.
	Device *m_device = nullptr;

	/// Owned SDL allocation.
	SDL_GPUBuffer *m_handle = nullptr;

	/// Logical byte size; the SDL allocation is at least four bytes even if this is smaller.
	std::size_t m_size = 0;

	/// Fixed SDL usage mask.
	SDL_GPUBufferUsageFlags m_usage = 0;
};

/// \brief
/// 	Collects writes to one or more buffers and records them together, reusing its staging memory.
///
/// Keep one batch for as long as you need to update buffers, e.g. as a member of your applet. Whenever data changes,
/// queue writes with \c upload, which copies the data immediately, so the input can be discarded right away. Then
/// either call \c record in a copy pass of a command buffer you submit yourself, or call \c submit to record and
/// submit the writes on a new command buffer. Afterwards, the batch is empty and can be reused immediately, even while
/// earlier submissions are still executing on the GPU. Its staging memory is kept for reuse.
///
/// Queued writes execute in the order they were queued. Destination buffers are not owned by the batch and must stay
/// alive until their writes have been recorded. The device must outlive the batch. Destroying or clearing a batch
/// discards queued writes without submitting them. All access must be serialized externally.
///
/// A moved-from batch is empty; \c upload, \c record and \c submit return \c InvalidState.
class FCG_FRAMEWORK_EXPORT UploadBatch
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Construct an empty batch for the given device. Allocates no GPU memory.
	///
	/// \param device
	/// 	The device of all destination buffers. Must outlive the batch.
	explicit UploadBatch (Device &device);

	/// Destroy the batch, discarding queued writes without submitting them. Never waits for the GPU.
	~UploadBatch ();

	/// Not copyable.
	UploadBatch (const UploadBatch&) = delete;

	/// Not copy-assignable.
	auto operator= (const UploadBatch&) -> UploadBatch& = delete;

	/// \brief
	/// 	Take over the queued writes and staging memory of another batch.
	///
	/// \param other
	/// 	The batch to move from; it is left empty.
	UploadBatch (UploadBatch &&other) noexcept;

	/// \brief
	/// 	Discard this batch's queued writes and staging memory, then take over those of another batch.
	///
	/// Never submits and never waits for the GPU. Self-assignment has no effect.
	///
	/// \param other
	/// 	The batch to move from; it is left empty.
	///
	/// \return
	/// 	A reference to this batch.
	auto operator= (UploadBatch &&other) noexcept -> UploadBatch&;


	////
	// Methods

	/// \brief
	/// 	Queue a write of \c data to \c destination.
	///
	/// The bytes are copied into the batch's staging memory immediately, so \c data may be discarded after the call.
	/// No GPU command is recorded until \c record or \c submit is called.
	///
	/// \param destination
	/// 	The buffer to write to, on the batch's device. Must stay alive until the write has been recorded.
	/// \param data
	/// 	The bytes to write; an empty span queues nothing.
	/// \param offset
	/// 	The byte offset in \c destination at which to write. The written range must lie within the buffer.
	/// \param cycle
	/// 	If \c true and \c destination is still in use by submitted GPU work when the write executes, SDL may give it
	/// 	fresh memory; bytes not written by this or later writes are then undefined. When several writes together
	/// 	replace the whole buffer, pass \c true only for the first one. Pass \c false to preserve other bytes.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if the batch or \c destination is moved-from,
	/// 	\c InvalidArgument if \c destination is on another device or the range does not fit, \c SDLFailure if
	/// 	allocating or mapping staging memory fails. On failure, previously queued writes are kept.
	[[nodiscard]] auto upload (
		Buffer &destination, std::span<const std::byte> data, std::size_t offset=0, bool cycle=false
	) -> std::expected<void, BufferError>;

	/// \brief
	/// 	Queue a write of the elements of \c data to \c destination.
	///
	/// Equivalent to the byte overload called with <tt>std::as_bytes(data)</tt>. The elements are copied bitwise,
	/// without any layout conversion.
	///
	/// \tparam T
	/// 	The element type; must be trivially copyable, and may be \c const.
	/// \tparam Extent
	/// 	The static or dynamic extent of the span.
	/// \param destination
	/// 	The buffer to write to, on the batch's device. Must stay alive until the write has been recorded.
	/// \param data
	/// 	The elements to write; an empty span queues nothing.
	/// \param offset
	/// 	The byte offset (not element index) in \c destination at which to write.
	/// \param cycle
	/// 	Whether SDL may give \c destination fresh memory, as for the byte overload.
	///
	/// \return
	/// 	The same result as the byte overload.
	template <class T, std::size_t Extent> requires std::is_trivially_copyable_v<T>
	[[nodiscard]] auto upload (
		Buffer &destination, std::span<T, Extent> data, std::size_t offset=0, bool cycle=false
	) -> std::expected<void, BufferError> {
		return upload(destination, std::span<const std::byte>(std::as_bytes(data)), offset, cycle);
	}

	/// \brief
	/// 	Record all queued writes, in order, into a copy pass and empty the queue.
	///
	/// The writes execute when the command buffer of \c pass is submitted. Ending the pass and submitting the command
	/// buffer remain the caller's responsibility.
	///
	/// \pre
	/// 	\c pass is an active copy pass of a command buffer on the batch's device, used on the thread that acquired
	/// 	the command buffer.
	///
	/// \param pass
	/// 	The copy pass to record into.
	///
	/// \return
	/// 	Nothing on success, including when the queue is empty, or an error: \c InvalidState if the batch is
	/// 	moved-from, \c InvalidArgument if \c pass is null. On failure, the queue is unchanged.
	[[nodiscard]] auto record (SDL_GPUCopyPass *pass) -> std::expected<void, BufferError>;

	/// \brief
	/// 	Record all queued writes on a new command buffer, submit it and empty the queue.
	///
	/// Returns without waiting for the GPU. If the queue is empty, nothing is submitted.
	///
	/// \return
	/// 	Nothing on success, or an error: \c InvalidState if the batch is moved-from, \c SDLFailure if acquiring the
	/// 	command buffer, beginning the copy pass or submitting fails. If acquiring the command buffer or beginning
	/// 	the copy pass fails, the queue is unchanged. If submission fails, the queued writes are discarded and their
	/// 	destinations may not have been updated.
	[[nodiscard]] auto submit () -> std::expected<void, BufferError>;

	/// Discard all queued writes without recording them, keeping the staging memory for reuse. Never waits for the GPU.
	void clear ();


private:

	////
	// Types

	/// Upload transfer buffer at a stable address, plus its mapping while writes are being staged. For internal use
	/// only.
	struct Page;

	/// A single queued copy operation. For internal use only.
	struct Copy
	{
		/// Source page index.
		std::size_t page;

		/// Source byte offset within that page.
		Uint32 sourceOffset;

		/// Borrowed destination and checked byte range.
		SDL_GPUBufferRegion destination;

		/// Whether this destination write may select fresh backing storage.
		bool cycle;
	};


	////
	// Fields

	/// Borrowed common device; null after move.
	Device *device = nullptr;

	/// Staging pages, held by pointer so that mapped pages never change their address.
	std::vector<std::unique_ptr<Page>> pages;

	/// Queued copies in user order, retaining capacity across submissions.
	std::vector<Copy> copies;
};



//////
//
// Functions
//

/// \brief
/// 	Push a block of uniform data to a uniform slot of one shader stage.
///
/// The data is copied immediately and may be discarded after the call. It is used by all draws or dispatches recorded
/// afterwards on the same command buffer, until another push to the same stage and slot replaces it. Pushing is
/// allowed both inside and outside of render and compute passes. The bytes must follow the \c std140 layout rules and
/// match the uniform block the shader declares at that slot.
///
/// \pre
/// 	\c commandBuffer has not been submitted yet and is used on the thread that acquired it.
///
/// \note
/// 	A uniform block can be at most 4096 bytes. Use a storage buffer for larger data.
///
/// \param commandBuffer
/// 	The command buffer to push to.
/// \param stage
/// 	The shader stage that receives the data.
/// \param slot
/// 	The uniform slot, i.e. the binding number of the uniform block in the shader; 0 to 3.
/// \param data
/// 	The uniform data; 1 to 4096 bytes.
///
/// \return
/// 	Nothing on success, or \c InvalidArgument if \c commandBuffer is null, \c slot is greater than 3, or \c data is
/// 	empty or larger than 4096 bytes.
[[nodiscard]] FCG_FRAMEWORK_EXPORT auto pushUniforms (
	SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, Uint32 slot, std::span<const std::byte> data
) -> std::expected<void, BufferError>;

/// \brief
/// 	Push the elements of \c data as one block of uniform data.
///
/// Equivalent to the byte overload called with <tt>std::as_bytes(data)</tt>: the block consists of the
/// <tt>data.size_bytes()</tt> bytes of the elements, copied bitwise without any layout conversion.
///
/// \pre
/// 	\c commandBuffer has not been submitted yet and is used on the thread that acquired it.
///
/// \note
/// 	A uniform block can be at most 4096 bytes. Use a storage buffer for larger data.
///
/// \tparam T
/// 	The element type; must be trivially copyable, and may be \c const.
/// \tparam Extent
/// 	The static or dynamic extent of the span.
/// \param commandBuffer
/// 	The command buffer to push to.
/// \param stage
/// 	The shader stage that receives the data.
/// \param slot
/// 	The uniform slot, i.e. the binding number of the uniform block in the shader; 0 to 3.
/// \param data
/// 	The elements to push, laid out according to the \c std140 rules.
///
/// \return
/// 	The same result as the byte overload.
template <class T, std::size_t Extent>
	requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline auto pushUniforms (
	SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, Uint32 slot, std::span<T, Extent> data
) -> std::expected<void, BufferError>
{
	return pushUniforms(commandBuffer, stage, slot, std::span<const std::byte>(std::as_bytes(data)));
}

/// \brief
/// 	Push a value as one block of uniform data.
///
/// The block consists of the <tt>sizeof(T)</tt> bytes of \c value, copied bitwise without any layout conversion. The
/// layout of \c T must therefore match the shader's \c std140 uniform block declaration, e.g. \c vec3 and \c vec4
/// members must be aligned to 16 bytes.
///
/// \pre
/// 	\c commandBuffer has not been submitted yet and is used on the thread that acquired it.
///
/// \note
/// 	A uniform block can be at most 4096 bytes. Use a storage buffer for larger data.
///
/// \tparam T
/// 	The uniform block type; must be trivially copyable.
/// \param commandBuffer
/// 	The command buffer to push to.
/// \param stage
/// 	The shader stage that receives the data.
/// \param slot
/// 	The uniform slot, i.e. the binding number of the uniform block in the shader; 0 to 3.
/// \param value
/// 	The value to push. Only read during the call.
///
/// \return
/// 	The same result as the byte overload.
template <class T>
	requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline auto pushUniforms (
	SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, Uint32 slot, const T &value
) -> std::expected<void, BufferError>
{
	return pushUniforms(
		commandBuffer, stage, slot, std::span<const std::byte>(std::as_bytes(std::span(&value, 1)))
	);
}



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif  // ifndef __FCG_BUFFER_H__
