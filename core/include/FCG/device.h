
#ifndef __FCG_DEVICE_H__
#define __FCG_DEVICE_H__


//////
//
// Module documentation
//

/// \defgroup fcg_devices Devices and shaders
/// \ingroup fcg_modules
/// \brief The GPU device, its window claims, and shader creation for pipelines.
///
/// <tt>\ref fcg::Device "Device"</tt> represents one GPU: everything else in the framework renders through the device
/// that <tt>\ref fcg::Device::create "create"</tt> initializes with the best available backend (Vulkan, Direct3D 12, or
/// Metal). Shaders for pipelines are created from SPIR-V bytecode with
/// <tt>\ref fcg::Device::createShader "createShader"</tt>, which declares the shader's resource counts.
///
/// \section fcg_devices_usage Getting started
/// Applications run by <tt>\ref fcg::run "run"</tt> already have their device: what applets do themselves is create
/// shaders for their pipelines. A shader stage is created from SPIR-V bytecode, its entry point, and its resource
/// counts:
/// \snippet device_examples.cpp shader_creation
/// The declared counts must match what the shader's GLSL actually uses. Each graphics stage has four binding sets:
/// set 0 holds the vertex stage's samplers, storage textures and storage buffers (in that order, binding numbers
/// counting up from 0), set 1 its uniform blocks; sets 2 and 3 hold the same for the fragment stage. Uniform blocks
/// are what <tt>\ref fcg::pushUniforms "pushUniforms"</tt> writes into.
/// The <tt>\ref fcg::Device::createShader "numUniformBlocks overload"</tt> declares uniform blocks only; use the
/// <tt>\ref fcg::Device::createShader "ShaderResources overload"</tt> for shaders that also use samplers or storage.
///
/// \section fcg_devices_workflows Common tasks
/// \subsection fcg_devices_workflows_windows Claiming windows
/// A window must be claimed by the device before frames can be acquired from it.
/// <tt>\ref fcg::Device::claimWindow "claimWindow"</tt> prepares a window for GPU rendering; a window can be claimed
/// by only one device at a time, and <tt>\ref fcg::Device::unclaimWindow "unclaimWindow"</tt> releases it. In
/// applications run by <tt>\ref fcg::run "run"</tt>, the main window is claimed automatically.
///
/// \subsection fcg_devices_workflows_custom_loops Custom main loops
/// Applications that manage their own loop instead of <tt>\ref fcg::run "run"</tt> create the device and claim their
/// windows themselves:
/// \snippet device_examples.cpp device_setup
///
/// \section fcg_devices_lifetime Ownership and lifetime
/// Every GPU resource (buffers, textures, shaders, pipelines) belongs to the device it was created from and must be
/// destroyed before the device. A claimed window must outlive the device, or be released with
/// <tt>\ref fcg::Device::unclaimWindow "unclaimWindow"</tt> first; destroying a claimed window terminates the
/// application. The device cannot be copied or moved. <tt>\ref fcg::Device::waitIdle "waitIdle"</tt> and the destructor
/// wait until the GPU has finished all submitted work.
///
/// \section fcg_devices_errors Handling errors
/// <tt>\ref fcg::Device::create "create"</tt> returns an empty optional when no GPU could be initialized; the reason
/// is written to the SDL log. <tt>\ref fcg::Device::createShader "createShader"</tt> returns \c nullptr on invalid
/// input (unsupported stage, empty bytecode, more than four uniform blocks) or on compilation failure, also logged.
///
/// \see \ref fcg_buffers, \ref fcg_textures, \ref fcg_resources, \ref fcg_windows



//////
//
// Includes
//

// C++ STL
#include <memory>
#include <span>
#include <string_view>
#include <set>
#include <optional>
#include <mutex>

// Local includes
#include "FCG/export.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPUDevice;
struct SDL_GPUShader;
struct SDL_GPUFence;

// Framework types
namespace fcg {
	class Window;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_devices
/// @{



//////
//
// Structs and enums
//

/// Identifies a shader stage of the SDL GPU pipeline.
enum class ShaderStage
{
	/// The vertex shader stage of a graphics pipeline.
	VERTEX,

	/// The fragment shader stage of a graphics pipeline.
	FRAGMENT,

	/// The compute shader stage of a compute pipeline.
	COMPUTE
};



/// \brief
/// 	Resource counts declared by one graphics shader stage, passed to \c Device::createShader.
///
/// The counts must exactly match the resources the shader declares. When writing GLSL for SPIR-V, a vertex shader
/// declares its samplers, storage textures and storage buffers in descriptor set 0 and its uniform blocks in set 1. A
/// fragment shader uses set 2 for samplers, storage textures and storage buffers, and set 3 for uniform blocks. Within
/// the sampler/storage set, bindings are numbered consecutively from 0: first all samplers, then all storage textures,
/// then all storage buffers. Uniform blocks are numbered from binding 0 in their set; the binding number is the slot
/// used with \c pushUniforms. For example, a fragment shader with one sampler and two storage buffers declares the
/// sampler at <tt>set = 2, binding = 0</tt> and the storage buffers at bindings 1 and 2 of set 2.
///
/// Storage buffers use the \c std430 layout and uniform blocks use \c std140. See \ref fcg_buffers for a
/// storage-buffer shader example. All counts default to zero.
struct ShaderResources
{
	/// Number of uniform blocks; at most four per stage.
	unsigned uniformBuffers = 0;

