
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <utility>

// FCG Framework
// [includes]
#include <FCG/util.h>
// [includes]



//////
//
// Structs and enums
//

// [states]
struct Idle {};
struct Running {};

/// The event that starts the machine.
struct StartCommand {};
// [states]

// [controller]
struct Controller
{
	void on (Idle &, const StartCommand &, fcg::StateMachine<Controller, Idle, Running> &fsm) {
		fsm.transition<Running>();
	}

	void onEnter (Running &) {
		// Called when Running is entered.
	}
};
// [controller]



//////
//
// Function implementations
//

// [state_machine]
void example ()
{
	Controller controller;
	fcg::StateMachine<Controller, Idle, Running> machine(controller);
	machine.handle(StartCommand{});   // The machine is in Running now.
	(void)machine.is<Running>();
}
// [state_machine]


/// \brief Transfer a mutable callable with a move-only capture, then clear it.
///
/// \return The second value produced by the counter.
///
/// \throws std::bad_alloc If allocating the capture or wrapper fails.
// [callable_ownership]
[[nodiscard]] auto callableOwnership () -> int
{
	fcg::MoveOnlyFunction<int()> next = [base = std::make_unique<int>(10), offset = 0] () mutable -> int {
		return *base + offset++;
	};
	(void)next(); // Produces 10.
	auto transferred = std::move(next); // next is empty; transferred owns the capture.
	const int value = transferred(); // Produces 11.
	transferred = nullptr; // Releases the owned capture.
	return value;
}
// [callable_ownership]

/// \brief Invoke a callable that borrows an argument for modification.
///
/// \return The modified value.
///
/// \throws std::bad_alloc If allocating the callable wrapper fails.
// [callable_arguments]
[[nodiscard]] auto callableArguments () -> int
{
	fcg::MoveOnlyFunction<void(int&, int)> add = [] (int &value, int amount) { value += amount; };
	int value = 3;
	add(value, 4);
	return value; // The reference argument was updated to 7.
}
// [callable_arguments]
