
#ifndef __FCG_TEXTURE_H__
#define __FCG_TEXTURE_H__


//////
//
// Module documentation
//

/// \defgroup fcg_textures Texture facilities
/// \ingroup fcg_modules
/// \brief Owned textures and samplers, CPU uploads and downloads, and asynchronous texel readback.
///
/// <tt>\ref fcg::Texture "Texture"</tt> owns a GPU texture with fixed metadata: create it once with its format, size
/// and usage, upload pixels into it, and sample it in shaders. <tt>\ref fcg::Sampler "Sampler"</tt> owns the sampling
/// state used to read a texture. <tt>\ref fcg::TextureReadback "TextureReadback"</tt> downloads texture contents back
/// to the CPU, e.g. to read a depth value under the mouse cursor.
///
/// \section fcg_textures_usage Getting started
/// Create a texture, then upload its initial pixels:
/// \snippet texture_examples.cpp create_upload
/// The upload copies the CPU bytes and returns without waiting for the GPU; the data can be released afterwards.
/// Row pitch 0 in the transfer layout means tightly packed rows. Scene textures typically use
/// \c SDL_GPU_TEXTUREUSAGE_SAMPLER, and sRGB formats decode to linear values when sampled.
///
/// \section fcg_textures_workflows Common tasks
/// \subsection fcg_textures_workflows_images Uploading a decoded image
/// The Image library creates textures from decoded files through <tt>\ref fcg::Image::upload "Image::upload"</tt>.
/// Choose normal color management for display, <tt>\ref fcg::Image::uploadWithFallback "uploadWithFallback"</tt> to
/// supply missing encoding, or <tt>\ref fcg::Image::uploadStoredSamples "uploadStoredSamples"</tt> for numeric data
/// textures. To establish missing metadata while loading, use
/// <tt>\ref fcg::ImageLoader::load "ImageLoader::load"</tt> or direct handler loading through
/// <tt>\ref fcg::ImageFormatHandler::load "ImageFormatHandler::load"</tt>. See \ref fcg_images and
/// \ref fcg_image_loading for these workflows and the conditional sample-preservation policy.
///
/// \subsection fcg_textures_workflows_transfers Transfer paths
/// Besides the one-call upload shown above, transfers can be recorded manually into a copy pass with
/// <tt>\ref fcg::Texture::uploadFrom "uploadFrom"</tt> and <tt>\ref fcg::Texture::downloadTo "downloadTo"</tt> for
/// caller-controlled submission, <tt>\ref fcg::Texture::copyTo "copyTo"</tt> copies between textures entirely on the
/// GPU, and <tt>\ref fcg::Texture::validateRegion "validateRegion"</tt> checks a region against the texture's
/// metadata before use. A region addresses one mip level and array layer (or cube face); regions of 3D textures can
/// cover multiple depth slices. Compressed formats, multisample textures, and automatic mip generation are not
/// supported by transfers.
///
/// \subsection fcg_textures_workflows_partial Partial updates
/// Leave the \c cycle flag of an upload \c false for a partial update that must preserve the other texels, and set
/// it \c true when the upload replaces the whole content; cycling may replace the texture storage, leaving unwritten
/// contents undefined.
///
/// \subsection fcg_textures_workflows_readback Reading pixels back
/// <tt>\ref fcg::Texture::readback "Texture::readback"</tt> downloads the current contents of previously submitted
/// work. Wait for completion, then map the result and read texels:
/// \snippet texture_examples.cpp readback
/// <tt>\ref fcg::TextureReadback::Mapping::readTexel "readTexel"</tt> copies one texel-sized value without conversion;
/// the mapped bytes otherwise retain the GPU representation, and row pitches may exceed the row size, so use
/// <tt>\ref fcg::TextureReadback::layout "layout"</tt> to walk the data. Mapping before completion fails with
/// <tt>\ref fcg::TextureErrorCode::NotReady "NotReady"</tt>.
///
/// \section fcg_textures_lifetime Ownership and lifetime
/// The device must outlive every texture, sampler, and readback created from it. There are no default constructors;
/// create objects with the \c create factories. Objects are move-only: moving transfers ownership and leaves the
/// source empty, and operations on an empty source fail. Destruction never waits for the GPU. While a mapping of a
/// readback exists, the readback must neither be moved nor destroyed. None of these objects synchronizes internally.
///
/// \section fcg_textures_errors Handling errors
/// Factories return <tt>std::expected&lt;T, \ref fcg::TextureError "TextureError"&gt;</tt> and operations return
/// <tt>std::expected&lt;void, \ref fcg::TextureError "TextureError"&gt;</tt>. Inspect the error's
/// <tt>\ref fcg::TextureError::code "code"</tt> and <tt>\ref fcg::TextureError::message "message"</tt>. Every
/// transfer validates geometry and byte arithmetic before recording anything, so a failed transfer records nothing.
/// The facilities do not log, so you decide how to report or recover.
///
/// \see \ref fcg_buffers, \ref fcg_fullscreen, \ref fcg_images



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

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_textures
/// @{



