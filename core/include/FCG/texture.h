
#ifndef __FCG_TEXTURE_H__
#define __FCG_TEXTURE_H__


//////
//
// Includes
//

// C++ STL
#include <array>
#include <bit>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <type_traits>

// GLM library
#include <glm/glm.hpp>

// Framework
#include <FCG/buffer.h>



//////
//
// Namespaces open
//

namespace fcg {

/// \defgroup fcg_textures Texture facilities
/// \ingroup fcg_components
/// \brief Owned textures, samplers, explicit transfers, and asynchronous copied texel readback.
///
/// <tt>\ref fcg::Texture</tt> retains immutable SDL metadata and borrows its <tt>\ref fcg::Device</tt>.
/// No owner has a default constructor; use factories and optional resource slots. Moves leave empty handles.
/// Release never waits. Devices outlive all owners, access is externally serialized, and command buffers and passes
/// belong to the device supplied by the caller. Every transfer validates geometry and byte arithmetic before encoding.
///
/// CPU uploads copy a byte span before return and submit without waiting. Recorded transfers use unmapped
/// <tt>\ref fcg::TransferBuffer</tt> storage and leave submission and synchronization to the caller. Pitch zero means
/// tightly packed. CPU sources may have arbitrary byte padding; GPU staging row pitches are multiples of the texel
/// size, and GPU slice pitches are multiples of rows. Automatic uploads repack CPU bytes into aligned staging.
/// A region addresses one mip and array layer or cube face; 3D regions can cover multiple depth slices.
/// Compressed and multisample transfers and automatic mip generation are not supported.
///
/// <tt>\ref fcg::TextureReadback</tt> snapshots previously submitted work and owns aligned staging and a fence.
/// Pending abandonment uses nonblocking device fence retirement. Poll with <tt>\ref fcg::TextureReadback::ready</tt>,
/// explicitly wait with <tt>\ref fcg::TextureReadback::wait</tt>, then map. Mapping before completion returns
/// <tt>\ref fcg::TextureErrorCode::NotReady</tt>. The ticket stays alive at the same address until its mapping ends.
/// Download bytes retain GPU representation; <tt>\ref fcg::TextureReadback::Mapping::readTexel</tt> copies a texel-sized
/// trivially copyable value without conversion or typed references into driver storage.
/// \snippet texture_examples.cpp readback
///
/// Errors own their diagnostics and do not log. Standard allocation exceptions can propagate. Cycling may replace
/// texture storage, making unwritten contents undefined; leave cycling disabled for partial updates.
/// All textures used by scene rendering represent linear samples, with sRGB texture formats decoding encoded RGB.
/// Alpha remains straight and linear.
///
/// \addtogroup fcg_textures
/// @{




//////
//
// Structs and enums
//

/// Recoverable texture operation failures.
enum class TextureErrorCode {

	/// Invalid descriptor, region, pitch, capacity, or device agreement.
	InvalidArgument,

	/// Empty owner or conflicting active mapping.
	InvalidState,

	/// The download fence has not signaled.
	NotReady,

	/// Unsupported format, usage, or multisampled transfer.
	UnsupportedFormat,

	/// SDL allocation, mapping, or submission failed.
	SDLFailure
};

/// An owned diagnostic, independent of SDL's error storage.
struct TextureError {

	/// Machine-readable category.
	TextureErrorCode code;

	/// Operation context and backend diagnostic.
	std::string message;
};

/// One mip and array layer (or cube face), with a texel box in that subresource.
struct TextureRegion {

	/// Mip level.
	Uint32 mipLevel = 0;

	/// Array layer or cube face; zero for 3D textures.
	Uint32 layer = 0;

	/// Texel offset within the mip.
	glm::uvec3 offset{0};

	/// Nonzero texel dimensions; depth is one except for 3D textures.
	glm::uvec3 extent{1};
};

/// Byte layout of host or staging data. Zero pitches select tight packing.
struct TextureTransferLayout {

	/// Starting byte; GPU staging offsets must align to the texel size.
	std::size_t offset = 0;

	/// Bytes between rows; GPU staging pitches are multiples of the texel size.
	std::size_t rowPitch = 0;

	/// Bytes between depth slices; GPU staging pitches are multiples of the row pitch.
	std::size_t slicePitch = 0;
};



//////
//
// Classes
//

/// Submitted texture snapshot. Devices outlive tickets; abandon pending work without waiting.
/// Destroy mappings before moving or destroying the ticket. Externally serialize access.
class FCG_FRAMEWORK_EXPORT TextureReadback
{
public:

	////
	// Types