	/// Number of read-only storage buffers.
	unsigned storageBuffers = 0;

	/// Number of read-only storage textures.
	unsigned storageTextures = 0;

	/// Number of combined texture-samplers (e.g. GLSL \c sampler2D).
	unsigned samplers = 0;
};



//////
//
// Classes
//

/// \brief
/// 	The GPU device shared by all windows and applets.
///
/// All rendering in the framework goes through this single device, so GPU resources such as textures, buffers and
/// pipelines can be used freely across windows. For example, an applet can render into a texture and use that texture
/// while drawing into the main window. SDL selects the most appropriate backend for the platform (Vulkan, Direct3D 12
/// or Metal).
///
/// When using \c fcg::run, the framework creates the device and passes it to every applet callback; applications
/// normally don't create a device themselves.
///
/// Every resource created from the device must be destroyed before the device. A window must be claimed with
/// \c claimWindow before it can be rendered into. A claimed window must outlive the device that claimed it, unless it
/// is released with \c unclaimWindow first. Destroying a window that is still claimed is a fatal error that terminates
/// the program.
class FCG_FRAMEWORK_EXPORT Device
{
	////
	// Friend declarations

	/// Needs access to the internal fence management for buffer readbacks.
	friend class Buffer;

	/// Needs access to the internal fence management for releasing pending readbacks without blocking.
	friend class BufferReadback;

	/// Needs access to the internal fence management for releasing pending readbacks without blocking.
	friend class TextureReadback;

	/// Manages the device on behalf of the running application.
	friend class Player;


	////
	// Types

	/// \brief
	/// 	Node in the list of fences of abandoned readbacks.
	///
	/// Readbacks allocate their node before submission, so that abandoning a pending readback neither allocates nor
	/// blocks.
	struct RetiredFence
	{
		/// Fence held until its submission completes.
		SDL_GPUFence *handle = nullptr;

		/// Next retired fence in the list; unlinked iteratively during collection.
		std::unique_ptr<RetiredFence> next;
	};

	/// Passkey that restricts use of the public constructor to \c Device::create.
	class PrivateConstructorKey final
	{
		/// Constructs the key in \c Device::create.
		friend Device;

		/// Private default constructor; a no-op without any runtime cost.
		constexpr PrivateConstructorKey() noexcept = default;
	};


public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Construct a device wrapping the given SDL GPU device handle.
	///
	/// Not callable by applications; use \c create instead.
	///
	/// \param key
	/// 	Passkey that only \c Device can create.
	/// \param handle
	/// 	SDL GPU device handle. The device takes ownership and destroys it on destruction.
	explicit Device([[maybe_unused]] PrivateConstructorKey key, SDL_GPUDevice *handle)
		: m_handle(handle)
	{}

	/// \brief
	/// 	Create a GPU device that supports SPIR-V, DXIL and MSL shaders.
	///
	/// Failures are written to the SDL log.
	///
	/// \note
	/// 	In debug builds, the device is created with SDL GPU validation enabled, which reports incorrect use of the
	/// 	SDL GPU API in the SDL log.
	///
	/// \return
	/// 	The device, or \c std::nullopt if SDL could not create a GPU device.
	[[nodiscard]] static auto create () -> std::optional<Device>;

	/// \brief
	/// 	Wait for the GPU to finish all pending work, release all window claims and destroy the device.
	///
	/// \pre
	/// 	All resources created from this device have been destroyed, and every window still claimed by this device
	/// 	is alive.
	~Device();

	/// Not copyable.
	Device(const Device&) = delete;

	/// Not copy-assignable.
	auto operator= (const Device&) -> Device& = delete;


	////
	// Accessors

	/// \brief
	/// 	Access the underlying SDL GPU device, e.g. for creating pipelines or acquiring command buffers.
	///
	/// \return
	/// 	The SDL GPU device handle, valid until this device is destroyed. The device keeps ownership; never destroy
	/// 	the handle yourself.
	[[nodiscard]] inline auto handle () const -> SDL_GPUDevice* {
		return m_handle;
	}


	////
	// Methods

	/// Block until the GPU has finished all work submitted so far.
	void waitIdle () const;