//////
//
// Structs and enums
//

/// The categories of errors reported by texture and sampler operations.
enum class TextureErrorCode
{
	/// \brief
	/// 	A parameter is invalid: e.g. an inconsistent texture or sampler descriptor, a region outside the texture, a
	/// 	byte layout that does not fit the data or transfer buffer, or resources that belong to different devices or
	/// 	have mismatching formats, sizes or transfer directions.
	InvalidArgument,

	/// A texture, readback or transfer buffer is empty (e.g. moved from), or storage is already mapped.
	InvalidState,

	/// A readback was mapped before the GPU finished the download.
	NotReady,

	/// \brief
	/// 	The device does not support the requested format, usage or sample count, or a transfer involves a
	/// 	multisampled texture or a format other than an uncompressed color format or
	/// 	\c SDL_GPU_TEXTUREFORMAT_D32_FLOAT.
	UnsupportedFormat,

	/// SDL failed to allocate, map, submit or wait.
	SDLFailure
};

/// \brief
/// 	Error returned by texture and sampler operations: a category plus a human-readable message (including SDL's
/// 	message where relevant).
struct TextureError
{
	/// The error category.
	TextureErrorCode code;

	/// A human-readable description of the failure.
	std::string message;
};

/// \brief
/// 	A box of texels within one mip level and one array layer or cube face of a texture.
///
/// For 3D textures, the box can span several depth slices instead.
struct TextureRegion
{
	/// The mip level, where 0 is the full-resolution level.
	Uint32 mipLevel = 0;

	/// The array layer or cube face. Must be 0 for 3D textures.
	Uint32 layer = 0;

	/// The texel coordinates of the box corner within the mip level. For 3D textures, \c z is the first depth slice.
	glm::uvec3 offset{0};

	/// The size of the box in texels. All components must be nonzero, and \c z must be 1 except for 3D textures.
	glm::uvec3 extent{1};
};

/// \brief
/// 	The byte layout of texel data in CPU memory or in a transfer buffer.
///
/// The first texel of a region starts at \c offset. Consecutive rows start \c rowPitch bytes apart, and consecutive
/// depth slices start \c slicePitch bytes apart. A pitch of 0 selects tight packing, i.e. a row pitch of
/// <tt>extent.x</tt> times the texel size and a slice pitch of <tt>extent.y</tt> times the row pitch. Nonzero pitches
/// must be at least these tight sizes, and all byte values must fit into 32 bits.
///
/// Layouts in a <tt>\ref fcg::TransferBuffer "TransferBuffer"</tt> have additional requirements: \c offset and
/// \c rowPitch must be multiples of the texel size, and \c slicePitch must be a multiple of \c rowPitch. Layouts of
/// CPU memory passed to <tt>\ref fcg::Texture::upload "Texture::upload"</tt> may use arbitrary padding.
struct TextureTransferLayout
{
	/// The byte offset of the first texel.
	std::size_t offset = 0;

	/// The number of bytes between the starts of consecutive rows, or 0 for tight packing.
	std::size_t rowPitch = 0;

	/// The number of bytes between the starts of consecutive depth slices, or 0 for tight packing.
	std::size_t slicePitch = 0;
};



//////
//
// Classes
//

/// \brief
/// 	An asynchronous download of texture contents into CPU-accessible memory.
///
/// Obtain a readback from <tt>\ref fcg::Texture::readback "Texture::readback"</tt>. It captures the region as written
/// by all commands submitted before the readback was created. Check for completion with <tt>\ref ready</tt> or block
/// with <tt>\ref wait</tt>, then call <tt>\ref map</tt> to access the bytes. The bytes keep the representation of the
/// texture format; no conversion is applied. Rows are padded, so use <tt>\ref layout</tt> to locate texels, or read
/// individual texels with <tt>\ref Mapping::readTexel</tt>.
///
/// \snippet texture_examples.cpp readback
///
/// The device must outlive the readback. Destroying a readback never waits for the GPU, even while the download is
/// still pending. A moved-from readback is empty. Only assignment and destruction are valid; <tt>\ref ready</tt>
/// returns \c false and other operations return \c InvalidState. Calls on the same readback must not run concurrently.
class FCG_FRAMEWORK_EXPORT TextureReadback
{
public:

	////
	// Types

	/// \brief
	/// 	Read-only CPU access to the bytes of a completed readback.
	///
	/// Obtained from <tt>\ref TextureReadback::map</tt>. A readback can have only one mapping at a time. The bytes
	/// remain accessible until the mapping is destroyed or assigned to. The readback must neither be moved nor
	/// destroyed while a mapping of it exists. A moved-from mapping is empty: <tt>\ref data</tt> is empty and
	/// <tt>\ref readTexel</tt> returns \c InvalidState.
	class Mapping
	{

		////
		// Friend declarations

		/// Needs access to the private constructor to create mappings.
		friend class TextureReadback;

	public:

		////
		// Object construction/destruction

		/// End the CPU access. Never waits for the GPU.
		~Mapping() = default;

		/// Not copyable.
		Mapping(const Mapping&) = delete;

		/// Not copy-assignable.
		auto operator= (const Mapping&) -> Mapping& = delete;

		/// Take over the access of another mapping, leaving it empty.
		Mapping(Mapping&&) noexcept = default;

		/// End the current access and take over the access of another mapping, leaving it empty.
		///
		/// \return A reference to this mapping.
		auto operator= (Mapping&&) noexcept -> Mapping& = default;


		////
		// Accessors

		/// \brief
		/// 	The mapped bytes, including row padding.
		///
		/// The span becomes invalid when the mapping is destroyed or assigned to. Use <tt>\ref layout</tt> to locate
		/// individual texels.
		///
		/// \return The mapped bytes, or an empty span if the mapping is empty.
		[[nodiscard]] auto data () const -> std::span<const std::byte> { return m_mapping.data(); }

		/// The format of the read-back texture, which also defines the representation of the mapped texels.
		[[nodiscard]] auto format () const -> SDL_GPUTextureFormat { return m_format; }

		/// The size of the read-back region in texels.
		[[nodiscard]] auto extent () const -> glm::uvec3 { return m_extent; }

		/// The byte layout of the mapped data, with all pitches nonzero.
		[[nodiscard]] auto layout () const -> TextureTransferLayout { return m_layout; }


		////
		// Methods

		/// \brief
		/// 	Copy the bytes of one texel into a value of type \c T.
		///
		/// The bytes are copied as they are, without color or format conversion. The mapped data need not be aligned
		/// for \c T. Choose a type matching the format, e.g. \c glm::u8vec4 for \c SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
		/// or \c float for \c SDL_GPU_TEXTUREFORMAT_D32_FLOAT.
		///
		/// \tparam T
		/// 	A trivially copyable type whose size equals the texel size of <tt>\ref format</tt>.
		///
		/// \param coordinates
		/// 	The texel coordinates relative to the origin of the read-back region. Each component must be less than
		/// 	the corresponding component of <tt>\ref extent</tt>.
		///
		/// \return
		/// 	The texel value. Returns \c InvalidState if the mapping is empty, or \c InvalidArgument if the size of
		/// 	\c T differs from the texel size or the coordinates are outside the region.
		template <class T> requires std::is_trivially_copyable_v<T>
		[[nodiscard]] auto readTexel (glm::uvec3 coordinates) const -> std::expected<T, TextureError>
		{
			if (data().empty())
				return std::unexpected(TextureError{TextureErrorCode::InvalidState, "Texture mapping is empty"});
			if (sizeof(T) != m_texelSize || glm::any(glm::greaterThanEqual(coordinates, m_extent)))
				return std::unexpected(
					TextureError{TextureErrorCode::InvalidArgument, "Texel size or coordinates are invalid"}
				);
			const auto offset = m_layout.offset + coordinates.z * m_layout.slicePitch
				+ coordinates.y * m_layout.rowPitch + coordinates.x * m_texelSize;
			std::array<std::byte, sizeof(T)> value;
			std::memcpy(value.data(), data().data() + offset, sizeof(T));
			return std::bit_cast<T>(value);
		}


