
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// FCG Framework
#include <FCG/util.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Fail the test with the given diagnostic.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Initial state, whose identity and hook mutations must survive failed transitions.
struct Old {
	/// Number of exit-hook calls applied to this object.
	int exits = 0;
};

/// Replacement state with a throwing value constructor and a nonthrowing commit move.
class Next
{
public:

	////
	// Object construction/destruction

	/// Construct a value, optionally failing before the transition commits.
	Next(int value, bool fail = false) : value(value) {
		if (fail)
			throw std::runtime_error("construction sentinel");
	}

	/// Commit a prepared state without throwing.
	Next(Next &&other) noexcept : value(other.value) {}


	////
	// Fields

	/// Value transferred into the machine on success.
	int value;
};

/// State that must be rejected by the template constraint.
class ThrowingMove
{
public:

	////
	// Object construction/destruction

	/// Declare a potentially throwing move.
	ThrowingMove(ThrowingMove&&) noexcept(false) {}
};

/// Hooks that record their order and can fail before the replacement is committed.
class Controller
{
public:

	////
	// Methods

	/// Mutate the still-active old state and optionally fail.
	void onExit (Old &old) {
		calls.push_back("exit");
		++old.exits;
		if (failExit)
			throw std::runtime_error("exit sentinel");
	}

	/// Observe the prepared replacement and optionally fail.
	void onEnter (Next&) {
		calls.push_back("enter");
		if (failEnter)
			throw std::runtime_error("enter sentinel");
	}


	////
	// Fields

	/// Whether the exit hook should throw.
	bool failExit = false;

	/// Whether the entry hook should throw.
	bool failEnter = false;

	/// Hook calls in observed order.
	std::vector<std::string> calls;
};

/// Whether the machine accepts the given replacement state.
template<class State>
concept AcceptedState = requires { typename fcg::StateMachine<Controller, Old, State>; };

static_assert(AcceptedState<Next>);
static_assert(!AcceptedState<ThrowingMove>);

/// Exercise each failure before commit and the subsequent successful transition.
void transitions ()
{
	Controller controller;
	fcg::StateMachine<Controller, Old, Next> machine(controller);
	const auto *original = &machine.get<Old>();
	for (int stage = 0; stage < 3; ++stage)
	{
		controller.failExit = stage == 0;
		controller.failEnter = stage == 2;
		try {
			machine.transition<Next>(42, stage == 1);
			throw std::logic_error("Transition did not fail");
		} catch (const std::runtime_error &error) {
			const char *expected[] = {"exit sentinel", "construction sentinel", "enter sentinel"};
			require(error.what() == std::string(expected[stage]), "Changed transition exception");
		}
		require(machine.is<Old>() && &machine.get<Old>() == original, "Failed transition replaced the old state");
		require(machine.get<Old>().exits == stage + 1, "Hook side effects were lost");
		require(!machine.variant().valueless_by_exception(), "Machine lost its active state");
	}
	controller.failEnter = false;
	machine.transition<Next>(42);
	require(machine.is<Next>() && machine.get<Next>().value == 42, "Successful transition did not commit");
	require(controller.calls == std::vector<std::string>{"exit", "exit", "exit", "enter", "exit", "enter"},
		"Incorrect hook order");

	// A caller can supply its own variant. Some standard libraries preserve it on failed emplace; others lose it.
	fcg::StateMachine<Controller, Old, Next>::VariantType initial;
	try {
		initial.emplace<Next>(0, true);
	} catch (const std::runtime_error&) {}
	if (initial.valueless_by_exception())
	{
		try {
			fcg::StateMachine<Controller, Old, Next> invalid(controller, std::move(initial));
			throw std::logic_error("Valueless initializer was accepted");
		} catch (const std::invalid_argument&) {}
	}
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Verify compile constraints and state preservation without SDL initialization.
auto main () -> int
{
	try {
		transitions();
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
