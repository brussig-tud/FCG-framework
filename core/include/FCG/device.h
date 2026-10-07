/**
 * \defgroup fcg_devices Devices and shaders
 * \ingroup fcg_components
 *
 * <code>\ref fcg::Device</code> provides access to the GPU device and shader creation.
 * <code>\ref fcg::ShaderStage</code> identifies shader stages, and <code>\ref fcg::ShaderResources</code> describes
 * graphics shader resource counts.
 *
 * \par Guide incomplete
 * This guide is a stub. Consult the API declarations below for currently documented behavior.
 *
 * \section fcg_devices_workflows Common workflows
 * Guide incomplete: workflow descriptions remain to be investigated and written.
 *
 * \section fcg_devices_lifetime Ownership and lifetime
 * Guide incomplete: consult individual type and member contracts.
 *
 * \section fcg_devices_errors Errors
 * Guide incomplete: error handling remains to be investigated and written.
 *
 * \section fcg_devices_examples Examples
 * Guide incomplete: worked examples remain to be added and compiled.
 *
 * \see \ref fcg_buffers, \ref fcg_resources, \ref fcg_windows
 */


#ifndef __FCG_DEVICE_H__
#define __FCG_DEVICE_H__


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

/** \addtogroup fcg_devices
 * @{
 */



//////
//
// Structs & enums
//

/// Indicate one of SDL3 GPU's supported shader stages.
enum class ShaderStage {
	VERTEX, FRAGMENT, COMPUTE
};



/// Resource counts declared by one graphics shader stage.
///
/// Counts must match the shader's binding layout on every backend. Storage buffers use `std430`; uniform blocks
/// use `std140`. SDL orders samplers before storage textures before storage buffers in each stage's resource set.
/// See \ref fcg_buffers for a storage-shader example. All counts default to zero.
struct ShaderResources {
	unsigned uniformBuffers = 0; ///< Number of uniform blocks (at most four per stage).
	unsigned storageBuffers = 0; ///< Number of read-only graphics storage buffers.
	unsigned storageTextures = 0; ///< Number of read-only graphics storage textures.
	unsigned samplers = 0; ///< Number of texture/sampler pairs.
};



//////
//
// Classes
//

/// An SDL GPU device that can be shared by all windows and applets.
///
/// This class owns the device. It is created with all common shader formats enabled, so that SDL picks the most
/// appropriate backend (Vulkan, Direct3D 12, Metal) for the current platform. Since all rendering in the framework
/// goes through this single device, GPU resources (textures, buffers, pipelines) can be freely shared between all
/// windows – e.g. an applet can render into a texture in its own window and use that texture while drawing into the
/// main window.
///
/// The device must outlive all \c Window instances that it was used to render into.
class FCG_FRAMEWORK_EXPORT Device
{
	////
	// Friend declarations

	// Buffers need access to the private RetiredFence type.
	friend class Buffer;

	// BufferReadbacks need access to the private RetiredFence type.
	friend class BufferReadback;

	// The player needs to manage this device abstraction.
	friend class Player;


	////
	// Types

	/// Preallocated retirement node: abandoning a pending readback must neither allocate nor block.
	struct RetiredFence {
		/// Fence held until its submission completes.
		SDL_GPUFence *handle = nullptr;

		/// Next deferred fence; unlinked iteratively during collection.
		std::unique_ptr<RetiredFence> next;
	};

	/// Zero-overhead key to access our pseudo-private constructors. Pseudo-private because we don't want them used
	/// outside our own internals, but they have to be public because otherwise they can't be used by STL functions
	/// which we use internally (like \c std::make_optional). WHY C++??? WHYYYYYYY??????!?!?!!!11
	class PrivateConstructorKey final {
		/// `Device` needs to construct the key inside <code>\ref Device::create</code>.
		friend Device;

		/// Private default constructor. Statically no-op and thus zero-overhead.
		constexpr PrivateConstructorKey() noexcept = default;
	};


public:

	////
	// Object construction/destruction

	/// Construct wrapping the given SDL GPU device handle (pseudo-private, for internal use only).
	///
	/// \param key Internal construction permission. \param handle Owned SDL device handle.
	explicit Device([[maybe_unused]] PrivateConstructorKey key, SDL_GPUDevice *handle)
		: m_handle(handle)
	{}