	/// \brief
	/// 	Claim the given window for this device, enabling rendering into it.
	///
	/// A window can be claimed by at most one device at a time.
	///
	/// \pre
	/// 	\c window is not null. The window must outlive this device, or be released with \c unclaimWindow before
	/// 	it is destroyed.
	///
	/// \param window
	/// 	The window to claim.
	///
	/// \return
	/// 	\c true if the window was claimed, \c false if SDL failed to claim the window or to create the resources
	/// 	needed for presenting to it. Failures are written to the SDL log.
	///
	/// \throws std::logic_error
	/// 	If the window is already claimed by this or another device.
	[[nodiscard]] auto claimWindow (std::unique_ptr<Window> &window) -> bool;

	/// \brief
	/// 	Release this device's claim on the given window.
	///
	/// Blocks until the GPU has finished all pending work, then releases the window's GPU resources. Afterwards, the
	/// window can be destroyed or claimed again.
	///
	/// \pre
	/// 	\c window is not null.
	///
	/// \param window
	/// 	The window to release.
	///
	/// \throws std::logic_error
	/// 	If the window is not currently claimed by this device.
	void unclaimWindow (std::unique_ptr<Window> &window);

	/// \brief
	/// 	Create a vertex or fragment shader from SPIR-V bytecode that uses only uniform blocks.
	///
	/// The shader declares no samplers, storage textures or storage buffers. Use the overload taking
	/// <tt>\ref ShaderResources "ShaderResources"</tt> for shaders that use them. On backends other than Vulkan, the
	/// SPIR-V is translated to the backend's shader language when the shader is created.
	///
	/// \param stage
	/// 	The shader stage; must be <tt>ShaderStage::VERTEX</tt> or <tt>ShaderStage::FRAGMENT</tt>.
	/// \param spirv
	/// 	The non-empty SPIR-V bytecode. Only read during the call.
	/// \param numUniformBlocks
	/// 	The number of uniform blocks the shader declares; at most 4.
	/// \param entrypoint
	/// 	The name of the shader's entry point function.
	///
	/// \return
	/// 	The shader, or \c nullptr on failure, with details written to the SDL log. The caller owns the shader and
	/// 	must release it with \c SDL_ReleaseGPUShader. Pipelines keep their own reference, so the shader can be
	/// 	released as soon as all pipelines using it have been created.
	[[nodiscard]] auto createShader (
		ShaderStage stage, std::span<const std::byte> spirv, unsigned numUniformBlocks,
		std::string_view entrypoint="main"
	) const -> SDL_GPUShader*;

	/// \brief
	/// 	Create a vertex or fragment shader from SPIR-V bytecode with explicit resource counts.
	///
	/// On backends other than Vulkan, the SPIR-V is translated to the backend's shader language when the shader is
	/// created. Compute pipelines are created directly through SDL.
	///
	/// \param stage
	/// 	The shader stage; must be <tt>ShaderStage::VERTEX</tt> or <tt>ShaderStage::FRAGMENT</tt>.
	/// \param spirv
	/// 	The non-empty SPIR-V bytecode. Only read during the call.
	/// \param resources
	/// 	The resource counts declared by the shader, following the binding rules described for
	/// 	<tt>\ref ShaderResources "ShaderResources"</tt>. At most 4 uniform blocks are allowed.
	/// \param entrypoint
	/// 	The name of the shader's entry point function.
	///
	/// \return
	/// 	The shader, or \c nullptr on failure, with details written to the SDL log. The caller owns the shader and
	/// 	must release it with \c SDL_ReleaseGPUShader. Pipelines keep their own reference, so the shader can be
	/// 	released as soon as all pipelines using it have been created.
	[[nodiscard]] auto createShader (
		ShaderStage stage, std::span<const std::byte> spirv, const ShaderResources &resources,
		std::string_view entrypoint="main"
	) const -> SDL_GPUShader*;


private:

	////
	// Methods

	/// \brief
	/// 	Release a completed fence immediately, or keep it in the retirement list until a later collection.
	///
	/// Never waits and never allocates. Nodes must be allocated before submission, so that readback destructors
	/// cannot fail due to allocation.
	///
	/// \param fence
	/// 	The node holding the fence to retire; ignored if null or holding a null fence.
	void retireFence (std::unique_ptr<RetiredFence> fence);

	/// \brief
	/// 	Poll and release the fences of abandoned readbacks.
	///
	/// Called when creating new readbacks, on explicit idle waits, and on device destruction.
	///
	/// \param idle
	/// 	\c true only after a successful device idle wait, releasing all fences; otherwise each fence is queried
	/// 	individually.
	void collectRetiredFences (bool idle=false) const;


	////
	// Fields

	/// The SDL GPU device handle.
	SDL_GPUDevice *m_handle = nullptr;

	/// List of currently claimed windows.
	std::set<Window*> m_claimedWindows;

	/// Protects the retirement list when independent readbacks are managed from different threads.
	mutable std::mutex m_fenceMutex;

	/// Fences of abandoned readbacks; keeps SDL from recycling a fence before its submission has completed.
	mutable std::unique_ptr<RetiredFence> m_retiredFences;
};



/// @}



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_DEVICE_H__