	private:

		////
		// Object construction/destruction

		/// Adopt a mapping of the readback's staging storage together with the already validated geometry.
		Mapping(
			TransferBuffer::Mapping &&mapping,
			SDL_GPUTextureFormat format,
			glm::uvec3 extent,
			TextureTransferLayout layout,
			std::size_t texelSize
		)
			: m_mapping(std::move(mapping)), m_format(format), m_extent(extent), m_layout(layout),
			  m_texelSize(texelSize)
		{}


		////
		// Fields

		/// The mapping of the readback's staging storage. It refers to the readback, which therefore must not move.
		TransferBuffer::Mapping m_mapping;

		/// The format of the read-back texture.
		SDL_GPUTextureFormat m_format;

		/// The size of the read-back region in texels.
		glm::uvec3 m_extent;

		/// The resolved byte layout of the staging data, with all pitches nonzero.
		TextureTransferLayout m_layout;

		/// The size of one texel in bytes.
		std::size_t m_texelSize;
	};


	////
	// Object construction/destruction

	/// \brief
	/// 	Start reading back a region of a texture described by a raw SDL region.
	///
	/// Prefer <tt>\ref fcg::Texture::readback "Texture::readback"</tt>, which validates the region against the
	/// texture. This function records the download into its own command buffer and submits it without waiting.
	///
	/// \pre
	/// 	The texture belongs to \p device, has the format \p format, is single-sample and supports downloads, and the
	/// 	region lies within one of its subresources. \p device outlives the returned readback.
	///
	/// \param device
	/// 	The device that owns the texture.
	/// \param source
	/// 	The region to read back. The texture must be non-null and the size nonzero.
	/// \param format
	/// 	The format of the texture: an uncompressed color format or \c SDL_GPU_TEXTUREFORMAT_D32_FLOAT.
	///
	/// \return
	/// 	The pending readback. Returns \c InvalidArgument if the texture is null, the size is zero, the coordinates
	/// 	overflow or the data would exceed SDL's 32-bit size limit, \c UnsupportedFormat for unsupported formats, or
	/// 	\c SDLFailure if allocating or submitting fails.
	[[nodiscard]] static auto create (Device &device, const SDL_GPUTextureRegion &source, SDL_GPUTextureFormat format)
		-> std::expected<TextureReadback, TextureError>;

	/// \brief
	/// 	Release the readback. Never waits for the GPU, even while the download is still pending.
	///
	/// \pre No mapping of this readback exists.
	~TextureReadback();

	/// Not copyable.
	TextureReadback(const TextureReadback&) = delete;

	/// Not copy-assignable.
	auto operator= (const TextureReadback&) -> TextureReadback& = delete;

	/// \brief
	/// 	Take over another readback, leaving it empty.
	///
	/// \pre No mapping of \p other exists.
	///
	/// \param other The readback to move from.
	TextureReadback(TextureReadback &&other) noexcept;

	/// \brief
	/// 	Release this readback without waiting for the GPU and take over another, leaving it empty.
	///
	/// \pre No mapping of either readback exists.
	///
	/// \param other The readback to move from.
	///
	/// \return A reference to this readback.
	auto operator= (TextureReadback &&other) noexcept -> TextureReadback&;


	////
	// Accessors

	/// The format of the read-back texture, which also defines the representation of the mapped texels.
	[[nodiscard]] auto format () const -> SDL_GPUTextureFormat { return m_format; }

	/// The size of the read-back region in texels.
	[[nodiscard]] auto extent () const -> glm::uvec3 { return m_region.extent; }

	/// The texture region that is read back. Texel coordinates of a mapping are relative to its offset.
	[[nodiscard]] auto region () const -> TextureRegion { return m_region; }

	/// The byte layout of the mapped data. Rows are padded, so the pitches are generally larger than tight packing.
	[[nodiscard]] auto layout () const -> TextureTransferLayout { return m_layout; }


	////
	// Methods

	/// Check whether the download has completed. Never blocks.
	///
	/// \return \c true if the data can be mapped, \c false while the download is pending or if the readback is empty.
	[[nodiscard]] auto ready () const -> bool;

	/// \brief
	/// 	Block until the download has completed.
	///
	/// Waits only for this readback, not for other GPU work. Calling it again after completion returns immediately.
	///
	/// \return
	/// 	Nothing on success. Returns \c InvalidState if the readback is empty, or \c SDLFailure if waiting fails.
	[[nodiscard]] auto wait () const -> std::expected<void, TextureError>;

