
#ifndef __FCG_BUFFER_H__
#define __FCG_BUFFER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_buffers Buffer facilities
/// \ingroup fcg_components
/// \brief GPU storage, host transfers, batched uploads, asynchronous readback, and pushed uniforms.
///
/// \section buffers_model Resource model and lifetime
/// \snippet buffer_examples.cpp includes
///
/// A \ref fcg::Buffer owns a fixed-size GPU allocation. Use SDL usage flags for
/// vertices, indices, indirect arguments, or graphics/compute storage. Instance data is a vertex buffer whose
/// pipeline description uses \c SDL_GPU_VERTEXINPUTRATE_INSTANCE. Storage bindings address whole buffers, not
/// arbitrary subranges. SDL does not expose a GPU uniform-buffer handle: \ref fcg::pushUniforms copies CPU values
/// into a command buffer's vertex, fragment, or compute uniform slot instead.
///
/// A \ref fcg::TransferBuffer is host-visible staging storage, created for either upload or download. Its
/// \ref fcg::TransferBuffer::Mapping guard unmaps on destruction. Destroy mappings before moving or destroying their
/// transfer buffer (including a readback ticket containing it). The device must outlive every resource. Resources
/// are move-only; moving transfers ownership and leaves the source empty. Buffer destruction asks SDL to release
/// storage when safe and never waits for GPU idle. None of these objects provides internal thread synchronization;
/// externally serialize access and record/submit command buffers on the thread that acquired them.
///
/// All sizes and offsets are in bytes, even in typed overloads. Typed uploads accept trivially copyable elements
/// and copy their object representations, without serializing, converting, or fixing shader layout. Uniforms must
/// follow std140 and storage data std430. In particular, align vec3/vec4 fields to 16 bytes and account for matrix
/// column and array strides. Trivially copyable does not mean shader-compatible. The portable uniform helper limits
/// each push to 4096 bytes (the descriptor range in SDL 3.4's Vulkan backend); use storage for larger blocks.
///
/// \section buffers_errors Construction, absence, and errors
/// Buffer and TransferBuffer have no default constructor. Their create() factories return either a fully
/// initialized resource or an error. Required resource members should be initialized directly from successful
/// factory results. Use \c std::optional for resources created later or released before their containing object.
/// Moving transfers ownership; the source has a null handle and zero size and may be destroyed or reassigned.
/// Moving an already moved-from owner transfers that empty state. Moving out of an optional does not disengage
/// the optional: explicitly reset it when it should represent absence. Never reset, replace, move, or destroy an
/// optional transfer buffer while a mapping still borrows it.
///
/// Factories return \c std::expected<T,BufferError>, and operations return \c std::expected<void,BufferError>.
/// Inspect the error's code and owned message; SDL failures capture SDL_GetError immediately. The facilities do not
/// log, so callers can decide how to report or recover. Standard allocation exceptions can still propagate from
/// C++ containers and strings. Moved-from owners reject resource operations with
/// \ref fcg::BufferErrorCode::InvalidState. Empty
/// uploads are no-ops on valid owners; zero-size allocations and downloads are errors. Buffers smaller than four
/// bytes use a four-byte SDL allocation internally while retaining the requested logical size.
///
/// \section buffers_upload Choosing an upload path
/// For occasional data, Buffer::create with initial data or Buffer::upload allocates temporary staging and submits
/// one copy pass. It copies CPU bytes once and returns without waiting. For repeated or multi-buffer updates,
/// retain an UploadBatch: it packs bytes directly into mapped staging pages, retains pages and descriptor capacity,
/// and amortizes allocation and command submission. Once staged, the input CPU span may be destroyed. Destinations
/// must remain alive until the queued operations have been recorded. record() uses a caller-owned copy pass;
/// submit() creates and submits a command buffer. Neither is called implicitly by a destructor.
///
/// The following snippets are compiled as part of the buffer examples target. Functions return errors to their
/// caller; pipeline/pass parameters must already be valid for the given device and data layout.
/// \snippet buffer_examples.cpp geometry
///
/// Keep required buffers together and make the entire resource optional when creation is delayed.
/// \snippet buffer_examples.cpp optional_ownership
///
/// A binding at slot 1 becomes per-instance when the pipeline uses the following description. Index element size
/// is supplied when binding, not when allocating the index buffer.
/// \snippet buffer_examples.cpp instances
///
/// Keep the batch outside your frame loop. After record() the caller must end the copy pass before rendering and
/// submit its command buffer later. A successful record only encodes work; it does not submit or complete it.
/// \snippet buffer_examples.cpp batch
///
/// \section buffers_cycling Partial updates and cycling
/// UploadBatch always cycles a staging page when mapping it for a new batch, protecting bytes referenced by earlier
/// commands. Destination cycling is independent and defaults to false. Setting it to true may select fresh GPU
/// storage: all contents not rewritten are undefined for subsequent commands, although earlier bound data remains
/// valid. For several writes building one replacement, cycle only the first write. Set false for a partial update
/// that must preserve other bytes. Never overwrite host staging still referenced by GPU work without cycling or
/// waiting for its fence. Noncycling GPU copies remain ordered on the GPU timeline; CPU mapping is not such a copy.
/// \snippet buffer_examples.cpp partial
///
/// For procedural data, map a TransferBuffer and write directly to avoid an intermediate CPU array. End the mapping
/// before recording the upload. With the low-level transfer path, the caller owns synchronization and submission.
/// \snippet buffer_examples.cpp mapped
///
/// \section buffers_shaders Uniforms, storage, and indirect commands
/// Uniform pushes copy immediately, so the input object can die afterwards. Data remains in a slot until another
/// push replaces it in the same command buffer. There are four slots per shader stage; slots must also match the
/// shader's declared resources. Pushing during a render or compute pass is allowed.
/// \snippet buffer_examples.cpp uniforms
///
/// Graphics shader creation must declare its storage resources as well as uniforms. SPIR-V uses set 0 for vertex
/// storage, set 2 for fragment storage, set 1 for vertex uniforms, and set 3 for fragment uniforms. Within a storage
/// set, samplers precede storage textures, which precede storage buffers, with consecutive bindings from zero.
/// \snippet buffer_examples.cpp graphics_storage
///
/// Compute read-only buffers are bound inside a pass; writable buffers are provided when beginning it. A writable
/// buffer can also be read by its shader. End a compute pass before another dependent dispatch: reads and writes
/// within the same compute pass are not implicitly synchronized. Configure compute shader bindings according to
/// SDL_CreateGPUComputePipeline; pipeline creation remains an SDL operation.
/// \snippet buffer_examples.cpp compute
///
/// Indirect argument buffers use SDL's argument structs and INDIRECT usage. Add COMPUTE_STORAGE_WRITE if a compute
/// shader generates them. The following functions assume the appropriate graphics or compute pipeline is bound.
/// \snippet buffer_examples.cpp indirect
///
/// GPU-to-GPU copies require no CPU staging. Both ranges must fit, and overlapping self-copies are rejected.
/// \snippet buffer_examples.cpp copy
///
/// \section buffers_readback Host readback and submission ordering
/// Buffer::readback allocates a download buffer and submits a separate copy with a fence. It sees work already
/// submitted, not commands pending in another unsubmitted command buffer. No host mirror or readback allocation is
/// kept by ordinary Buffer objects. A ticket can be destroyed while pending without waiting; SDL retires its storage
/// safely. The device retains abandoned fences until a later readback, idle wait, or teardown collects them.
/// ready() polls, wait() explicitly blocks, and map() reports NotReady until the fence is signaled. A mapping
/// borrows ticket storage and must end before the ticket moves or dies. A mapped download is a snapshot, not live
/// GPU memory. Copy bytes into your own objects with memcpy when you need typed CPU values.
/// \snippet buffer_examples.cpp readback
///
/// To download as part of an existing render/compute submission, call Buffer::downloadTo in a copy pass with a
/// caller-owned download TransferBuffer. End the pass, submit with SDL_SubmitGPUCommandBufferAndAcquireFence, and
/// wait or poll that fence before map(false). Never cycle a download mapping: that could discard the result. The
/// wrapper cannot inspect raw command-buffer ownership, pass state, or external fences; these remain caller
/// preconditions. See SDL's GPU overview for the underlying synchronization and shader-binding contracts.



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



