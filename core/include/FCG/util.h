/**
 * \defgroup fcg_utilities Utilities
 * \ingroup fcg_components
 *
 * <code>\ref fcg::StateMachine</code> is a generic finite state machine utility. The declarations in
 * <code>\ref fcg::fsm</code> describe optional controller hooks.
 *
 * \par Guide incomplete
 * This guide is a stub. Consult the API declarations below for currently documented behavior.
 *
 * \section fcg_utilities_workflows Common workflows
 * Guide incomplete: workflow descriptions remain to be investigated and written.
 *
 * \section fcg_utilities_lifetime Ownership and lifetime
 * Guide incomplete: consult individual type and member contracts.
 *
 * \section fcg_utilities_errors Errors
 * Guide incomplete: error handling remains to be investigated and written.
 *
 * \section fcg_utilities_examples Examples
 * Guide incomplete: worked examples remain to be added and compiled.
 *
 * \see \ref fcg_runtime, \ref fcg_orbit_camera
 */

#ifndef __FCG_UTIL_H__
#define __FCG_UTIL_H__


//////
//
// Includes
//

// C++ STL
#include <string>
#include <variant>

// Local includes
#include "FCG/export.h"



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/** \addtogroup fcg_utilities
 * @{
 */



//////
//
// Classes
//

/// Auxiliaries of the <code>\ref fcg::StateMachine</code> facility.
namespace fsm {
	/// The concept of providing an \c onEnter state transition hook in a <code>\ref fcg::StateMachine</code> controller.
	template<class C, class S>
	concept has_onEnter = requires(C c, S& s) { c.onEnter(s); };

	/// The concept of providing an \c onExit state transition hook in a <code>\ref fcg::StateMachine</code> controller.
	template<class C, class S>
	concept has_onExit = requires(C c, S& s) { c.onExit(s); };
}

/// Generic finite state machine utility.
///
/// The controller type may provide the following optional member functions:
/// - \code template<class State> void onEnter(State&); \endcode
/// - \code template<class State> void onExit(State&); \endcode
/// - \code template<class State, class Event> void on(State&, const Event&, StateMachine&); \endcode
///
/// Guard conditions and transition actions are implemented inside the controller's `on` method.
template<class Controller, class... States>
class StateMachine
{
public:

	////
	// Types

	/// Our variant type resulting from our own template arguments.
	using VariantType = std::variant<States...>;


	////
	// Object construction/destruction

	/// Construct a state machine with the given controller and initial state.
	explicit StateMachine(Controller& ctrl, VariantType init={})
		: m_controller(ctrl), m_state(std::move(init))
	{}

	/// Check if the FSM is in the given state type.
	template<class S>
		requires (std::disjunction_v<std::is_same<S, States>...>)
	[[nodiscard]] auto is () const -> bool {
		return std::holds_alternative<S>(m_state);
	}

	/// Const-reference current state if it is \c S, fault otherwise.
	template<class S>
		requires (std::disjunction_v<std::is_same<S, States>...>)
	[[nodiscard]] auto get () const -> const S& {
		return std::get<S>(m_state);
	}

	/// Reference the current state if it is \c S, fault otherwise.
	template<class S>
		requires (std::disjunction_v<std::is_same<S, States>...>)
	[[nodiscard]] auto get() -> S& {
		return std::get<S>(m_state);
	}

	template<class Old>
	inline void onExit (Old &old) {
		if constexpr (fsm::has_onExit<Controller, Old>) {
			m_controller.onExit(old);
		}
	}

	/// Transition to a new state, forwarding the given constructor arguments.
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

	/// Dispatch an event to the controller.
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

	/// Expose underlying variant (read‑only).
	[[nodiscard]] const VariantType& variant() const { return m_state; }


private:

	////
	// Fields

	/// The controller that decides state transitions.
	Controller &m_controller;

	/// The corrent state of the FSM, stored as a variant over all possible states.
	VariantType m_state;
};



/** @} */

//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_UTIL_H__