	/// \brief
	/// 	Map the downloaded data for reading on the CPU. Never waits for the GPU.
	///
	/// \pre The readback is neither moved nor destroyed while the returned mapping exists.
	///
	/// \return
	/// 	The mapping. Returns \c InvalidState if the readback is empty or already mapped, \c NotReady if the
	/// 	download has not completed yet, or \c SDLFailure if mapping fails.
	[[nodiscard]] auto map () -> std::expected<Mapping, TextureError>;


private:

	////
	// Object construction/destruction

	/// Adopt the staging storage, the fence of the submitted download and the resolved geometry.
	TextureReadback(
		TransferBuffer &&storage,
		std::unique_ptr<Device::RetiredFence> fence,
		SDL_GPUTextureFormat format,
		TextureRegion region,
		TextureTransferLayout layout
	);


	////
	// Fields

	/// The download staging storage. It must not move while mapped, since the mapping refers to it.
	TransferBuffer m_storage;

	/// The fence of the submitted download, handed to the device for nonblocking release on destruction.
	std::unique_ptr<Device::RetiredFence> m_fence;

	/// The format of the read-back texture.
	SDL_GPUTextureFormat m_format;

	/// The texture region that is read back.
	TextureRegion m_region;

	/// The resolved byte layout of the staging data, with rows padded to multiples of 256 bytes.
	TextureTransferLayout m_layout;
};

