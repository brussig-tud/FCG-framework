
#ifndef __FCG_PRESENTATION_H__
#define __FCG_PRESENTATION_H__


//////
//
// Includes
//

// C++ STL
#include <concepts>
#include <expected>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

// Framework
#include <FCG/fullscreen.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_runtime
/// @{



//////
//
// Structs and enums
//

/// \brief Borrowed resources for recording the main window's presentation.
///
/// All resources are valid only during the callback invocation. Obtain scene and window-image handles anew on each
/// invocation, including after a resize; never retain them across frames.
struct PresentationContext
{
	/// The current scene texture, whose sampled colors are linear. Borrowed for this invocation only.
	const Texture &scene;

	/// The default scene sampler. Borrowed for this invocation only.
	const Sampler &sceneSampler;

	/// The command buffer to record into. Borrowed for this invocation; leave submission and cancellation to the caller.
	SDL_GPUCommandBuffer *commandBuffer;

	/// The borrowed window image with its actual format, device and extent in pixels, discard/load and store operations.
	/// Its texture handle is valid only for this invocation.
	FullscreenTarget target;
};



//////
//
// Classes
//

/// \brief An owning, move-only presentation callable, supporting mutable and non-copyable captures.
///
/// The callable accepts <tt>const PresentationContext &</tt> and returns <tt>std::expected<void, FullscreenError></tt>.
/// Owned captures live until replacement or destruction. Captured references must remain valid while the callback is
/// owned. Default-constructed and moved-from callbacks are empty and may be assigned or destroyed.
/// Each invocation must finish every GPU pass, even on error or exception, and write display-encoded window colors.
/// Leave frame completion and acquisition to the caller.
class PresentationCallback
{
public:
	////
	// Object construction/destruction

	/// Create an empty callback.
	PresentationCallback() noexcept = default;

	/// \brief Own a callable by moving an rvalue or copying a copyable lvalue; mutable invocation is supported.
	///
	/// \tparam Callable A callable with the presentation signature, constructible from the supplied argument.
	///
	/// \param callable The callable to own. Referenced objects are not owned and must outlive its use.
	///
	/// \throws std::bad_alloc If allocating callback ownership fails.
	/// \throws ... Any exception from constructing the owned callable propagates unchanged.
	template <typename Callable>
		requires (!std::same_as<std::remove_cvref_t<Callable>, PresentationCallback>)
			&& std::constructible_from<std::decay_t<Callable>, Callable>
			&& requires (std::decay_t<Callable> &callable, const PresentationContext &context) {
				{ std::invoke (callable, context) } -> std::same_as<std::expected<void, FullscreenError>>;
			}
	PresentationCallback(Callable &&callable)
		: m_callable(std::make_unique<Model<std::decay_t<Callable>>> (std::forward<Callable> (callable))) {}

	/// Destroy the callback and its owned captures.
	~PresentationCallback() = default;

	/// \brief Copy construction is not supported.
	///
	/// \param other The callback that cannot be copied.
	PresentationCallback(const PresentationCallback &other) = delete;

	/// \brief Copy assignment is not supported.
	///
	/// \param other The callback that cannot be copied.
	///
	/// \return Assignment is unavailable.
	auto operator = (const PresentationCallback &other) -> PresentationCallback & = delete;

	/// \brief Transfer ownership, leaving the source empty.
	///
	/// \param other The callback to move from.
	PresentationCallback(PresentationCallback &&other) noexcept = default;

	/// \brief Destroy the previous callable and transfer ownership, leaving a distinct source empty.
	///
	/// \param other The callback to move from. Self-assignment preserves ownership.
	///
	/// \return This callback.
	auto operator = (PresentationCallback &&other) noexcept -> PresentationCallback & = default;


	////
	// Accessors

	/// Return whether a callable is owned; empty and moved-from callbacks return false.
	[[nodiscard]] explicit operator bool () const noexcept {
		return bool(m_callable);
	}


	////
	// Methods

	/// \brief Invoke the owned callable, allowing it to modify its captures.
	///
	/// \pre Context resources remain valid throughout the call. Do not replace or destroy this callback during invocation.
	///
	/// \param context Borrowed recording resources; neither the context nor its resources may be retained.
	///
	/// \return The callable's result unchanged, including any reported fullscreen error.
	///
	/// \throws std::bad_function_call If this callback is empty.
	/// \throws ... Any exception thrown by the callable propagates unchanged.
	[[nodiscard]] auto operator () (const PresentationContext &context) -> std::expected<void, FullscreenError> {
		if (!m_callable)
			throw std::bad_function_call();
		return m_callable->invoke (context);
	}


private:

	////
	// Types

	/// Virtual invocation and destruction interface for the owned callable.
	class Interface
	{
	public:
		////
		// Object construction/destruction

		/// Destroy the concrete callable through its erased interface.
		virtual ~Interface() = default;


		////
		// Methods

		/// Invoke the concrete callable with borrowed resources and propagate its result or exception.
		virtual auto invoke (const PresentationContext &context) -> std::expected<void, FullscreenError> = 0;
	};

	/// \brief Store and invoke one concrete callable type.
	///
	/// \tparam Callable The decayed type of the owned callable.
	template <typename Callable>
	class Model final : public Interface
	{
	public:
		////
		// Object construction

		/// \brief Forward construction to the stored callable, propagating construction exceptions.
		///
		/// \tparam Source The forwarded constructor argument type.
		///
		/// \param callable The callable to copy or move into storage.
		template <typename Source>
		Model(Source &&callable) : m_value(std::forward<Source> (callable)) {}


		////
		// Interface: Interface

		/// Invoke the mutable stored callable with the borrowed context.
		auto invoke (const PresentationContext &context) -> std::expected<void, FullscreenError> override {
			return std::invoke (m_value, context);
		}


	private:

		////
		// Fields

		/// The concrete callable and its captures, owned until model destruction.
		Callable m_value;
	};


	////
	// Fields

	/// The uniquely owned erased callable; null represents an empty callback.
	std::unique_ptr<Interface> m_callable;
};



//////
//
// Namespaces close
//

/// @}

} // namespace fcg


#endif // __FCG_PRESENTATION_H__