	/// Exclusive read-only CPU access with copied texel reads.
	class Mapping
	{

		////
		// Friend declarations

		/// The ticket supplies validated geometry when adopting a transfer mapping.
		friend class TextureReadback;

	public:

		////
		// Object construction/destruction

		/// Release scoped access without waiting.
		~Mapping() = default;

		/// Mappings uniquely own their unmap guard.
		Mapping(const Mapping&) = delete;

		/// Mappings cannot be copied.
		auto operator= (const Mapping&) -> Mapping& = delete;

		/// Transfer scoped access, leaving empty bytes in the source.
		Mapping(Mapping&&) noexcept = default;

		/// Unmap previous storage and transfer scoped access.
		auto operator= (Mapping&&) noexcept -> Mapping& = default;


		////
		// Accessors

		/// Read-only allocation bytes, including padding; expire on unmapping.
		[[nodiscard]] auto data () const -> std::span<const std::byte> { return m_mapping.data(); }

		/// Original GPU format; no conversion is performed.
		[[nodiscard]] auto format () const -> SDL_GPUTextureFormat { return m_format; }

		/// Downloaded region dimensions, in texels.
		[[nodiscard]] auto extent () const -> glm::uvec3 { return m_extent; }

		/// Effective byte pitches and offset.
		[[nodiscard]] auto layout () const -> TextureTransferLayout { return m_layout; }


		////
		// Methods

		/// Copy one texel representation, without alignment requirements or color conversion.
		///
		/// \tparam T Trivially copyable representation, exactly the format's texel size.
		/// \param coordinates Coordinates relative to the downloaded region.
		/// \return A copied value or a state, size, or coordinate error.
		template <class T> requires std::is_trivially_copyable_v<T>
		[[nodiscard]] auto readTexel (glm::uvec3 coordinates) const -> std::expected<T, TextureError>
		{
			if (data().empty())
				return std::unexpected(TextureError{TextureErrorCode::InvalidState, "Texture mapping is empty"});
			if (sizeof(T) != m_texelSize || glm::any(glm::greaterThanEqual(coordinates, m_extent)))
				return std::unexpected(TextureError{TextureErrorCode::InvalidArgument, "Texel size or coordinates are invalid"});
			const auto offset = m_layout.offset + coordinates.z * m_layout.slicePitch
				+ coordinates.y * m_layout.rowPitch + coordinates.x * m_texelSize;
			std::array<std::byte, sizeof(T)> value;
			std::memcpy(value.data(), data().data() + offset, sizeof(T));
			return std::bit_cast<T>(value);
		}


	private:

		////
		// Object construction/destruction

		/// Adopt an already validated download mapping.
		Mapping(
			TransferBuffer::Mapping &&mapping,
			SDL_GPUTextureFormat format,
			glm::uvec3 extent,
			TextureTransferLayout layout,
			std::size_t texelSize
		)
			: m_mapping(std::move(mapping)), m_format(format), m_extent(extent), m_layout(layout), m_texelSize(texelSize)
		{}


		////
		// Fields

		/// Guard borrowing stationary ticket storage.
		TransferBuffer::Mapping m_mapping;

		/// Source format.
		SDL_GPUTextureFormat m_format;

		/// Region dimensions.
		glm::uvec3 m_extent;

		/// Validated effective pitches.
		TextureTransferLayout m_layout;

		/// Bytes per uncompressed texel.
		std::size_t m_texelSize;
	};


	////
	// Object construction/destruction

	/// Download a borrowed texture with explicitly supplied format and geometry.
	///
	/// \pre Source belongs to the device, supports GPU downloads, is single-sample, and its actual metadata covers the region and format.
	/// \param device Borrowed device that outlives this ticket.
	/// \param source Non-null source region, from previously submitted work.
	/// \param format Actual source format; uncompressed color or \c D32_FLOAT.
	/// \return A submitted ticket or an owned diagnostic. Rows are aligned to 256 bytes.
	[[nodiscard]] static auto create (Device &device, const SDL_GPUTextureRegion &source, SDL_GPUTextureFormat format)
		-> std::expected<TextureReadback, TextureError>;

	/// Retire the fence and release storage without waiting; no mapping may remain.
	~TextureReadback();

	/// Tickets uniquely own their storage and fence.
	TextureReadback(const TextureReadback&) = delete;

	/// Tickets cannot be copied.
	auto operator= (const TextureReadback&) -> TextureReadback& = delete;

	/// Transfer ownership; neither ticket may be mapped.
	TextureReadback(TextureReadback&&) noexcept;