//////
//
// Structs & enums
//

/// Categories of recoverable buffer failures. \ingroup fcg_buffers
enum class BufferErrorCode
{
	/// Invalid size, range, usage, slot, or device combination.
	InvalidArgument,

	/// Moved-from owner, duplicate mapping, or incompatible current state.
	InvalidState,

	/// A nonblocking readback mapping was requested before GPU completion.
	NotReady,

	/// SDL resource creation, mapping, submission, or waiting failed.
	SDLFailure
};

/// A self-contained failure; messages remain valid after subsequent SDL calls. \ingroup fcg_buffers
struct BufferError {
	/// Machine-readable error category for recovery decisions.
	BufferErrorCode code;

	/// Human-readable context, including captured SDL diagnostics when applicable.
	std::string message;
};



//////
//
// Classes
//

/// \brief Host-visible upload or download staging, with exclusive scoped mapping. \ingroup fcg_buffers
///
/// The device is borrowed and must outlive this owner and its mapping. Only one mapping may exist at a time. Map upload
/// storage with cycling when earlier commands may still reference it; download storage must be fence-ready and mapped
/// without cycling. This class cannot know about externally submitted fences. Moving or destroying a mapped owner
/// violates a precondition (asserted in debug builds). Destruction releases storage without waiting. There is no
/// default constructor, instances are obtained via \ref create . Only moves can leave an existing owner without storage.
///
/// All access needs to be externally serialized. See \ref fcg_buffers for complete transfer and synchronization
/// examples.
class FCG_FRAMEWORK_EXPORT TransferBuffer
{
public:

	////
	// Types

	// Forward declaration of our mapping guard class (defined outside for readability).
	class Mapping;


	////
	// Object construction/destruction

	/// Create staging memory. Does not map, submit, or wait.
	///
	/// \param device Borrowed device; must outlive the returned owner and any mapping.
	/// \param size Number of bytes; must be nonzero and fit \c Uint32.
	/// \param usage SDL upload or download direction (not a combination).
	/// \param properties Optional SDL creation properties; borrowed only during this call.
	///
	/// \returns
	/// 	An owner, \link BufferErrorCode::InvalidArgument InvalidArgument \endlink for invalid input, or
	/// 	\link BufferErrorCode::SDLFailure SDLFailure \endlink for allocation failure.
	[[nodiscard]] static auto create (
		Device &device, std::size_t size, SDL_GPUTransferBufferUsage usage, SDL_PropertiesID properties=0
	) -> std::expected<TransferBuffer, BufferError>;

