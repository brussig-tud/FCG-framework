
#ifndef __FCG_PLAYER_H__
#define __FCG_PLAYER_H__


//////
//
// Includes
//

// C++ STL
#include <vector>
#include <span>
#include <atomic>
#include <string>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/run.h"
#include "FCG/applet.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPUFence;
struct SDL_GPUTransferBuffer;

// Required SDL3 function prototypes
extern void* SDL_MapGPUTransferBuffer(SDL_GPUDevice*, SDL_GPUTransferBuffer*, bool);
extern void SDL_UnmapGPUTransferBuffer(SDL_GPUDevice*, SDL_GPUTransferBuffer*);

// Framework types
namespace fcg {
	class Window;
	class Frame;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Classes
//

/// Represents a texture view onto a GPU readback buffer.
///
///	\todo Once we have proper texture facilities, move this there.
template <class Texel, unsigned Dims=2>
	requires (sizeof(Texel) > 0 && Dims >= 1 && Dims <= 3)
class TextureView
{
	Texel *texels;
	glm::vec<Dims, unsigned> m_extent;
	glm::vec<Dims, unsigned> m_stride;

public:
	TextureView (Texel *texels, const glm::vec<Dims, unsigned> &extent, const glm::vec<Dims, unsigned> &stride)
		: texels(texels), m_extent(extent), m_stride(stride)
	{}

	[[nodiscard]] inline static constexpr auto coordsInBounds (
		const glm::vec<Dims, unsigned> &coords, const glm::vec<Dims, unsigned> &extent
	) -> bool {
		for (unsigned i=0; i<Dims; ++i)
			if (coords[i] >= extent[i])
				return false;
		return true;
	}

	[[nodiscard]] inline static constexpr auto offset (
		const glm::vec<Dims, unsigned> &coords, const glm::vec<Dims, unsigned> &stride
	) -> size_t {
		size_t offset = 0;
		for (unsigned i=0; i<Dims; ++i) // this should get unrolled by the compiler and end up being very cheap
			offset += coords[i] * stride[i];
		return offset;
	}

	[[nodiscard]] inline static constexpr auto dims () -> unsigned {
		return Dims;
	}

	[[nodiscard]] inline auto extent () const -> glm::vec<Dims, unsigned> {
		return m_extent;
	}

	[[nodiscard]] inline auto stride () const -> glm::vec<Dims, unsigned> {
		return m_stride;
	}

	[[nodiscard]] inline auto texel (const glm::vec<Dims, unsigned> &coords) -> Texel& {
		assert(coordsInBounds(coords, m_extent));
		return texels[offset(coords, m_stride)];
	}

	[[nodiscard]] inline auto texel (const glm::vec<Dims, unsigned> &coords) const -> const Texel& {
		assert(coordsInBounds(coords, m_extent));
		return texels[offset(coords, m_stride)];
	}

	[[nodiscard]] inline auto data () -> std::span<Texel> {
		return texels;
	}

	[[nodiscard]] inline auto data () const -> std::span<const Texel> {
		return texels;
	}
};

///	\todo Once we have proper texture facilities, move this there.
template <class Texel, unsigned Dims=2>
	requires (sizeof(Texel) > 0 && Dims >= 1 && Dims <= 3)
class OwningTextureView
{
	TextureView<Texel, Dims> view;
	Device &device;
	SDL_GPUTransferBuffer *buffer;
public:
	OwningTextureView (
		Device &device, SDL_GPUTransferBuffer *buffer, const glm::vec<Dims, unsigned> &extent,
		const glm::vec<Dims, unsigned> &stride
	)
		: view((Texel*)SDL_MapGPUTransferBuffer(device.handle(), buffer, false), extent, stride),
		  device(device), buffer(buffer)
	{}

	~OwningTextureView () {
		SDL_UnmapGPUTransferBuffer(device.handle(), buffer);
	}

	[[nodiscard]] inline operator TextureView<Texel, Dims> () {
		return view;
	}

	[[nodiscard]] inline operator TextureView<const Texel, Dims> () const {
		return view;
	}
};

/// The central state of the \ref fcg::run main loop.
///
/// An instance of this class is owned by \ref fcg::run and passed to the endpoints of every running \ref Applet,
/// providing them with a way to interact with the main loop and other global application state.
class FCG_FRAMEWORK_EXPORT Player
{
	////
	// Friend declarations

	/// The main loop needs to manipulate the player.
	friend auto fcg::run (std::vector<std::unique_ptr<Applet>>, PlayerSettings&&) -> int;


	////
	// Types

	/// State of a single readback operation.
	template <class Texel> requires (sizeof(Texel) > 0)
	struct ReadbackState
	{
		~ReadbackState();

		/// Transition to the mapped state, potentially blocking until the GPU fence is signaled.
		void transitionToMapped (SDL_GPUTransferBuffer *buffer);

		/// The device the readback operation was dispatched on.
		Device &device;

		/// The texture dimensions of the targeted texture.
		glm::uvec2 extent;

