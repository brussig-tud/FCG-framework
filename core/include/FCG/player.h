
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
#include <optional>
#include <string>
#include <utility>

// GLM library
#include <glm/glm.hpp>

// Local includes
#include "FCG/export.h"
#include "FCG/run.h"
#include "FCG/applet.h"
#include "FCG/util.h"
#include "FCG/buffer.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
struct SDL_GPUFence;

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

/** \addtogroup fcg_runtime
 * @{
 */



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

/// Texture interpretation of a scoped transfer mapping.
///
/// Owns only the mapping, not its TransferBuffer. The transfer buffer must remain alive and unmoved until this
/// view is destroyed. Moving the view transfers mapping ownership; borrowed TextureViews expire on unmapping.
/// \todo Once we have proper texture facilities, move this there.
template <class Texel, unsigned Dims=2>
	requires (sizeof(Texel) > 0 && Dims >= 1 && Dims <= 3)
class OwningTextureView
{
public:

	////
	// Object construction/destruction

	/// Interpret an already successful mapping using caller-supplied texel geometry.
	/// \param mapping Completed GPU download mapping, moved into this object.
	/// \param extent Dimensions in texels. \param stride Per-axis strides in texels, not bytes.
	/// \pre The mapped allocation covers this geometry and is suitably aligned for Texel.
	OwningTextureView (
		TransferBuffer::Mapping &&mapping, const glm::vec<Dims, unsigned> &extent,
		const glm::vec<Dims, unsigned> &stride
	)
		: mapping(std::move(mapping)), extent(extent), stride(stride)
	{}

	/// Exactly one owner unmaps the storage; views cannot be copied.
	OwningTextureView (const OwningTextureView&) = delete;
	/// Owning views cannot be copy-assigned.
	auto operator= (const OwningTextureView&) -> OwningTextureView& = delete;
	/// Transfer the mapping; previously borrowed views remain valid until the new owner releases it.
	OwningTextureView (OwningTextureView&&) noexcept = default;
	/// Unmap the previous storage and take another mapping. Invalidates views into the previous storage.
	auto operator= (OwningTextureView&&) noexcept -> OwningTextureView& = default;
	/// Unmap the transfer storage, invalidating all derived TextureViews, without waiting on the GPU.
	~OwningTextureView () = default;


	////
	// Accessors

	/// Borrow a texture interpretation. Valid only while this object's mapping remains active.
	[[nodiscard]] operator TextureView<Texel, Dims> () {
		return {reinterpret_cast<Texel*>(mapping.data().data()), extent, stride};
	}

	/// Borrow a read-only interpretation, whose lifetime is bounded by this object's mapping.
	[[nodiscard]] operator TextureView<const Texel, Dims> () const {
		return {reinterpret_cast<const Texel*>(mapping.data().data()), extent, stride};
	}


private:

	////
	// Fields

	TransferBuffer::Mapping mapping; ///< Scoped CPU access; the transfer allocation is owned by Player.
	glm::vec<Dims, unsigned> extent; ///< Logical texture dimensions in texels.
	glm::vec<Dims, unsigned> stride; ///< Memory strides in texels.
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

	/// The state of a readback operation that has been dispatched but whose GPU fence has not yet been
	/// signaled, i.e. the readback is still in flight.
	struct PendingReadback
	{
		~PendingReadback();

		/// A pending readback own GPU resource (a fence) and therefore cannot be copied.
		PendingReadback (const PendingReadback&) = delete;
		auto operator= (const PendingReadback&) -> PendingReadback& = delete;

		/// The move constructor.
		PendingReadback (PendingReadback &&other) noexcept
			: device(other.device), fence(std::move(other.fence)),
			  extent(other.extent), stride(other.stride), token(other.token)
		{}

		/// Construct from the dispatch site's knowns.
		PendingReadback (
			Device &device, std::unique_ptr<Device::RetiredFence> fence, const glm::uvec2 &extent,
			const glm::uvec2 &stride, uint64_t token
		)
			: device(device), fence(std::move(fence)), extent(extent), stride(stride), token(token)
		{}

		/// The device the readback operation was dispatched on. Needed to release \ref fence.
		Device &device;

		/// The fence guarding the readback copy, or \c nullptr once it has been waited on and released.
		std::unique_ptr<Device::RetiredFence> fence;

		/// The texture dimensions of the targeted texture.
		glm::uvec2 extent;

		/// The per-dimension strides of the targeted texture.
		glm::uvec2 stride;

		/// The token of the readback operation, as returned by \ref scheduleDepthReadback.
		uint64_t token = -1;
	};

	/// The state of a readback operation whose result has been mapped and is ready to be queried.
	template <class Texel>
	struct ReadyReadback {
		/// The mapped readback view.
		OwningTextureView<Texel, 2> view;

		/// The token of the readback operation, as returned by \ref scheduleDepthReadback.
		uint64_t token = -1;
	};

	/// Event: a new frame has begun, i.e. any in-flight readback results should be collected.
	struct FrameBegin {};

	/// Event: \ref scheduleDepthReadback was invoked and a new readback should be dispatched.
	struct ScheduleReadback {};

	/// Event: a readback result was queried via \ref getDepthReadbackResult.
	struct QueryReadback {
		/// The token of the readback operation whose result is being queried.
		uint64_t token;
	};

	/// The controller bundling all depth buffer readback state machine logic.
	template <class Texel>
	struct ReadbackController
	{
		using StateMachine = fcg::StateMachine<
			ReadbackController, std::monostate, PendingReadback, ReadyReadback<Texel>
		>;
		ReadbackController(Player &p) : fsm(*this), player(p) {}

		/// Handle a readback being scheduled.
		inline void on (const std::monostate&, const ScheduleReadback &event, StateMachine &fsm);
		inline void on (PendingReadback &curState, const ScheduleReadback &event, StateMachine &fsm);
		inline void on (ReadyReadback<Texel> &curState, const ScheduleReadback &event, StateMachine &fsm);

		/// Handle a readback result being queried.
		inline void on (const std::monostate&, const QueryReadback &event, StateMachine &fsm);
		inline void on (PendingReadback &curState, const QueryReadback &event, StateMachine &fsm);
		inline void on (ReadyReadback<Texel> &curState, const QueryReadback &event, StateMachine &fsm);

		/// Handle a new frame starting, collecting in-flight readback results.
		inline void on (PendingReadback &curState, const FrameBegin &event, StateMachine &fsm);

		/// The state machine itself.
		StateMachine fsm;

		/// The player whose readback machinery we drive.
		Player &player;

	private:
		/// Dispatch a new depth buffer readback and return the state tracking the submitted fence.
		auto dispatch () -> PendingReadback;

		/// Wait for the GPU to finish a pending readback copy, release its fence and map the results.
		auto completeReadback (PendingReadback &pending) -> OwningTextureView<Texel, 2>;
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

	/// Manage the readback buffers.
	void recreateReadbackBuffers ();

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

	/// Depth-download storage, absent before viewport initialization or after allocation failure.
	/// The readback state must release its mapping before this optional is reset or replaced.
	std::optional<TransferBuffer> depthReadbackBuffer;

	/// The controller handling the depth buffer readback state machine.
	ReadbackController<float> depthReadback{*this};

	/// The current frame's readback token.
	uint64_t readbackToken = 0;
};



/** @} */

//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_PLAYER_H__