	/// Release staging without waiting. There must be no live Mapping referring to this owner.
	~TransferBuffer ();

	/// A \c TransferBuffer cannot be copy-constructed (staging storage has unique ownership).
	TransferBuffer (const TransferBuffer&) = delete;

	/// A \c TransferBuffer cannot be copy-assigned (staging storage has unique ownership).
	auto operator= (const TransferBuffer&) -> TransferBuffer& = delete;

	/// Transfer ownership and empty the source without waiting. The source must not have a live mapping.
	///
	/// \param other Source owner, left without storage after the move.
	TransferBuffer (TransferBuffer &&other) noexcept;

	/// Move assignment. Release existing storage, take ownership, and empty the source. Neither owner may be mapped; no
	/// GPU wait.
	///
	/// \param other Source owner, left empty after a successful move; self-assignment is a no-op.
	///
	/// \returns A reference to this owner.
	auto operator= (TransferBuffer &&other) noexcept -> TransferBuffer&;


	////
	// Accessors

	/// Borrow the SDL handle, or null for a moved-from owner. Never release it through SDL; ownership stays here.
	///
	/// \returns The borrowed SDL handle, or nullptr after move; ownership stays with this object.
	[[nodiscard]] inline auto handle () const -> SDL_GPUTransferBuffer* {
		return m_handle;
	}

	/// Logical allocation size in bytes; zero for a moved-from owner. No synchronization is performed.
	///
	/// \returns The logical allocation size in bytes, or zero after move.
	[[nodiscard]] inline auto size () const -> std::size_t {
		return m_size;
	}

	/// Transfer direction. Meaningful only when handle() is non-null.
	///
	/// \returns The fixed creation usage; meaningful only while owning storage.
	[[nodiscard]] inline auto usage () const -> SDL_GPUTransferBufferUsage {
		return m_usage;
	}

	/// Whether a live mapping currently borrows this storage. Does not query GPU completion.
	///
	/// \returns True while an exclusive CPU mapping is active, false otherwise.
	[[nodiscard]] auto mapped () const -> bool {
		return m_mapped;
	}

	/// Borrow the associated device, or null for a moved-from owner. Does not extend its lifetime.
	///
	/// \returns The borrowed creating device, or nullptr after move.
	[[nodiscard]] auto device () const -> Device* {
		return m_device;
	}


	////
	// Methods

	/// Map the whole allocation without submitting or waiting. The returned guard unmaps automatically.
	///
	/// \pre
	/// 	Any GPU download has completed. With noncycling upload mapping, no GPU command may still read the bytes
	/// 	being changed. The owner must remain alive at its current address until the mapping ends.
	///
	/// \param cycle
	/// 	Select fresh storage if bound; allowed only for upload buffers. Defaults to false so download results cannot
	/// 	accidentally be discarded. Upload callers should normally pass true.
	///
	/// \returns
	/// 	A mapping guard for the transfer buffer, or \link BufferErrorCode::InvalidState InvalidState \endlink if
	/// 	moved from/already mapped, \link BufferErrorCode::InvalidArgument InvalidArgument \endlink for download
	/// 	cycling, or \link BufferErrorCode::SDLFailure SDLFailure \endlink if mapping fails. A failed call leaves the
	/// 	owner unmapped (or its existing mapping intact).
	[[nodiscard]] auto map (bool cycle=false) -> std::expected<Mapping, BufferError>;


private:

	////
	// Object construction/destruction

	/// Construct adopting a successful allocation. For internal use only, use \ref create to obtain an instance.
	///
	/// \param device Borrowed creating device; must outlive the allocation.
	/// \param handle Non-null SDL allocation whose ownership is transferred here.
	/// \param size Validated nonzero logical byte count.
	/// \param usage Validated transfer direction.
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

	/// The once-fixed transfer direction.
	SDL_GPUTransferBufferUsage m_usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;

	/// Whether exclusive CPU mapping is active.
	bool m_mapped = false;
};

/// \brief Exclusive mapping of a TransferBuffer; its destructor unmaps, without waiting. \ingroup fcg_buffers
///
/// Returned only after a successful map. The transfer owner must outlive this guard and stay at the same address.
/// Moving the guard transfers the mapping and empties the source; spans obtained before the move remain valid until the
/// new guard unmaps. Pointers must not escape that lifetime. The driver owns the memory: never free it. Download
/// mappings expose bytes only after caller-managed synchronization (or BufferReadback's fence check).
class FCG_FRAMEWORK_EXPORT TransferBuffer::Mapping
{
	////
	// Friend declarations

	/// Only the transfer owner can adopt a successful SDL mapping.
	friend class TransferBuffer;


public:

	////
	// Object construction/destruction

	/// Unmap if still active. Every pointer/span into this mapping is invalidated. Does not submit or wait.
	~Mapping ();

	/// A \c Mapping cannot be copy-constructed (exactly one guard must perform the unmap).
	Mapping (const Mapping&) = delete;

