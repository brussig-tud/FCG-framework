
#ifndef __FCG_UTIL_H__
#define __FCG_UTIL_H__


//////
//
// Module documentation
//

/// \defgroup fcg_utilities Utilities
/// \ingroup fcg_modules
/// \brief Move-only callable ownership and state machines driven by controller objects.
///
/// Use <tt>\ref fcg::MoveOnlyFunction "MoveOnlyFunction"</tt> to store a callable whose captures may own resources,
/// such as a <tt>std::unique_ptr</tt>. Its function signature specifies the result and argument types; invocation
/// can update captured state. Include <tt>\#include &lt;FCG/util.h&gt;</tt> to use either utility.
///
/// <tt>\ref fcg::StateMachine "StateMachine"</tt> holds exactly one state object at a time, of one of the state types
/// given as template arguments. All behavior lives in a separate controller object: it provides an \c on handler for
/// events (for any subset of the states), and optional \c onEnter and \c onExit hooks. This keeps state-specific data
/// in the state types and transition logic in one place.
///
/// \section fcg_utilities_usage Getting started
/// \subsection fcg_utilities_usage_callables Owning a callable
/// Specify a signature such as <tt>int()</tt> and move the lambda into the wrapper. This counter owns its initial
/// value and keeps a mutable offset between calls:
/// \snippet utility_examples.cpp callable_ownership
///
/// \subsection fcg_utilities_usage_states Defining a state machine
/// Define the state types:
/// \snippet utility_examples.cpp states
/// Define a controller with handlers for the events it cares about:
/// \snippet utility_examples.cpp controller
/// Construct the machine with the controller as its behavior, and run it:
/// \snippet utility_examples.cpp state_machine
///
/// \section fcg_utilities_workflows Common tasks
/// \subsection fcg_utilities_workflows_callables Passing arguments and results
/// <tt>\ref fcg::MoveOnlyFunction "MoveOnlyFunction"</tt> accepts ordinary function signatures. Use reference
/// arguments to borrow existing objects, and value arguments to transfer values, including move-only objects.
/// The callable must return exactly the declared result type, including reference qualifications; a \c void
/// signature requires a \c void result. Callables may be lambdas, function pointers, member pointers, or other
/// invocable objects. Invocation requires a non-const wrapper.
/// \snippet utility_examples.cpp callable_arguments
///
/// \subsection fcg_utilities_workflows_hooks Controller hooks
/// The controller may provide, for any subset of the states:
/// - <tt>void on(State&, const Event&, StateMachine&)</tt> to handle events delivered through
///   <tt>\ref fcg::StateMachine::handle "handle"</tt>, typically calling
///   <tt>\ref fcg::StateMachine::transition "transition"</tt> to change state;
/// - <tt>void onEnter(State&)</tt> and <tt>void onExit(State&)</tt> for entry and exit actions.
///
/// Missing hooks are skipped silently, so a controller only needs handlers for the states it knows.
///
/// \subsection fcg_utilities_workflows_querying Querying the current state
/// <tt>\ref fcg::StateMachine::is "is"</tt> tests for a state type, and <tt>\ref fcg::StateMachine::get "get"</tt>
/// accesses the current state object.
///
/// \section fcg_utilities_lifetime Ownership and lifetime
/// Moving a callable wrapper transfers its captures and leaves the source empty. Assigning \c nullptr clears it;
/// replacement, clearing and destruction release owned captures. Captured references remain borrowed and must
/// outlive their use. Reference results remain valid only while the objects they refer to remain alive.
/// Do not replace or destroy a wrapper while its callable is executing.
///
/// The state machine stores a reference to its controller, which must outlive the machine. Each state object lives
/// until the next transition replaces it; references obtained through <tt>\ref fcg::StateMachine::get "get"</tt> are
/// invalid afterwards.
///
/// \section fcg_utilities_errors Handling errors
/// Test a callable wrapper in a condition before invoking it if it might be empty. Default construction and null
/// function or member pointers produce empty wrappers; empty invocation throws \c std::bad_function_call.
/// Callable construction and invocation exceptions propagate unchanged. If constructing a replacement throws,
/// the previous callable remains owned. Incompatible signatures are rejected at compile time.
///
/// Calling <tt>\ref fcg::StateMachine::transition "transition"</tt> with a state type outside the machine's state
/// types is rejected at compile time. <tt>\ref fcg::StateMachine::get "get"</tt> throws \c std::bad_variant_access if
/// the machine is not currently in the requested state.



