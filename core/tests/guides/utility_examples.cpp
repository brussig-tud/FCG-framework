
//////
//
// Includes
//

// C++ STL
/* nothing here yet */

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