	/// A \c Mapping cannot be copy-assigned (exactly one guard must perform the unmap).
	auto operator= (const Mapping&) -> Mapping& = delete;

	/// The move constructor. Transfer the active mapping; the source becomes empty and no longer unmaps anything.
	///
	/// \param other Source owner, left empty after the move.
	Mapping (Mapping &&other) noexcept;

	/// Move assignment. Unmap this guard's previous storage, then take the other's mapping. The source becomes empty;
	/// no GPU wait.
	///
	/// \param other Source owner, left empty after a successful move; self-assignment is a no-op.
	///
	/// \returns A reference to this owner.
	auto operator= (Mapping &&other) noexcept -> Mapping&;


	////
	// Accessors

	/// Mutable mapped bytes, or an empty span after move/unmap. Valid only until this mapping ends.
	/// For downloads, mutations affect staging only, not the original GPU buffer.
	///
	/// \returns A span over the active mapping, or an empty span after move or unmap.
	[[nodiscard]] auto data () -> std::span<std::byte> {
		return m_data;
	}

	/// Read-only mapped bytes, or an empty span after move/unmap. Does not extend the mapping lifetime.
	///
	/// \returns A span over the active mapping, or an empty span after move or unmap.
	[[nodiscard]] auto data () const -> std::span<const std::byte> {
		return m_data;
	}


	////
	// Methods

	/// End the mapping early and invalidate its spans. Safe to call repeatedly; never submits or waits.
	void unmap ();


private:

	////
	// Object construction/destruction

	/// Construct adopting a successful mapping. For internal use only, use \ref TransferBuffer::map to obtain an
	/// instance.
	///
	/// \param owner The parent transfer buffer that is being mapped.
	/// \param data The view on the driver-owned transfer buffer bytes at the mapping.
	Mapping (TransferBuffer &owner, std::span<std::byte> data) : owner(&owner), m_data(data) {}


	////
	// Fields

	/// The parent transfer buffer that is being mapped. Needed so the guard can autonomously unmap and mark available
	/// again.
	TransferBuffer *owner = nullptr;

	/// Driver-owned bytes, invalid after unmapping.
	std::span<std::byte> m_data;
};

/// \brief A submitted buffer download and its completion fence. \ingroup fcg_buffers
///
/// Obtained from \ref Buffer::readback; owns staging and a fence, not the source GPU buffer. No source ownership is
/// needed after submission. Destruction may occur before completion and does not wait. Mapping is exclusive and
/// permitted only after the fence signals. Destroy the mapping before moving or destroying the ticket. The device must
/// outlive the ticket. Access must be externally serialized, including polling and waiting.
class FCG_FRAMEWORK_EXPORT BufferReadback
{
	////
	// Friend declarations

	/// Only Buffer::readback can adopt storage and its successfully submitted fence.
	friend class Buffer;


public:

	////
	// Object construction/destruction

	/// Release fence and download storage without waiting. No mapping may outlive this ticket.
	~BufferReadback ();

	/// A \c BufferReadback cannot be copy-constructed (a ticket uniquely owns its fence and storage).
	BufferReadback (const BufferReadback&) = delete;

	/// A \c BufferReadback cannot be copy-assigned (a ticket uniquely owns its fence and storage).
	auto operator= (const BufferReadback&) -> BufferReadback& = delete;

	/// The move constructor. Transfer the pending/completed operation and empty the source. No mapping may be active.
	///
	/// \param other Source owner, left empty after the move.
	BufferReadback (BufferReadback &&other) noexcept;

	/// Move assignment. Release this operation and take another, without waiting. Neither ticket may have a live
	/// mapping.
	///
	/// \param other Source owner, left empty after a successful move; self-assignment is a no-op.
	///
	/// \returns A reference to this owner.
	auto operator= (BufferReadback &&other) noexcept -> BufferReadback&;


	////
	// Methods

	/// Poll the fence without blocking. Returns false for an moved-from ticket; never maps or submits.
	///
	/// \returns True when this ticket's fence has signaled, false while pending or after move.
	[[nodiscard]] auto ready () const -> bool;

	/// Wait for this operation only, not the entire device. Repeated waits on a completed ticket are allowed.
	///
	/// \returns Success, InvalidState for a moved-from ticket, or SDLFailure if waiting fails.
	[[nodiscard]] auto wait () const -> std::expected<void, BufferError>;

	/// Map completed staging without waiting. The guard must end before moving or destroying this ticket.
	///
	/// \returns A mapping, NotReady if pending, InvalidState if moved from/already mapped, or SDLFailure.
	[[nodiscard]] auto map () -> std::expected<TransferBuffer::Mapping, BufferError>;


private:

	////
	// Object construction/destruction

	/// Construct adopting the given download storage and its successfully acquired completion fence.
	///
	/// \param owner The \c TransferBuffer that was used for downloading.
	/// \param data The view on the driver-owned transfer buffer bytes at the mapping.
	BufferReadback (TransferBuffer &&storage, std::unique_ptr<Device::RetiredFence> fence);


	////
	// Fields

	/// The download allocation, kept alive while the fence is pending.
	TransferBuffer m_storage;