//////
//
// Includes
//

// C++ STL
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

// Local includes
#include "FCG/export.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_utilities
/// @{



//////
//
// Classes
//

/// \brief Own a callable with a specified signature, including callables with move-only captures.
///
/// Use an ordinary function signature such as <tt>int(int)</tt> or <tt>void()</tt>. Invocation may modify the owned
/// callable. The result type must match the signature exactly, including reference qualifications; a \c void
/// signature requires a callable returning \c void. Qualified function signatures are not supported.
///
/// Owned captures live until replacement, clearing or destruction. Captured references remain borrowed and must
/// outlive their use. Default construction and construction from \c nullptr produce an empty wrapper. Invoking an
/// empty wrapper throws \c std::bad_function_call. Moving transfers ownership and leaves the source empty.
///
/// See \ref fcg_move_only_function_signature for construction and invocation contracts.
///
/// \tparam Signature An unqualified function type describing the result and arguments.
template <typename Signature>
class MoveOnlyFunction;

/// \anchor fcg_move_only_function_signature
/// \brief Own and invoke a callable returning \c R with arguments of types <tt>Args...</tt>.
///
/// Arguments follow the signature's value and reference semantics. Reference results remain borrowed from the
/// callable; their validity depends on the referenced object's lifetime. Const wrappers cannot be invoked.
///
/// \tparam R The exact result type, which may be \c void or a reference type.
/// \tparam Args The argument types, including any reference qualifications.
template <typename R, typename... Args>
class MoveOnlyFunction<R(Args...)>
{
public:
	////
	// Object construction/destruction

	/// Create an empty wrapper.
	MoveOnlyFunction() noexcept = default;

	/// \brief Create an empty wrapper from a null pointer literal.
	///
	/// \param empty The null pointer literal.
	MoveOnlyFunction([[maybe_unused]] std::nullptr_t empty) noexcept {}

	/// \brief Own a callable by moving an rvalue or copying a copyable lvalue; mutable invocation is supported.
	///
	/// Null function and member pointers produce an empty wrapper. Other callables are stored as supplied, including
	/// wrappers that may themselves be empty. Replacement through assignment constructs the new wrapper first;
	/// construction failure leaves the previous callable intact.
	///
	/// \tparam Callable A callable constructible from the argument and invocable with the exact result type \c R.
	///
	/// \param callable The callable to own. Referenced objects remain borrowed and must outlive their use.
	///
	/// \throws std::bad_alloc If allocating callable ownership fails.
	/// \throws ... Exceptions from constructing the owned callable propagate unchanged.
	template <typename Callable>
		requires (!std::same_as<std::remove_cvref_t<Callable>, MoveOnlyFunction>)
			&& std::constructible_from<std::decay_t<Callable>, Callable>
			&& requires (std::decay_t<Callable> &callable, Args&&... args) {
				{ std::invoke (callable, std::forward<Args> (args)...) } -> std::same_as<R>;
			}
	MoveOnlyFunction(Callable &&callable)
	{
		if constexpr (std::is_pointer_v<std::decay_t<Callable>> || std::is_member_pointer_v<std::decay_t<Callable>>)
			if (callable == nullptr)
				return;
		m_callable = std::make_unique<Model<std::decay_t<Callable>>> (std::forward<Callable> (callable));
	}

	/// Destroy the callable and its owned captures.
	~MoveOnlyFunction() = default;

	/// \brief Copy construction is not supported.
	///
	/// \param other The wrapper that cannot be copied.
	MoveOnlyFunction(const MoveOnlyFunction &other) = delete;

	/// \brief Copy assignment is not supported.
	///
	/// \param other The wrapper that cannot be copied.
	///
	/// \return Assignment is unavailable.
	auto operator = (const MoveOnlyFunction &other) -> MoveOnlyFunction & = delete;

	/// \brief Transfer ownership, leaving the source empty.
	///
	/// \param other The wrapper to move from.
	MoveOnlyFunction(MoveOnlyFunction &&other) noexcept = default;

	/// \brief Destroy the previous callable and transfer ownership, leaving a distinct source empty.
	///
	/// \param other The wrapper to move from. Self-assignment preserves ownership.
	///
	/// \return This wrapper.
	auto operator = (MoveOnlyFunction &&other) noexcept -> MoveOnlyFunction & = default;


	////
	// Accessors

