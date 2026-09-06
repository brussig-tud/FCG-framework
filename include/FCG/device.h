
#ifndef __FCG_DEVICE_H__
#define __FCG_DEVICE_H__


//////
//
// Includes
//

// C++ STL
#include <memory>
#include <span>
#include <set>
#include <optional>

// Local includes
#include "FCG/export.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPUDevice;
struct SDL_GPUShader;

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



//////
//
// Structs & enums
//

/// Indicate one of SDL3 GPU's supported shader stages.
enum class ShaderStage {
	VERTEX, FRAGMENT, COMPUTE
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
/// The device must outlive all \ref Window instances that it was used to render into.
class FCG_FRAMEWORK_EXPORT Device
{
	/// Zero-overhead key to access our pseudo-private constructors. Pseudo-private because we don't want them used
	/// outside our own internals, but they have to be public because otherwise they can't be used by STL functions
	/// which we use internally (like \c std::make_optional). WHY C++??? WHYYYYYYY??????!?!?!!!11
	class PrivateConstructorKey final {
		friend Device;
		constexpr PrivateConstructorKey() noexcept = default;
	};


public:

	////
	// Object construction/destruction

	/// Construct wrapping the given SDL GPU device handle (pseudo-private, for internal use only)
	explicit Device(PrivateConstructorKey, SDL_GPUDevice *handle)
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

	/// The raw SDL GPU device handle.
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
	/// \returns The shader, or \c nullptr on failure (details are written to the SDL error log). The device
	///          retains ownership.
	[[nodiscard]] auto createShader (
		ShaderStage stage, std::span<const std::byte> spirv, unsigned numUniformBlocks,
		std::string_view entrypoint="main"
	) const -> SDL_GPUShader*;


private:

	////
	// Fields

	/// The SDL GPU device handle.
	SDL_GPUDevice *m_handle = nullptr;

	/// List of currently claimed windows.
	std::set<Window*> m_claimedWindows;
};



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_DEVICE_H__