	/// Preallocated fence ownership; handed to the device if abandoned.
	std::unique_ptr<Device::RetiredFence> m_fence;
};

/// \brief Fixed-size GPU memory with typed upload conveniences and explicit SDL interoperability. \ingroup fcg_buffers
///
/// Owns one \c SDL_GPUBuffer , borrows its device, and never retains a CPU mirror or permanent upload/download staging.
/// Use \ref UploadBatch for frequent uploads. Usage is fixed at creation; combine compatible SDL flags for resources
/// generated by compute and later consumed as vertices, storage, or indirect arguments. Allocations cannot resize, but
/// you can create a replacement and optionally copy old contents. All offsets and lengths are bytes. Destruction
/// releases through SDL without a device-wide wait. Access is externally synchronized; a handle does not extend
/// ownership. There is no default constructor. Obtain instances through \ref create . Only moves can leave an existing
/// owner without storage.
class FCG_FRAMEWORK_EXPORT Buffer
{
public:

	////
	// Object construction/destruction

	/// Allocate uninitialized GPU memory. No staging allocation, submission, or wait occurs.
	///
	/// \param device Borrowed creating device, which must outlive the buffer.
	/// \param size Logical bytes, nonzero and representable by Uint32; backing storage is at least four bytes.
	/// \param usage Nonzero SDL usage flags. VERTEX and INDEX cannot be combined.
	/// \param properties SDL creation properties, borrowed only for this call; zero means no properties.
	///
	/// \returns An owner, InvalidArgument for invalid parameters, or SDLFailure for allocation failure.
	[[nodiscard]] static auto create (
		Device &device, std::size_t size, SDL_GPUBufferUsageFlags usage, SDL_PropertiesID properties=0
	) -> std::expected<Buffer, BufferError>;

	/// Allocate and submit an initial upload. CPU data is copied before return; GPU completion is asynchronous.
	///
	/// \param device Borrowed device that must outlive the returned buffer.
	/// \param data Initial bytes, nonempty and at most Uint32 bytes.
	/// \param usage Fixed SDL buffer usage flags.
	/// \param properties Optional SDL allocation properties, borrowed during creation.
	///
	/// \returns An initialized owner or the allocation/upload error. Failure releases all temporary resources.
	[[nodiscard]] static auto create (
		Device &device, std::span<const std::byte> data, SDL_GPUBufferUsageFlags usage, SDL_PropertiesID properties=0
	) -> std::expected<Buffer, BufferError>;

	/// Typed initial upload; equivalent to the byte overload using std::as_bytes, with no layout conversion.
	///
	/// \tparam T Trivially copyable element type (possibly const).
	/// \tparam Extent Static or dynamic span extent.
	/// \param device Borrowed device. \param data Nonempty input elements, copied during the call.
	/// \param usage Fixed SDL usage flags. \param properties Optional creation properties.
	///
	/// \returns The created owner or the same errors as the byte overload; sizeof(T) contributes to byte size.
	template <class T, std::size_t Extent> requires std::is_trivially_copyable_v<T>
	[[nodiscard]] static auto create (
		Device &device, std::span<T, Extent> data, SDL_GPUBufferUsageFlags usage, SDL_PropertiesID properties=0
	) -> std::expected<Buffer, BufferError> {
		return create(device, std::span<const std::byte>(std::as_bytes(data)), usage, properties);
	}

	/// Release through SDL without waiting for GPU idle. Queued but unrecorded UploadBatch references must end first.
	~Buffer ();

	/// GPU allocations have unique ownership and cannot be copied.
	Buffer (const Buffer&) = delete;

	/// GPU allocations cannot be copy-assigned.
	auto operator= (const Buffer&) -> Buffer& = delete;

	/// Transfer ownership, leaving the source empty. Recorded SDL commands remain valid; no GPU wait occurs.
	///
	/// \param other Source owner, left empty after the move.
	Buffer (Buffer &&other) noexcept;

	/// Release current storage and take the other's allocation. The source becomes empty; no GPU wait occurs.
	///
	/// \param other Source owner, left empty after a successful move; self-assignment is a no-op.
	///
	/// \returns A reference to this owner.
	auto operator= (Buffer &&other) noexcept -> Buffer&;


	////
	// Accessors

	/// Borrow the SDL handle for indirect commands and other interoperability; null after move. Do not release it.
	///
	/// \returns The borrowed SDL handle, or nullptr after move; ownership stays with this object.
	[[nodiscard]] auto handle () const -> SDL_GPUBuffer* { return m_handle; }

	/// Fixed logical byte count, or zero after move; this is not an element count.
	///
	/// \returns The logical allocation size in bytes, or zero after move.
	[[nodiscard]] auto size () const -> std::size_t { return m_size; }

	/// Creation usage flags, or zero after move. Does not query or synchronize the GPU.
	///
	/// \returns The fixed creation usage; meaningful only while owning storage.
	[[nodiscard]] auto usage () const -> SDL_GPUBufferUsageFlags { return m_usage; }

	/// Borrow the creating device, or null after move. The caller must keep it alive independently.
	///
	/// \returns The borrowed creating device, or nullptr after move.
	[[nodiscard]] auto device () const -> Device* { return m_device; }


	////
	// Transfers