	/// Return whether a callable is owned; empty and moved-from wrappers return \c false.
	[[nodiscard]] explicit operator bool () const noexcept {
		return bool(m_callable);
	}


	////
	// Methods

	/// \brief Invoke the owned callable, allowing it to modify its captures.
	///
	/// \pre Do not replace or destroy this wrapper during invocation. Borrowed objects remain valid during their use.
	///
	/// \param args Arguments passed by value or reference according to the signature.
	///
	/// \return The callable's result unchanged, or nothing for a \c void signature.
	///
	/// \throws std::bad_function_call If this wrapper is empty.
	/// \throws ... Exceptions from the callable or argument transfer propagate unchanged.
	auto operator () (Args... args) -> R {
		if (!m_callable)
			throw std::bad_function_call();
		return m_callable->invoke (std::forward<Args> (args)...);
	}


private:

	////
	// Types

	/// Virtual invocation and destruction interface; reference parameters avoid additional argument copies.
	class Interface
	{
	public:
		////
		// Object construction/destruction

		/// Destroy the concrete callable through its erased interface.
		virtual ~Interface() = default;


		////
		// Methods

		/// Forward arguments according to the public signature and propagate the callable's result or exception.
		virtual auto invoke (Args&&... args) -> R = 0;
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

		/// Invoke the mutable stored callable with the forwarded arguments.
		auto invoke (Args&&... args) -> R override {
			return std::invoke (m_value, std::forward<Args> (args)...);
		}


	private:

		////
		// Fields

		/// The concrete callable and its captures, owned until model destruction.
		Callable m_value;
	};


	////
	// Fields

	/// The uniquely owned erased callable; null represents an empty wrapper.
	std::unique_ptr<Interface> m_callable;
};


/// Auxiliaries of the <tt>\ref fcg::StateMachine "StateMachine"</tt> facility.
namespace fsm {

/// \brief
/// 	The concept of a <tt>\ref fcg::StateMachine "StateMachine"</tt> controller that provides an \c onEnter hook
/// 	for a state.
///
/// \tparam C The controller type.
/// \tparam S The state type.
template<class C, class S>
concept has_onEnter = requires(C c, S& s) { c.onEnter(s); };

/// \brief
/// 	The concept of a <tt>\ref fcg::StateMachine "StateMachine"</tt> controller that provides an \c onExit hook
/// 	for a state.
///
/// \tparam C The controller type.
/// \tparam S The state type.
template<class C, class S>
concept has_onExit = requires(C c, S& s) { c.onExit(s); };

// namespace fsm
}

/// \brief
/// 	A generic finite state machine whose states are types and whose behavior is defined by a controller object.
///
/// The machine holds exactly one state object at a time, of one of the types \p States. The controller may provide
/// any of the following member functions, for all or only some of the states (e.g. as templates or overloads):
/// - <tt>void onEnter(State&)</tt>: called when a state is entered through <tt>\ref transition "transition"</tt>;
/// - <tt>void onExit(State&)</tt>: called when a state is left through <tt>\ref transition "transition"</tt>;
/// - <tt>void on(State&, const Event&, StateMachine&)</tt>: handles an event passed to <tt>\ref handle "handle"</tt>
/// 	in the given state. Guard conditions and transition actions are implemented here, typically by calling
/// 	<tt>\ref transition "transition"</tt> on the given state machine.
///
/// Missing functions are skipped silently. The initial state is set by the constructor; \c onEnter is not called for
/// it.
///
/// \tparam Controller The controller type. The machine stores a reference to the controller.
/// \tparam States The state types. Each type may appear only once.
template<class Controller, class... States>
class StateMachine
{
public:

	////
	// Types

	/// The variant type holding the current state.
	using VariantType = std::variant<States...>;


	////
	// Object construction/destruction

	/// \brief
	/// 	Constructs a state machine with the given controller and initial state. \c onEnter is not called for the
	/// 	initial state.
	///
	/// \param ctrl The controller. It must outlive the state machine.
	/// \param init The initial state. Defaults to a default-constructed object of the first state type.
	explicit StateMachine(Controller& ctrl, VariantType init={})
		: m_controller(ctrl), m_state(std::move(init))
	{}


	////
	// Accessors

	/// \brief
	/// 	Checks whether the machine is currently in the given state.
	///
	/// \tparam S The state type to check for. Must be one of \p States.
	///
	/// \return \c true if the current state is of type \c S.
	template<class S>
		requires (std::disjunction_v<std::is_same<S, States>...>)
	[[nodiscard]] auto is () const -> bool {
		return std::holds_alternative<S>(m_state);
	}