/// \brief
/// 	A GPU texture owned by the application.
///
/// Create a texture with <tt>\ref create</tt>. Its contents are undefined until they are written by an upload, a
/// copy or rendering. The creation parameters are fixed and available from <tt>\ref info</tt>. Data can be
/// transferred in several ways:
/// - <tt>\ref upload</tt> copies bytes from CPU memory and submits the transfer immediately. This is the simplest way
///   to fill a texture.
/// - <tt>\ref uploadFrom</tt> and <tt>\ref downloadTo</tt> record transfers between the texture and a
///   <tt>\ref fcg::TransferBuffer "TransferBuffer"</tt> into a copy pass of your own command buffer.
/// - <tt>\ref copyTo</tt> records a copy into another texture.
/// - <tt>\ref readback</tt> starts an asynchronous download into CPU memory.
///
/// Transfers support single-sample textures with uncompressed color formats or \c SDL_GPU_TEXTUREFORMAT_D32_FLOAT.
/// Mip levels are not generated automatically. Every transfer validates all parameters first; if validation fails,
/// nothing is recorded or submitted.
///
/// Several transfers accept a \c cycle flag. If it is \c true and the texture is still in use by previously recorded
/// commands, SDL writes into fresh storage instead of waiting for those commands. All contents of the whole texture
/// that are not written by the transfer then become undefined. Leave cycling disabled for partial updates.
///
/// When sampled in a shader, textures with sRGB formats return linear RGB values. Alpha is always linear and not
/// premultiplied.
///
/// The device must outlive the texture. Destroying a texture never waits for the GPU. A moved-from texture is empty.
/// Only assignment and destruction are valid; other operations return \c InvalidState. Calls on the same texture
/// must not run concurrently.
class FCG_FRAMEWORK_EXPORT Texture
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Create a texture with undefined contents.
	///
	/// \pre \p device outlives the returned texture.
	///
	/// \param device
	/// 	The device on which to create the texture.
	/// \param info
	/// 	The SDL texture descriptor. Width and height are limited to 16384 texels (2048 for 3D textures), array
	/// 	layers and depth to 2048, and the number of mip levels to the length of the full mip chain.
	///
	/// \return
	/// 	The new texture. Returns \c InvalidArgument for an invalid or inconsistent descriptor (e.g. zero or too
	/// 	large dimensions, unknown or incompatible usage flags, or wrong layer counts for the texture type),
	/// 	\c UnsupportedFormat if the device does not support the format with the requested type, usage or sample
	/// 	count, or \c SDLFailure if allocation fails.
	[[nodiscard]] static auto create (Device &device, const SDL_GPUTextureCreateInfo &info)
		-> std::expected<Texture, TextureError>;

	/// Release the texture. Never waits for the GPU; SDL frees the memory once the GPU no longer uses it.
	~Texture();

	/// Not copyable.
	Texture(const Texture&) = delete;

	/// Not copy-assignable.
	auto operator= (const Texture&) -> Texture& = delete;

	/// Take over another texture, leaving it empty.
	///
	/// \param other The texture to move from.
	Texture(Texture &&other) noexcept;

	/// Release this texture without waiting for the GPU and take over another, leaving it empty.
	///
	/// \param other The texture to move from.
	///
	/// \return A reference to this texture.
	auto operator= (Texture &&other) noexcept -> Texture&;


	////
	// Accessors

	/// The SDL texture, or \c nullptr if empty. The texture keeps ownership; never release it yourself.
	[[nodiscard]] auto handle () const -> SDL_GPUTexture* { return m_handle; }

	/// The device the texture was created on, or \c nullptr if empty.
	[[nodiscard]] auto device () const -> Device* { return m_device; }

	/// The descriptor the texture was created with. Its \c props member is always 0.
	[[nodiscard]] auto info () const -> const SDL_GPUTextureCreateInfo& { return m_info; }

	/// The region covering all of mip level 0 of layer 0, including every depth slice of a 3D texture.
	[[nodiscard]] auto baseRegion () const -> TextureRegion;

	/// \brief
	/// 	Check whether a region can be used in transfers with this texture. Records nothing.
	///
	/// \param region The region to check.
	///
	/// \return
	/// 	Nothing if the region is valid. Returns \c InvalidState if the texture is empty, \c UnsupportedFormat if the
	/// 	texture is multisampled or its format does not support transfers, or \c InvalidArgument if the mip level,
	/// 	layer or texel box lies outside the texture or the extent has a zero component.
	[[nodiscard]] auto validateRegion (const TextureRegion &region) const -> std::expected<void, TextureError>;


	////
	// Transfers

	/// \brief
	/// 	Upload texel data from CPU memory into a region of the texture.
	///
	/// Copies \p data into newly allocated staging memory, then records the upload into a separate command buffer and
	/// submits it without waiting. \p data may be modified or released as soon as the call returns.
	///
	/// \param data
	/// 	The texel data in the representation of the texture format.
	/// \param region
	/// 	The region to write.
	/// \param layout
	/// 	The byte layout of \p data. Arbitrary offsets and padding are allowed; the default is tight packing.
	/// \param cycle
	/// 	Whether SDL may write into fresh storage if the texture is still in use (see the class description).
	///
	/// \return
	/// 	Nothing on success. Returns any error of <tt>\ref validateRegion</tt>, \c InvalidArgument if \p layout is
	/// 	invalid or \p data is too small for it, or \c SDLFailure if allocating, mapping or submitting fails.
	[[nodiscard]] auto upload (
		std::span<const std::byte> data,
		const TextureRegion &region,
		TextureTransferLayout layout={},
		bool cycle=false
	) -> std::expected<void, TextureError>;

	/// \overload
	///
	/// Uploads into the whole <tt>\ref baseRegion</tt>.
	[[nodiscard]] auto upload (std::span<const std::byte> data, TextureTransferLayout layout={}, bool cycle=false)
		-> std::expected<void, TextureError> { return upload(data, baseRegion(), layout, cycle); }

	/// \brief
	/// 	Record an upload from a transfer buffer into a region of the texture.
	///
	/// The upload is recorded into \p pass; submitting the command buffer is up to the caller. The transfer buffer
	/// contents are read when the GPU executes the upload.
	///
	/// \pre \p pass is an active copy pass of a command buffer of the texture's device.
	///
	/// \param pass
	/// 	The copy pass to record into.
	/// \param source
	/// 	An unmapped upload transfer buffer on the same device as the texture.
	/// \param region
	/// 	The region to write.
	/// \param layout
	/// 	The byte layout of the data in \p source, observing the transfer buffer requirements of
	/// 	<tt>\ref fcg::TextureTransferLayout "TextureTransferLayout"</tt>.
	/// \param cycle
	/// 	Whether SDL may write into fresh storage if the texture is still in use (see the class description).
	///
	/// \return
	/// 	Nothing on success. Returns any error of <tt>\ref validateRegion</tt>, \c InvalidState if \p source is empty
	/// 	or mapped, or \c InvalidArgument if \p pass is null, \p source belongs to another device or is not an upload
	/// 	buffer, or \p layout is invalid or exceeds the size of \p source.
	[[nodiscard]] auto uploadFrom (
		SDL_GPUCopyPass *pass,
		const TransferBuffer &source,
		const TextureRegion &region,
		TextureTransferLayout layout={},
		bool cycle=false
	) -> std::expected<void, TextureError>;

	/// \overload
	///
	/// Uploads into the whole <tt>\ref baseRegion</tt>.
	[[nodiscard]] auto uploadFrom (
		SDL_GPUCopyPass *pass,
		const TransferBuffer &source,
		TextureTransferLayout layout={},
		bool cycle=false
	) -> std::expected<void, TextureError>
		{ return uploadFrom(pass, source, baseRegion(), layout, cycle); }

	/// \brief
	/// 	Record a download from a region of the texture into a transfer buffer.
	///
	/// The download is recorded into \p pass; submitting the command buffer is up to the caller. Map \p destination
	/// only after the GPU has completed the command buffer, e.g. by waiting on a fence obtained from
	/// \c SDL_SubmitGPUCommandBufferAndAcquireFence. For a simpler asynchronous download, use <tt>\ref readback</tt>.
	///
	/// \pre \p pass is an active copy pass of a command buffer of the texture's device.
	///
	/// \param pass
	/// 	The copy pass to record into.
	/// \param destination
	/// 	An unmapped download transfer buffer on the same device as the texture.
	/// \param region
	/// 	The region to read.
	/// \param layout
	/// 	The byte layout of the data in \p destination, observing the transfer buffer requirements of
	/// 	<tt>\ref fcg::TextureTransferLayout "TextureTransferLayout"</tt>.
	///
	/// \return
	/// 	Nothing on success. Returns any error of <tt>\ref validateRegion</tt>, \c InvalidState if \p destination is
	/// 	empty or mapped, or \c InvalidArgument if \p pass is null, \p destination belongs to another device or is
	/// 	not a download buffer, or \p layout is invalid or exceeds the size of \p destination.
	[[nodiscard]] auto downloadTo (
		SDL_GPUCopyPass *pass,
		const TransferBuffer &destination,
		const TextureRegion &region,
		TextureTransferLayout layout={}
	) const -> std::expected<void, TextureError>;

	/// \overload
	///
	/// Downloads the whole <tt>\ref baseRegion</tt>.
	[[nodiscard]] auto downloadTo (
		SDL_GPUCopyPass *pass,
		const TransferBuffer &destination,
		TextureTransferLayout layout={}
	) const -> std::expected<void, TextureError>
		{ return downloadTo(pass, destination, baseRegion(), layout); }

	/// \brief
	/// 	Record a copy from a region of this texture into a region of another texture.
	///
	/// The copy is recorded into \p pass; submitting the command buffer is up to the caller. Both regions may belong
	/// to the same texture as long as they lie in different subresources.
	///
	/// \pre \p pass is an active copy pass of a command buffer of the textures' device.
	///
	/// \param pass
	/// 	The copy pass to record into.
	/// \param destination
	/// 	The texture to write, with the same device and format as this texture.
	/// \param sourceRegion
	/// 	The region of this texture to read.
	/// \param destinationRegion
	/// 	The region of \p destination to write. Its extent must equal that of \p sourceRegion.
	/// \param cycle
	/// 	Whether SDL may write into fresh storage if \p destination is still in use (see the class description).
	///
	/// \return
	/// 	Nothing on success. Returns any error of <tt>\ref validateRegion</tt> for either texture, or
	/// 	\c InvalidArgument if \p pass is null, the devices, formats or extents differ, or both regions lie in the
	/// 	same subresource of the same texture.
	[[nodiscard]] auto copyTo (
		SDL_GPUCopyPass *pass,
		Texture &destination,
		const TextureRegion &sourceRegion,
		const TextureRegion &destinationRegion,
		bool cycle=false
	) const -> std::expected<void, TextureError>;

	/// \overload
	///
	/// Copies the whole <tt>\ref baseRegion</tt> of this texture into the whole base region of \p destination. Both
	/// base regions must have the same size.
	[[nodiscard]] auto copyTo (SDL_GPUCopyPass *pass, Texture &destination, bool cycle=false) const
		-> std::expected<void, TextureError>
		{ return copyTo(pass, destination, baseRegion(), destination.baseRegion(), cycle); }

	/// \brief
	/// 	Start an asynchronous download of a region into CPU-accessible memory.
	///
	/// Records the download into a separate command buffer and submits it without waiting. The readback captures the
	/// region as written by all commands submitted before this call.
	///
	/// \pre The device of this texture outlives the returned readback.
	///
	/// \param region The region to read.
	///
	/// \return
	/// 	The pending readback. Returns any error of <tt>\ref validateRegion</tt>, \c InvalidArgument if the data
	/// 	would exceed SDL's 32-bit size limit, or \c SDLFailure if allocating or submitting fails.
	[[nodiscard]] auto readback (const TextureRegion &region) const -> std::expected<TextureReadback, TextureError>;

	/// \overload
	///
	/// Reads back the whole <tt>\ref baseRegion</tt>.
	[[nodiscard]] auto readback () const -> std::expected<TextureReadback, TextureError>
		{ return readback(baseRegion()); }