	/// Copy CPU bytes into temporary staging and submit one upload without waiting for completion.
	///
	/// \param data Bytes copied before return; an empty span is a no-op for a valid buffer/range.
	/// \param offset Destination byte offset; the entire span must fit the logical size.
	/// \param cycle Cycle destination storage if bound. False preserves other bytes; true may discard them.
	///
	/// \returns Success after submission, or a validation/SDL error. Success does not imply GPU completion.
	[[nodiscard]] auto upload (std::span<const std::byte> data, std::size_t offset=0, bool cycle=false)
		-> std::expected<void, BufferError>;

	/// Upload trivially copyable elements without conversion; delegates to the byte overload.
	///
	/// \tparam T Trivially copyable element type. \tparam Extent Static or dynamic span extent.
	///
	/// \param data Elements copied during the call. \param offset Destination offset in bytes, not elements.
	/// \param cycle Whether destination storage may be discarded. \returns The byte upload's result.
	template <class T, std::size_t Extent> requires std::is_trivially_copyable_v<T>
	[[nodiscard]] auto upload (std::span<T, Extent> data, std::size_t offset=0, bool cycle=false)
		-> std::expected<void, BufferError> {
		return upload(std::span<const std::byte>(std::as_bytes(data)), offset, cycle);
	}

	/// Record a staging-to-GPU copy into an active copy pass; never submits or waits.
	///
	/// \pre The pass is valid and source bytes are initialized; neither resource is released before recording.
	///
	/// \param pass Active copy pass on this device. \param source Unmapped UPLOAD staging on this device.
	/// \param size Bytes to copy (zero is a no-op after validation). \param sourceOffset Staging byte offset.
	/// \param destinationOffset GPU byte offset. \param cycle Whether destination contents may be discarded.
	///
	/// \returns Success after encoding, or an invalid-argument/state error. Both ranges must fit.
	[[nodiscard]] auto uploadFrom (
		SDL_GPUCopyPass *pass, const TransferBuffer &source, std::size_t size, std::size_t sourceOffset=0,
		std::size_t destinationOffset=0, bool cycle=false
	) -> std::expected<void, BufferError>;

	/// Record a GPU-to-staging copy; the caller submits and waits on a fence before mapping the destination.
	///
	/// \pre No earlier pending download or live mapping uses the destination range. Pass/device validity is caller-owned.
	///
	/// \param pass Active copy pass on this device. \param destination Unmapped DOWNLOAD staging on this device.
	/// \param size Nonzero bytes to download. \param sourceOffset GPU byte offset.
	/// \param destinationOffset Staging byte offset.
	///
	/// \returns Success after encoding, or an invalid-argument/state error; success is not host visibility.
	[[nodiscard]] auto downloadTo (
		SDL_GPUCopyPass *pass, TransferBuffer &destination, std::size_t size, std::size_t sourceOffset=0,
		std::size_t destinationOffset=0
	) const -> std::expected<void, BufferError>;

	/// Record a GPU-to-GPU copy. Subsequent GPU commands see the copied bytes; no CPU staging or wait is involved.
	///
	/// \param pass Active copy pass on the common device. \param destination Buffer to receive the bytes.
	/// \param size Bytes to copy; zero is a no-op after validation. \param sourceOffset Source byte offset.
	/// \param destinationOffset Destination byte offset. \param cycle Whether destination contents may be discarded.
	///
	/// \returns
	/// 	Success or a validation error. Both ranges must fit; overlapping self-copies are invalid. Self-copy with
	/// 	cycling is rejected because it can change which underlying allocation supplies the source.
	[[nodiscard]] auto copyTo (
		SDL_GPUCopyPass *pass, Buffer &destination, std::size_t size, std::size_t sourceOffset=0,
		std::size_t destinationOffset=0, bool cycle=false
	) const -> std::expected<void, BufferError>;

	/// Submit a separate download with its own staging and fence; returns without waiting for completion.
	///
	/// \pre Writes to be observed have already been submitted. Commands pending in other command buffers are excluded.
	///
	/// \param offset Source byte offset. \param size Bytes to copy; dynamic_extent means the remaining buffer.
	///
	/// \returns An owning ticket, or validation/SDL failure. Empty ranges are invalid. No host mirror is retained here.
	[[nodiscard]] auto readback (std::size_t offset=0, std::size_t size=std::dynamic_extent) const
		-> std::expected<BufferReadback, BufferError>;


	////
	// Bindings

	/// Build a borrowed SDL vertex/index descriptor; does not record commands or extend buffer lifetime.
	///
	/// \param offset Byte offset strictly within the buffer.
	///
	/// \returns A descriptor, or InvalidState/InvalidArgument for an empty, incompatible, or out-of-range buffer.
	[[nodiscard]] auto binding (std::size_t offset=0) const -> std::expected<SDL_GPUBufferBinding, BufferError>;

	/// Build a borrowed compute-write descriptor for SDL_BeginGPUComputePass; requires COMPUTE_STORAGE_WRITE.
	///
	/// \param cycle Whether pass startup may discard previous buffer contents.
	///
	/// \returns A descriptor or an moved-from/incompatible-buffer error. No pass is begun and no synchronization occurs.
	[[nodiscard]] auto readWriteBinding (bool cycle=false) const
		-> std::expected<SDL_GPUStorageBufferReadWriteBinding, BufferError>;