	/// \brief
	/// 	Accesses the current state as the given type.
	///
	/// \tparam S The state type to access. Must be one of \p States.
	///
	/// \return A reference to the current state, valid until the next transition.
	///
	/// \throws std::bad_variant_access
	/// 	If the current state is not of type \c S.
	template<class S>
		requires (std::disjunction_v<std::is_same<S, States>...>)
	[[nodiscard]] auto get () const -> const S& {
		return std::get<S>(m_state);
	}

	/// \brief
	/// 	Accesses the current state as the given type for modification.
	///
	/// \tparam S The state type to access. Must be one of \p States.
	///
	/// \return A reference to the current state, valid until the next transition.
	///
	/// \throws std::bad_variant_access
	/// 	If the current state is not of type \c S.
	template<class S>
		requires (std::disjunction_v<std::is_same<S, States>...>)
	[[nodiscard]] auto get () -> S& {
		return std::get<S>(m_state);
	}

	/// \brief
	/// 	Read-only access to the variant holding the current state.
	///
	/// \return A reference to the variant, which stays valid for the lifetime of the state machine.
	[[nodiscard]] auto variant () const -> const VariantType& { return m_state; }


	////
	// Methods

	/// \brief
	/// 	Calls the controller's \c onExit hook for the given state if the controller provides one, and does nothing
	/// 	otherwise. Called by <tt>\ref transition "transition"</tt>; applications normally don't call this.
	///
	/// \tparam Old The type of the state being left.
	///
	/// \param old The state being left.
	template<class Old>
	inline void onExit (Old &old) {
		if constexpr (fsm::has_onExit<Controller, Old>) {
			m_controller.onExit(old);
		}
	}

	/// \brief
	/// 	Leaves the current state and enters a new state of type \c NewState.
	///
	/// The steps are, in this order:
	/// 1. The controller's \c onExit is called for the current state.
	/// 2. The new state is constructed from \p args.
	/// 3. The controller's \c onEnter is called for the new state. At this point, the new state is a temporary
	/// 	object that is moved into the machine afterwards, so \c onEnter must not keep references to it.
	/// 4. The new state is moved into the machine, which destroys the old state.
	///
	/// When this is called from within the controller's \c on handler, the state reference passed to \c on refers to
	/// the destroyed old state afterwards. The handler must not use it after the call.
	///
	/// If constructing the new state or \c onEnter throws, the old state remains current (although \c onExit has
	/// already been called for it).
	///
	/// \tparam NewState The type of the new state. Must be one of \p States.
	/// \tparam Args The types of the constructor arguments.
	///
	/// \param args The arguments forwarded to the constructor of \c NewState.
	template<class NewState, class... Args>
	void transition (Args&&... args)
	{
		static_assert((std::is_same_v<NewState, States> || ...), "NewState must be one of the FSM states");

		// Call exit on the current state if controller defines it.
		std::visit([this](auto &old) { onExit(old); }, m_state);

		// Construct the new state.
		NewState ns(std::forward<Args>(args)...);

		// Call enter on the new state if controller defines it.
		if constexpr (fsm::has_onEnter<Controller, NewState>) {
			m_controller.onEnter(ns);
		}
		m_state.template emplace<NewState>(std::move(ns));
	}

	/// \brief
	/// 	Passes an event to the controller's \c on handler for the current state.
	///
	/// The call <tt>on(state, event, *this)</tt> is made only if the controller has an \c on member function that
	/// accepts the current state, the event and the state machine and returns \c void. Otherwise, the event is
	/// ignored silently.
	///
	/// \tparam Event The event type.
	///
	/// \param event The event to dispatch.
	template<class Event>
	void handle (const Event& event)
	{
		std::visit([this, &event](auto &curState) {
			using Cur = std::decay_t<decltype(curState)>;
			if constexpr (requires(Controller &c, Cur &state, const Event &event, StateMachine& fsm) {
				{ c.on(state, event, fsm) } -> std::same_as<void>;
			}){
				m_controller.on(curState, event, *this);
			}
		}, m_state);
	}


private:

	////
	// Fields

	/// The controller that decides state transitions.
	Controller &m_controller;

	/// The current state of the FSM, stored as a variant over all possible states.
	VariantType m_state;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif  // ifndef __FCG_UTIL_H__