	/// Retire previous work and transfer ownership, without waiting.
	auto operator= (TextureReadback&&) noexcept -> TextureReadback&;


	////
	// Accessors

	/// Snapshot format.
	[[nodiscard]] auto format () const -> SDL_GPUTextureFormat { return m_format; }

	/// Snapshot region dimensions.
	[[nodiscard]] auto extent () const -> glm::uvec3 { return m_region.extent; }

	/// Original source geometry; mapping coordinates are relative to this region.
	[[nodiscard]] auto region () const -> TextureRegion { return m_region; }

	/// Effective staging layout, including aligned rows.
	[[nodiscard]] auto layout () const -> TextureTransferLayout { return m_layout; }


	////
	// Methods

	/// Poll completion; false after move. Never waits or maps.
	[[nodiscard]] auto ready () const -> bool;

	/// Wait only for this submission; repeated waits are allowed.
	[[nodiscard]] auto wait () const -> std::expected<void, TextureError>;

	/// Map completed storage. Returns \c NotReady while pending; never waits.
	///
	/// \pre Ticket remains alive and stationary until the returned mapping is destroyed.
	[[nodiscard]] auto map () -> std::expected<Mapping, TextureError>;


private:

	////
	// Object construction/destruction

	/// Adopt staging, geometry, and a submitted fence.
	TextureReadback(
		TransferBuffer &&storage,
		std::unique_ptr<Device::RetiredFence> fence,
		SDL_GPUTextureFormat format,
		TextureRegion region,
		TextureTransferLayout layout
	);


	////
	// Fields

	/// Stationary download allocation while mapped.
	TransferBuffer m_storage;

	/// Preallocated nonblocking fence retirement node.
	std::unique_ptr<Device::RetiredFence> m_fence;

	/// Snapshot format.
	SDL_GPUTextureFormat m_format;

	/// Original source region geometry.
	TextureRegion m_region;

	/// Effective aligned byte layout.
	TextureTransferLayout m_layout;
};

/// Unique texture allocation with immutable metadata. Device outlives the owner.
/// No host mirror or permanent staging is retained. Release never waits. Access is externally serialized.
class FCG_FRAMEWORK_EXPORT Texture
{
public:

	////
	// Object construction/destruction

	/// Validate a descriptor and allocate uninitialized storage, without submission or waiting.
	[[nodiscard]] static auto create (Device &device, const SDL_GPUTextureCreateInfo &info)
		-> std::expected<Texture, TextureError>;

	/// Release the allocation without waiting.
	~Texture();

	/// Textures cannot be copied.
	Texture(const Texture&) = delete;

	/// Textures cannot be copy-assigned.
	auto operator= (const Texture&) -> Texture& = delete;

	/// Transfer ownership, leaving an empty source.
	Texture(Texture&&) noexcept;

	/// Release previous storage and transfer ownership.
	auto operator= (Texture&&) noexcept -> Texture&;


	////
	// Accessors

	/// Borrowed SDL handle; null after move. Never release it directly.
	[[nodiscard]] auto handle () const -> SDL_GPUTexture* { return m_handle; }

	/// Borrowed device; null after move.
	[[nodiscard]] auto device () const -> Device* { return m_device; }

	/// Retained creation descriptor; creation properties are cleared after allocation.
	[[nodiscard]] auto info () const -> const SDL_GPUTextureCreateInfo& { return m_info; }

	/// Whole base level of layer zero; includes every depth slice for 3D textures.
	[[nodiscard]] auto baseRegion () const -> TextureRegion;

	/// Validate region and transfer support, without issuing commands.
	[[nodiscard]] auto validateRegion (const TextureRegion &region) const -> std::expected<void, TextureError>;


	////
	// Transfers

	/// Copy CPU bytes to aligned staging and submit an upload without waiting. CPU data may die on return.
	[[nodiscard]] auto upload (
		std::span<const std::byte> data,
		const TextureRegion &region,
		TextureTransferLayout layout={},
		bool cycle=false
	) -> std::expected<void, TextureError>;

	/// Upload the whole base level of layer zero.
	[[nodiscard]] auto upload (std::span<const std::byte> data, TextureTransferLayout layout={}, bool cycle=false)
		-> std::expected<void, TextureError> { return upload(data, baseRegion(), layout, cycle); }

	/// Record an upload from unmapped staging on the same device. Caller owns synchronization and submission.
	[[nodiscard]] auto uploadFrom (
		SDL_GPUCopyPass *pass,
		const TransferBuffer &source,
		const TextureRegion &region,
		TextureTransferLayout layout={},
		bool cycle=false
	) -> std::expected<void, TextureError>;