	/// Bind one vertex/instance buffer; requires VERTEX usage and an active compatible render pass.
	///
	/// \param pass Active render pass on this device. \param slot Pipeline vertex slot.
	/// \param offset Byte offset within the buffer; layout/stride are supplied by the pipeline.
	///
	/// \returns Success after encoding or a validation error. SDL validates pipeline-specific slot limits.
	[[nodiscard]] auto bindVertex (SDL_GPURenderPass *pass, Uint32 slot=0, std::size_t offset=0) const
		-> std::expected<void, BufferError>;

	/// Bind an index buffer; requires INDEX usage and a compatible active render pass.
	///
	/// \param pass Active render pass. \param elementSize SDL 16-bit or 32-bit index size.
	/// \param offset Byte offset, aligned to the chosen element size, with at least one element remaining.
	///
	/// \returns Success after encoding or a validation error. Does not draw or submit.
	[[nodiscard]] auto bindIndex (
		SDL_GPURenderPass *pass, SDL_GPUIndexElementSize elementSize, std::size_t offset=0
	) const -> std::expected<void, BufferError>;

	/// Bind this whole buffer as graphics read-only storage; requires GRAPHICS_STORAGE_READ usage.
	///
	/// \param pass Active render pass. \param stage VERTEX or FRAGMENT (COMPUTE is invalid for this overload).
	/// \param slot Storage-buffer slot declared by the shader, independent of sampler/texture slots.
	///
	/// \returns Success after encoding or a validation error. Shader counts/layout must match the pipeline.
	[[nodiscard]] auto bindStorage (SDL_GPURenderPass *pass, ShaderStage stage, Uint32 slot=0) const
		-> std::expected<void, BufferError>;

	/// Bind this whole buffer as compute read-only storage; requires COMPUTE_STORAGE_READ usage.
	///
	/// \param pass Active compute pass. \param slot Read-only storage-buffer slot declared by the compute shader.
	///
	/// \returns Success after encoding or a validation error. Writable bindings instead use readWriteBinding().
	[[nodiscard]] auto bindStorage (SDL_GPUComputePass *pass, Uint32 slot=0) const -> std::expected<void, BufferError>;


private:

	////
	// Object construction/destruction

	/// Construct adopting a successful allocation. For internal use only, use \ref create to obtain an instance.
	///
	/// \param device Borrowed creating device; must outlive the allocation.
	/// \param handle Non-null SDL allocation whose ownership is transferred here.
	/// \param size Validated nonzero logical byte count.
	/// \param usage Validated SDL usage flags.
	Buffer (Device &device, SDL_GPUBuffer *handle, std::size_t size, SDL_GPUBufferUsageFlags usage) noexcept
		: m_device(&device), m_handle(handle), m_size(size), m_usage(usage)
	{}


	////
	// Fields

	/// Borrowed creating device.
	Device *m_device = nullptr;

	/// Owned SDL allocation.
	SDL_GPUBuffer *m_handle = nullptr;

	/// Logical byte size, possibly smaller than SDL's minimum allocation.
	std::size_t m_size = 0;

	/// Fixed SDL usage mask.
	SDL_GPUBufferUsageFlags m_usage = 0;
};

/// \brief Reusable packed staging for several uploads and/or repeated frames. \ingroup fcg_buffers
///
/// \ref upload copies input immediately into mapped pages and queues descriptors, but records no commands. \ref record
/// unmaps pages and encodes all queued uploads in order into a borrowed pass; \ref submit owns a fresh command buffer.
/// Afterwards, the same batch can be reused while prior submissions are in flight: staging maps cycle automatically.
/// Page and descriptor allocations are retained until destruction; destination allocations are never owned. Keep
/// destinations alive until recording, and the device alive throughout. Destruction discards unrecorded work rather
/// than submitting it. Access is externally serialized. A moved-from batch rejects further uploads/recording.
class FCG_FRAMEWORK_EXPORT UploadBatch
{
public:

	////
	// Object construction/destruction

	/// Construct an empty batch borrowing the device. No SDL allocation, mapping, or submission occurs yet.
	/// \param device Borrowed device; must outlive this batch and its recorded resource use.
	explicit UploadBatch (Device &device);

	/// Unmap and release retained staging; discard queued descriptors without submitting or waiting.
	~UploadBatch ();

	/// An \c UploadBatch cannot be copy-constructed (batches uniquely own their staging).
	UploadBatch (const UploadBatch&) = delete;

	/// An \c UploadBatch cannot be copy-assigned (batches uniquely own their staging).
	auto operator= (const UploadBatch&) -> UploadBatch& = delete;

	/// The move constructor. Transfer pages and queued operations, leaving an inert source. Existing input copies
	/// remain queued.
	///
	/// \param other Source owner, left empty after the move.
	UploadBatch (UploadBatch &&other) noexcept;

	/// Move assignement. Discards this batch's pending work and takes the other's pages/queue. No implicit submission
	/// or GPU wait.
	///
	/// \param other Source owner, left empty after a successful move; self-assignment is a no-op.
	///
	/// \returns A reference to this owner.
	auto operator= (UploadBatch &&other) noexcept -> UploadBatch&;


	////
	// Methods