private:

	////
	// Object construction/destruction

	/// Adopt a successfully created allocation and its descriptor, clearing the descriptor's creation properties.
	Texture(Device &device, SDL_GPUTexture *handle, SDL_GPUTextureCreateInfo info)
		: m_device(&device), m_handle(handle), m_info(info) { m_info.props = 0; }


	////
	// Fields

	/// The borrowed creating device.
	Device *m_device = nullptr;

	/// The owned SDL texture.
	SDL_GPUTexture *m_handle = nullptr;

	/// The creation descriptor, with \c props cleared.
	SDL_GPUTextureCreateInfo m_info{};
};

/// \brief
/// 	A GPU sampler owned by the application, describing how shaders filter and address textures.
///
/// The device must outlive the sampler. Destroying a sampler never waits for the GPU. A moved-from sampler is empty
/// and its <tt>\ref handle</tt> is \c nullptr.
class FCG_FRAMEWORK_EXPORT Sampler
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Create a sampler.
	///
	/// \pre \p device outlives the returned sampler.
	///
	/// \param device
	/// 	The device on which to create the sampler.
	/// \param info
	/// 	The SDL sampler descriptor.
	///
	/// \return
	/// 	The new sampler. Returns \c InvalidArgument if a filter, mipmap mode, address mode or enabled compare
	/// 	operation is out of range, an LOD value is not finite, \c min_lod is negative or exceeds \c max_lod, or
	/// 	anisotropy is enabled with \c max_anisotropy outside [1, 16]. Returns \c SDLFailure if creation fails.
	[[nodiscard]] static auto create (Device &device, const SDL_GPUSamplerCreateInfo &info)
		-> std::expected<Sampler, TextureError>;

	/// Release the sampler. Never waits for the GPU; SDL frees it once the GPU no longer uses it.
	~Sampler();

	/// Not copyable.
	Sampler(const Sampler&) = delete;

	/// Not copy-assignable.
	auto operator= (const Sampler&) -> Sampler& = delete;

	/// Take over another sampler, leaving it empty.
	///
	/// \param other The sampler to move from.
	Sampler(Sampler &&other) noexcept;

	/// Release this sampler without waiting for the GPU and take over another, leaving it empty.
	///
	/// \param other The sampler to move from.
	///
	/// \return A reference to this sampler.
	auto operator= (Sampler &&other) noexcept -> Sampler&;


	////
	// Accessors

	/// The SDL sampler, or \c nullptr if empty. The sampler keeps ownership; never release it yourself.
	[[nodiscard]] auto handle () const -> SDL_GPUSampler* { return m_handle; }

	/// The device the sampler was created on, or \c nullptr if empty.
	[[nodiscard]] auto device () const -> Device* { return m_device; }


private:

	////
	// Object construction/destruction

	/// Adopt a successfully created sampler.
	Sampler(Device &device, SDL_GPUSampler *handle) : m_device(&device), m_handle(handle) {}


	////
	// Fields

	/// The borrowed creating device.
	Device *m_device = nullptr;

	/// The owned SDL sampler.
	SDL_GPUSampler *m_handle = nullptr;
};



//////
//
// Functions
//

/// \brief
/// 	Determine the size of one texel of a format that supports transfers.
///
/// \param format The texture format.
///
/// \return
/// 	The texel size in bytes for uncompressed color formats and \c SDL_GPU_TEXTUREFORMAT_D32_FLOAT, or
/// 	\c UnsupportedFormat for compressed formats and all other depth/stencil formats.
[[nodiscard]] FCG_FRAMEWORK_EXPORT auto textureTexelSize (SDL_GPUTextureFormat format)
	-> std::expected<std::size_t, TextureError>;



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_TEXTURE_H__