		/// The per-dimension strides of the targeted texture.
		glm::uvec2 stride;

		/// The readback state and its associated data.
		std::variant<SDL_GPUFence*, OwningTextureView<Texel, 2>> state;

		/// The token of the readback operation, as returned by \ref dispatchDepthReadback.
		uint64_t token = -1;
	};


public:

	////
	// Object construction/destruction

	/// Create a player using the given device, connected to the given main window. Both device and window are only
	/// referenced, not owned, and must outlive the player.
	explicit Player (Device &device, Window *mainWindow);

	/// The destructor.
	~Player();

	/// Players are not copyable.
	Player(const Player&) = delete;

	/// Players are not copy-assignable.
	auto operator= (const Player&) -> Player& = delete;


	////
	// Methods

	/// Set the title of the main window. Since the main window is shared by all applets, the most
	/// recently set title wins. Note that SDL requires window titles to be set on the main thread,
	/// which all applet endpoints run on.
	///
	/// \param title The new window title.
	void setWindowTitle (const std::string &title);

	/// Push a continuous redraw request. As long as at least one such request exists, the main loop runs continuously,
	/// i.e. another iteration is started as soon as possible after the current one, instead of blocking while waiting
	/// for events. This is useful e.g. for applets that are animating something. Every push must be balanced by a call
	/// to \ref popContinuousRedraw once continuous redrawing is no longer needed.
	void pushContinuousRedraw ();

	/// Pop a continuous redraw request previously registered via \ref pushContinuousRedraw.
	void popContinuousRedraw ();

	/// Whether at least one continuous redraw request currently exists.
	[[nodiscard]] auto continuousRedrawRequested () const -> bool;

	/// Request the main loop to shut down. For a player connected to a main window this also forwards the request
	/// to that window; for a default-constructed player with no window the request is still recorded.
	void requestClose ();

	/// Check whether closing the application was requested, e.g. by a call to \ref requestClose or by the user
	/// closing the main window.
	[[nodiscard]] auto shouldClose () const -> bool;


	////
	// Accessors

	/// Read-write reference the default main window clear color.
	///
	/// \todo Right now there is no real reason to hide this property behind an accessor. This could change in the
	/// future though in case of multithreading, where we might want to wrap the reference in a scoped lock.
	[[nodiscard]] auto clearColor () -> glm::fvec4& { return m_clearColor; }

	/// Read-only access to the main window clear color.
	///
	/// \todo Right now there is no real reason to hide this property behind an accessor. This could change in the
	/// future though in case of multithreading, where we might want to wrap the reference in a scoped lock.
	[[nodiscard]] auto clearColor () const -> const glm::fvec4& { return m_clearColor; }

	/// The texture format of the main window's swapchain images, as needed for pipeline render targets.
	[[nodiscard]] auto swapchainFormat () const -> SDL_GPUTextureFormat;

	/// Reference the current dimensions of the main window viewport.
	[[nodiscard]] auto viewportSize () const -> glm::uvec2;

	/// Ask for a readback of the main viewport depth buffer.
	///
	/// \note
	/// 	The readback result \em must be queried after being scheduled. Clients have exactly one frame to do so,
	/// 	failure to retrieve the result before it is overwritten is a logic error and can cause a crash.
	///
	/// \return A token that can be used to check for completion of the readback operation and to retrieve the results.
	[[nodiscard]] auto scheduleDepthReadback () -> uint64_t;

	/// Ask for the result of a previously scheduled depth readback operation. Will block if the transfer is still
	/// pending (it is guaranteed to be available at the beginning of the next frame after the one it was requested).
	///
	/// \return A \ref TextureView on the read-back depth buffer.
	[[nodiscard]] auto getDepthReadbackResult (uint64_t token) -> TextureView<float>;


private:

	////
	// Methods

	/// Manage the depth readback buffer.
	void recreateDepthReadbackBuffer ();

	/// Collect the results of any dispatched readback operations.
	void collectReadbackResults ();


	////
	// Member variables

	/// The main rendering device.
	Device &device;

	/// The main window that applets can interact with through the player. Non-owning – the window is owned by whoever
	/// created the \c fcg::Player, (e.g., \ref fcg::run) and must outlive the player.
	Window *m_window = nullptr;

	/// The currently ongoing frame. Non-owning reference, managed externally.
	Frame *frame = nullptr;

	/// The current clear color of the main window viewport.
	glm::fvec4 m_clearColor = { 0.1f, 0.2f, 0.4f, 1.0f };

	/// The number of currently active continuous redraw requests.
	std::atomic<unsigned> m_numContinuousRedrawRequests{0};

	/// Whether closing the application was requested on this player itself.
	std::atomic<bool> m_closeRequested{false};

	/// The buffer used for depth buffer readback operations.
	SDL_GPUTransferBuffer *depthReadbackBuffer = nullptr;

	/// The pending depth readback operation, if any.
	std::optional<ReadbackState<float>> depthReadback;

	/// The current frame's readback token.
	uint64_t readbackToken = 0;
};



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_PLAYER_H__