	/// Stage bytes for a later record()/submit(). Previously queued entries survive a failed staging operation.
	///
	/// \param destination Live buffer on the batch's device; must survive until recording.
	/// \param data Bytes copied before return; empty data adds no operation.
	/// \param offset Destination byte offset. \param cycle Forwarded to SDL for this destination write only.
	///
	/// \returns Success or a validation/allocation/mapping error. No GPU command is recorded yet.
	[[nodiscard]] auto upload (
		Buffer &destination, std::span<const std::byte> data, std::size_t offset=0, bool cycle=false
	) -> std::expected<void, BufferError>;

	/// Stage trivially copyable elements using their bytes, with no shader-layout conversion.
	///
	/// \tparam T Trivially copyable element type. \tparam Extent Static or dynamic span extent.
	///
	/// \param destination Borrowed GPU destination. \param data Elements copied during this call.
	/// \param offset Destination byte offset. \param cycle Whether this write may discard old destination contents.
	///
	/// \returns The same result as the byte overload; input elements need not survive successful staging.
	template <class T, std::size_t Extent> requires std::is_trivially_copyable_v<T>
	[[nodiscard]] auto upload (
		Buffer &destination, std::span<T, Extent> data, std::size_t offset=0, bool cycle=false
	) -> std::expected<void, BufferError> {
		return upload(destination, std::span<const std::byte>(std::as_bytes(data)), offset, cycle);
	}

	/// Unmap staging, encode queued copies in order, then clear the queue while retaining capacity.
	///
	/// \param pass Caller-owned active copy pass on the batch device; must be ended/submitted by its caller.
	///
	/// \returns
	/// 	Success or InvalidState/InvalidArgument for a moved-from owner/null pass. Failure before encoding preserves
	/// 	the queue. An empty queue is a no-op. Cannot verify raw pass device/thread ownership.
	[[nodiscard]] auto record (SDL_GPUCopyPass *pass) -> std::expected<void, BufferError>;

	/// Record and submit one owned command buffer without waiting. An empty valid batch is a successful no-op.
	///
	/// \returns
	/// 	Success or an SDL/state error. Acquisition failure preserves the queue. Once encoded, the queue is consumed
	/// 	even if submission fails; do not assume failed submissions updated any destination.
	[[nodiscard]] auto submit () -> std::expected<void, BufferError>;

	/// Discard queued writes and unmap pages, retaining capacity for reuse. Never submits or waits.
	void clear ();


private:

	////
	// Types

	/// Stable-address staging owner and its optional live mapping. For internal use only.
	struct Page;

	/// A single copy operation. For internal use only.
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

	/// Stable owners; mapped pages must never move.
	std::vector<std::unique_ptr<Page>> pages;

	/// Queued copies in user order, retaining capacity across submissions.
	std::vector<Copy> copies;
};



//////
//
// Functions
//

/// \brief Push a uniform byte block to one shader stage and slot. \ingroup fcg_buffers
///
/// SDL copies bytes immediately; the input may die after return. The value applies to subsequent draws/dispatches in
/// this command buffer until replaced in the same slot. Valid inside render or compute passes; does not submit or wait.
/// The caller supplies bytes observing \c std140 rules and matching their shader declarations. The portable block limit
/// is 4096 bytes, matching SDL 3.4's Vulkan uniform descriptor range.
///
/// \param commandBuffer Live command buffer, used on its acquiring thread.
/// \param stage VERTEX, FRAGMENT, or COMPUTE. \param slot Uniform slot in [0, 3].
/// \param data Nonempty std140-compatible bytes, at most 4096 bytes.
///
/// \returns Success after the push or InvalidArgument for invalid stage, slot, length, or null command buffer.
[[nodiscard]] FCG_FRAMEWORK_EXPORT auto pushUniforms (
	SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, Uint32 slot, std::span<const std::byte> data
) -> std::expected<void, BufferError>;

/// \brief Push contiguous uniform elements, rather than the representation of the span itself. \ingroup fcg_buffers
///
/// \tparam T Trivially copyable element type. \tparam Extent Static or dynamic extent.
///
/// \param commandBuffer Live command buffer. \param stage Shader stage. \param slot Uniform slot [0, 3].
/// \param data Contiguous std140-compatible data, copied immediately; no element layout conversion occurs.
///
/// \returns The byte overload's result; byte length is data.size_bytes(), not sizeof(span).
template <class T, std::size_t Extent>
	requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline auto pushUniforms (
	SDL_GPUCommandBuffer *commandBuffer, ShaderStage stage, Uint32 slot, std::span<T, Extent> data
) -> std::expected<void, BufferError>
{
	return pushUniforms(commandBuffer, stage, slot, std::span<const std::byte>(std::as_bytes(data)));
}

/// \brief Push the object representation of a uniform value, without allocating a CPU wrapper. \ingroup fcg_buffers
///
/// \tparam T Trivially copyable uniform structure; shader-compatible field alignment/padding is caller-owned.
///
/// \param commandBuffer Live command buffer. \param stage Shader stage. \param slot Uniform slot [0, 3].
/// \param value Value copied immediately, with sizeof(T) bytes. It need not outlive this call.
///
/// \returns The byte overload's result. This does not infer std140 layout or validate shader declarations.
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

// namespace fcg
}


#endif  // ifndef __FCG_BUFFER_H__
