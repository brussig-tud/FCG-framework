
#ifndef __FCG_RENDER_PRIMITIVE_CONNECTIVITY_H__
#define __FCG_RENDER_PRIMITIVE_CONNECTIVITY_H__


//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

// FCG Framework
#include <FCG/buffer.h>
#include <FCG/Render/error.h>
#include <FCG/Render/export.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_connected_primitives
/// @{



//////
//
// Structs and enums
//

/// \brief A borrowed contiguous array of 32-bit node indices in a GPU buffer.
///
/// The buffer must have graphics-storage-read usage on the connectivity collection's device. The offset must be
/// divisible by four, and all indices must fit in the buffer. The offset, count and byte addresses must fit SDL's
/// 32-bit limits. Keep the buffer alive and unmoved until the view is replaced or its collection is destroyed.
struct ConnectivityBufferView
{
	/// The buffer containing the indices; not owned and must be live when binding a view.
	const Buffer *buffer = nullptr;

	/// Byte offset of the first index, divisible by four.
	std::size_t offset = 0;

	/// Number of consecutive 32-bit indices.
	std::size_t count = 0;
};



//////
//
// Classes
//

/// \brief GPU node indices shared by connected primitive renderers.
///
/// A renderer groups successive indices according to \c ConnectedPrimitiveRenderer::nodesPerPrimitive. For lines, the
/// sequence (0,1,1,2) draws two independent segments sharing node 1. The collection owns uploaded arrays and borrows
/// buffers supplied through \c bind. Operations never wait for GPU idle.
///
/// The device must outlive this object. Use the collection from one thread at a time, without concurrent draws
/// or updates. All input preconditions are unchecked; violating them results in undefined behavior.
class FCG_RENDER_EXPORT PrimitiveConnectivity
{
public:

	////
	// Object construction/destruction

	/// \brief Construct an empty connectivity collection.
	///
	/// \pre \p device is live and outlives the collection.
	///
	/// \param device The device that owns uploaded connectivity buffers.
	explicit PrimitiveConnectivity(Device &device);

	/// Release owned resources without waiting for GPU idle; submitted draws still complete.
	~PrimitiveConnectivity();

	/// Not copyable.
	PrimitiveConnectivity(const PrimitiveConnectivity&) = delete;

	/// Not copy-assignable.
	auto operator= (const PrimitiveConnectivity&) -> PrimitiveConnectivity& = delete;

	/// \brief Transfer the collection; existing source views remain valid under the destination's ownership.
	///
	/// The source subsequently permits only assignment and destruction.
	///
	/// \param other The collection to move.
	PrimitiveConnectivity(PrimitiveConnectivity &&other) noexcept;

	/// \brief Release this collection and transfer another collection, preserving its source views.
	///
	/// The source subsequently permits only assignment and destruction.
	///
	/// \param other The collection to move.
	///
	/// \return This collection.
	auto operator= (PrimitiveConnectivity &&other) noexcept -> PrimitiveConnectivity&;


	////
	// Accessors

	/// The device on which the collection was created.
	[[nodiscard]] auto device () const -> Device&;

	/// Number of node indices, rather than connected primitives.
	[[nodiscard]] auto indexCount () const -> std::size_t;

	/// \brief The current GPU index view, or an empty view for an empty collection.
	///
	/// An uploaded array's view borrows collection-owned storage and remains valid until the next successful change
	/// of source or destruction of its owning collection. Moving transfers that ownership to the destination.
	///
	/// \return The current index view.
	[[nodiscard]] auto source () const -> ConnectivityBufferView;

	/// Whether the source can be drawn; false after a failed automatic upload submission until replaced successfully.
	[[nodiscard]] auto usable () const -> bool;


	////
	// Methods

	/// \brief Copy an index array to GPU storage in one automatic submission.
	///
	/// Empty arrays clear the collection. Pre-recording failures preserve the previous source. Failed submission
	/// preserves its descriptor and marks it unusable until a successful assignment or \c clear.
	///
	/// \pre
	/// 	The device is live. The span remains valid and unchanged until return, and its byte size fits SDL's
	/// 	32-bit allocation limits.
	///
	/// \param indices The consecutive node indices to copy.
	///
	/// \return Success, or \c RenderErrorCode::SDLFailure if allocation, staging or submission fails.
	[[nodiscard]] auto setIndices (std::span<const std::uint32_t> indices) -> std::expected<void, RenderError>;

	/// \brief Copy an index array to GPU storage using a caller-provided copy pass.
	///
	/// The source changes when recording succeeds. Draws must follow the end of the copy pass in this command
	/// buffer or a later submitted command buffer.
	///
	/// \pre
	/// 	The span and size meet the automatic overload's requirements. The nonnull copy pass is active on the
	/// 	collection's device and is used on its owning thread.
	///
	/// \note
	/// 	If recording partly fails, the command buffer is canceled or submission fails, replace affected arrays
	/// 	successfully before drawing them.
	///
	/// \param copyPass The active copy pass into which uploads are recorded.
	/// \param indices The consecutive node indices to copy.
	///
	/// \return Success, or \c RenderErrorCode::SDLFailure if allocation or staging fails.
	[[nodiscard]] auto setIndices (SDL_GPUCopyPass *copyPass, std::span<const std::uint32_t> indices)
		-> std::expected<void, RenderError>;

	/// \brief Use a caller-owned GPU index array without copying it.
	///
	/// Each draw reads the buffer's current contents.
	///
	/// \pre \p view meets \c ConnectivityBufferView requirements, including device, layout and lifetime.
	///
	/// \param view The borrowed GPU index array.
	void bind (ConnectivityBufferView view);

	/// Clear the source and make the empty collection usable.
	void clear ();


private:

	////
	// Types

	/// Owned storage, current source and reusable upload staging.
	struct State;


	////
	// Methods

	/// \brief Stage indices, then record them or submit them automatically.
	///
	/// \param pass The copy pass, or null to submit automatically.
	/// \param indices The indices satisfying the public upload preconditions.
	///
	/// \return Success or an operational upload error.
	[[nodiscard]] auto upload (SDL_GPUCopyPass *pass, std::span<const std::uint32_t> indices)
		-> std::expected<void, RenderError>;


	////
	// Fields

	/// Storage and source state, retained across updates; null after moving.
	std::unique_ptr<State> state;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // ifndef __FCG_RENDER_PRIMITIVE_CONNECTIVITY_H__