	/// Create the shared GPU device.
	/// \returns The device, or `std::nullopt` if GPU device creation failed.
	[[nodiscard]] static auto create () -> std::optional<Device>;

	/// The destructor. Waits for the GPU to finish all pending work, then destroys the device.
	~Device();

	/// \c Device is not copyable.
	Device(const Device&) = delete;

	/// \c Device is not copy-assignable.
	auto operator= (const Device&) -> Device& = delete;


	////
	// Accessors

	/// \returns The borrowed SDL GPU device handle, valid until this device is destroyed.
	[[nodiscard]] inline auto handle () const -> SDL_GPUDevice* {
		return m_handle;
	}


	////
	// Methods

	/// Wait until the GPU has finished all pending work.
	void waitIdle () const;

	/// Claim the given window for this device.
	///
	/// \param window The window to claim. Must outlive the device.
	///
	/// \returns `true` if the window was successfully claimed, `false` if the claim failed because of some runtime
	/// error (typically inside SDL).
	[[nodiscard]] auto claimWindow (std::unique_ptr<Window> &window) -> bool;

	/// Remove claim to the given window for this device.
	///
	/// \param window The window to remove the claim for. Must have been previously claimed by this device.
	void unclaimWindow (std::unique_ptr<Window> &window);

	/// Create a GPU shader for this device from embedded SPIR-V bytecode.
	///
	/// On Vulkan backends, the SPIR-V is used directly. On all other backends (Metal, Direct3D 12), it is
	/// translated to the backend's shader format at runtime via SDL_shadercross.
	///
	/// \param stage The shader stage. Compute shaders are not supported by this method yet.
	/// \param spirv The SPIR-V bytecode.
	/// \param numUniformBlocks The number of uniform blocks used by the shader.
	/// \param entrypoint The shader entry point. Defaults to \c main, which is what the build system's shader
	///                   compilation produces.
	///
	/// \returns The shader, or \c nullptr on failure (details are written to the SDL error log). The caller
	///          owns the returned handle and must release it with \c SDL_ReleaseGPUShader.
	[[nodiscard]] auto createShader (
		ShaderStage stage, std::span<const std::byte> spirv, unsigned numUniformBlocks,
		std::string_view entrypoint="main"
	) const -> SDL_GPUShader*;


	/// Create a graphics shader with explicit resource counts, including storage buffers.
	///
	/// Uses the same resource declaration for native SPIR-V and runtime shadercross translation. Existing pipelines
	/// retain their own shader references; release the returned shader when pipeline construction is complete.
	/// \param stage `VERTEX` or `FRAGMENT`; compute pipelines are created through SDL directly.
	/// \param spirv Nonempty embedded SPIR-V bytecode, borrowed during the call.
	/// \param resources Counts matching the shader's resource declarations and SDL binding conventions.
	/// \param entrypoint Shader entry point; defaults to \c main.
	/// \returns A caller-owned shader or `nullptr` on failure, with details logged through SDL. Does not submit or wait.
	[[nodiscard]] auto createShader (
		ShaderStage stage, std::span<const std::byte> spirv, const ShaderResources &resources,
		std::string_view entrypoint="main"
	) const -> SDL_GPUShader*;


private:

	////
	// Methods

	/// Release a completed fence or retain it until a later collection, without waiting or allocating.
	/// Nodes must be allocated before submission so readback destructors cannot fail due to allocation.
	void retireFence (std::unique_ptr<RetiredFence> fence);

	/// Poll and release abandoned fences. Called during new readbacks, explicit idle waits, and device teardown.
	/// \param idle `true` only after a successful device idle wait; otherwise individually query each fence.
	void collectRetiredFences (bool idle=false) const;


	////
	// Fields

	/// The SDL GPU device handle.
	SDL_GPUDevice *m_handle = nullptr;

	/// List of currently claimed windows.
	std::set<Window*> m_claimedWindows;

	/// Protect the retirement list when independent readbacks are managed from different threads.
	mutable std::mutex m_fenceMutex;

	/// Abandoned fences; keeps SDL from recycling a fence before its submission has completed.
	mutable std::unique_ptr<RetiredFence> m_retiredFences;
};



/** @} */

//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_DEVICE_H__