	/// Upload the complete base level in a caller-owned pass.
	[[nodiscard]] auto uploadFrom (
		SDL_GPUCopyPass *pass,
		const TransferBuffer &source,
		TextureTransferLayout layout={},
		bool cycle=false
	) -> std::expected<void, TextureError>
		{ return uploadFrom(pass, source, baseRegion(), layout, cycle); }

	/// Record a download to unmapped staging. Caller must fence completion before mapping.
	[[nodiscard]] auto downloadTo (
		SDL_GPUCopyPass *pass,
		const TransferBuffer &destination,
		const TextureRegion &region,
		TextureTransferLayout layout={}
	) const -> std::expected<void, TextureError>;

	/// Download the complete base level in a caller-owned pass.
	[[nodiscard]] auto downloadTo (
		SDL_GPUCopyPass *pass,
		const TransferBuffer &destination,
		TextureTransferLayout layout={}
	) const -> std::expected<void, TextureError>
		{ return downloadTo(pass, destination, baseRegion(), layout); }

	/// Record a same-format, same-device GPU copy with equally sized regions. Self-copy in one subresource is rejected.
	[[nodiscard]] auto copyTo (
		SDL_GPUCopyPass *pass,
		Texture &destination,
		const TextureRegion &sourceRegion,
		const TextureRegion &destinationRegion,
		bool cycle=false
	) const -> std::expected<void, TextureError>;

	/// Copy complete base levels of layer zero.
	[[nodiscard]] auto copyTo (SDL_GPUCopyPass *pass, Texture &destination, bool cycle=false) const
		-> std::expected<void, TextureError> { return copyTo(pass, destination, baseRegion(), destination.baseRegion(), cycle); }

	/// Submit a snapshot download of previously submitted work, without waiting.
	[[nodiscard]] auto readback (const TextureRegion &region) const -> std::expected<TextureReadback, TextureError>;

	/// Snapshot the complete base level of layer zero.
	[[nodiscard]] auto readback () const -> std::expected<TextureReadback, TextureError> { return readback(baseRegion()); }


private:

	////
	// Object construction/destruction

	/// Adopt a validated allocation.
	Texture(Device &device, SDL_GPUTexture *handle, SDL_GPUTextureCreateInfo info)
		: m_device(&device), m_handle(handle), m_info(info) { m_info.props = 0; }


	////
	// Fields

	/// Borrowed creating device.
	Device *m_device = nullptr;

	/// Owned allocation.
	SDL_GPUTexture *m_handle = nullptr;

	/// Retained metadata.
	SDL_GPUTextureCreateInfo m_info{};
};

/// Move-only sampler owner. Device outlives it; destruction does not wait.
class FCG_FRAMEWORK_EXPORT Sampler
{
public:

	////
	// Object construction/destruction

	/// Allocate a sampler after validating filter, address, and LOD settings.
	[[nodiscard]] static auto create (Device &device, const SDL_GPUSamplerCreateInfo &info)
		-> std::expected<Sampler, TextureError>;

	/// Release without waiting.
	~Sampler();

	/// Samplers cannot be copied.
	Sampler(const Sampler&) = delete;

	/// Samplers cannot be copy-assigned.
	auto operator= (const Sampler&) -> Sampler& = delete;

	/// Transfer ownership and empty the source.
	Sampler(Sampler&&) noexcept;

	/// Release previous sampler and take ownership.
	auto operator= (Sampler&&) noexcept -> Sampler&;


	////
	// Accessors

	/// Borrowed SDL handle; null after move.
	[[nodiscard]] auto handle () const -> SDL_GPUSampler* { return m_handle; }

	/// Borrowed creating device; null after move.
	[[nodiscard]] auto device () const -> Device* { return m_device; }


private:

	////
	// Object construction/destruction

	/// Adopt a successful allocation.
	Sampler(Device &device, SDL_GPUSampler *handle) : m_device(&device), m_handle(handle) {}


	////
	// Fields

	/// Borrowed device.
	Device *m_device = nullptr;

	/// Owned SDL sampler.
	SDL_GPUSampler *m_handle = nullptr;
};



//////
//
// Functions
//

/// Bytes per supported transfer texel, or \c UnsupportedFormat for compressed and other depth formats.
[[nodiscard]] FCG_FRAMEWORK_EXPORT auto textureTexelSize (SDL_GPUTextureFormat format)
	-> std::expected<std::size_t, TextureError>;



/// @}



//////
//
// Namespaces close
//

} // namespace fcg


#endif // ifndef __FCG_TEXTURE_H__
