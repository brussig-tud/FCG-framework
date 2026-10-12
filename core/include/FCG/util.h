
#ifndef __FCG_UTIL_H__
#define __FCG_UTIL_H__


//////
//
// Module documentation
//

/// \defgroup fcg_utilities Utilities
/// \ingroup fcg_modules
/// \brief A generic finite state machine whose states are types and whose behavior comes from a controller object.
///
/// <tt>\ref fcg::StateMachine "StateMachine"</tt> holds exactly one state object at a time, of one of the state types
/// given as template arguments. All behavior lives in a separate controller object: it provides an \c on handler for
/// events (for any subset of the states), and optional \c onEnter and \c onExit hooks. This keeps state-specific data
/// in the state types and transition logic in one place.
///
/// \section fcg_utilities_usage Getting started
/// Define the state types:
/// \snippet utility_examples.cpp states
/// Define a controller with handlers for the events it cares about:
/// \snippet utility_examples.cpp controller
/// Construct the machine with the controller as its behavior, and run it:
/// \snippet utility_examples.cpp state_machine
///
/// \section fcg_utilities_workflows Common tasks
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
/// The state machine stores a reference to its controller, which must outlive the machine. Each state object lives
/// until the next transition replaces it; references obtained through <tt>\ref fcg::StateMachine::get "get"</tt> are
/// invalid afterwards.
///
/// \section fcg_utilities_errors Handling errors
/// Calling <tt>\ref fcg::StateMachine::transition "transition"</tt> with a state type outside the machine's state
/// types is rejected at compile time. <tt>\ref fcg::StateMachine::get "get"</tt> throws \c std::bad_variant_access if
/// the machine is not currently in the requested state.



//////
//
// Includes
//

// C++ STL
#include <concepts>
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



/// @}



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_UTIL_H__
