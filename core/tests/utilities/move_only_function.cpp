
//////
//
// Includes
//

// C++ STL
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// Framework
#include <FCG/util.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Assert a contract independently of whether release assertions are enabled.
void check (bool condition) {
	if (!condition)
		throw std::runtime_error("Move-only function contract failed");
}

/// Count destruction of a uniquely owned capture.
class Capture
{
public:
	////
	// Object construction/destruction

	/// Borrow a counter that outlives the capture.
	explicit Capture(int &destroyed) : m_destroyed(destroyed) {}

	/// Record exactly one destruction.
	~Capture() {
		++m_destroyed;
	}


private:

	////
	// Fields

	/// The borrowed destruction counter.
	int &m_destroyed;
};

/// A copyable callable whose copy construction fails.
class ThrowingCopy
{
public:
	////
	// Object construction/destruction

	/// Construct an ordinary source callable.
	ThrowingCopy() = default;

	/// Throw to verify strong replacement guarantees.
	ThrowingCopy(const ThrowingCopy &) {
		throw std::runtime_error("copy failed");
	}


	////
	// Methods

	/// Return a sentinel value after successful construction.
	[[nodiscard]] auto operator () () -> int {
		return 42;
	}
};

/// Exercise ownership, moves, replacement and clearing without initializing SDL or a GPU.
void ownership ()
{
	int destroyed = 0;
	{
		auto callable = [capture = std::make_unique<Capture> (destroyed), count = 0]
			() mutable -> int {
			return ++count;
		};
		static_assert(!std::is_constructible_v<fcg::MoveOnlyFunction<int()>, decltype(callable) &>);
		fcg::MoveOnlyFunction<int()> first(std::move (callable));
		check (bool(first) && first() == 1);
		fcg::MoveOnlyFunction<int()> second(std::move (first));
		check (!first && bool(second) && destroyed == 0 && second() == 2);
		first = std::move (second);
		check (!second && bool(first) && destroyed == 0 && first() == 3);
		ThrowingCopy source;
		try {
			first = source;
			check (false);
		} catch (const std::runtime_error &error) {
			check (std::string(error.what()) == "copy failed");
		}
		check (bool(first) && destroyed == 0 && first() == 4);
		auto &self = first;
		first = std::move(self);
		check (bool(first) && destroyed == 0 && first() == 5);
		first = [capture = std::make_unique<Capture> (destroyed)]
			() -> int { return 42; };
		check (destroyed == 1);
		first = fcg::MoveOnlyFunction<int()>{};
		check (!first && destroyed == 2);
		second = [capture = std::make_unique<Capture> (destroyed)]
			() -> int { return 42; };
	}
	check (destroyed == 3);
}

/// Verify that copying a callable into the wrapper retains its captures.
void lvalueOwnership ()
{
	auto capture = std::make_shared<int> (42);
	auto callable = [capture] () -> int {
		return 42;
	};
	check (capture.use_count() == 2);
	{
		fcg::MoveOnlyFunction<int()> function(callable);
		check (bool(function) && capture.use_count() == 3);
		function = nullptr;
		check (!function && capture.use_count() == 2);
	}
	check (capture.use_count() == 2);
}

/// A value whose moves reveal unwanted transfers inside the invocation machinery.
class Argument
{
public:
	////
	// Object construction/destruction

	/// \brief Borrow a counter of argument moves.
	///
	/// \param moves Counter that outlives this argument.
	explicit Argument(int &moves) : moves(moves) {}

	/// \brief Count transfer of this argument.
	///
	/// \param other The argument to move from.
	Argument(Argument &&other) noexcept : moves(other.moves) { ++moves; }


private:

	////
	// Fields

	/// Caller-owned count of moves.
	int &moves;
};

/// A callable that may only be invoked on an rvalue.
class RvalueOnly
{
public:
	////
	// Methods

	/// Return a sentinel when invoked on an rvalue.
	[[nodiscard]] auto operator () () && -> int { return 42; }
};

/// An object for invoking member function and data pointers.
class Counter
{
public:
	////
	// Methods

	/// \brief Add an amount to the counter.
	///
	/// \param amount The amount to add.
	///
	/// \return The updated value.
	[[nodiscard]] auto add (int amount) -> int { return value += amount; }


	////
	// Fields

	/// Mutable value also accessible through a member data pointer.
	int value = 0;
};

/// \brief Add two arguments through an ordinary function pointer.
///
/// \param left First operand.
/// \param right Second operand.
///
/// \return The sum.
[[nodiscard]] auto sum (int left, int right) -> int { return left + right; }

/// \brief Check that a callable wrapper throws when empty.
///
/// \tparam Signature The signature to exercise.
/// \tparam Args The supplied argument types.
///
/// \param function The empty wrapper.
/// \param args Arguments suitable for its signature.
template <typename Signature, typename... Args>
void checkEmpty (fcg::MoveOnlyFunction<Signature> &function, Args&&... args)
{
	check(!function);
	bool caught = false;
	try {
		function(std::forward<Args>(args)...);
	} catch (const std::bad_function_call &) {
		caught = true;
	}
	check(caught);
}

/// \brief Detect the ordinary function signatures supported by the wrapper.
///
/// \tparam Signature The function type to check.
template <typename Signature>
concept SupportedSignature = requires { sizeof(fcg::MoveOnlyFunction<Signature>); };

static_assert(!std::is_copy_constructible_v<fcg::MoveOnlyFunction<int()>>);
static_assert(!std::is_copy_assignable_v<fcg::MoveOnlyFunction<int()>>);
static_assert(std::is_nothrow_move_constructible_v<fcg::MoveOnlyFunction<int()>>);
static_assert(std::is_nothrow_move_assignable_v<fcg::MoveOnlyFunction<int()>>);
static_assert(std::is_nothrow_default_constructible_v<fcg::MoveOnlyFunction<int()>>);
static_assert(std::is_nothrow_constructible_v<fcg::MoveOnlyFunction<int()>, std::nullptr_t>);
static_assert(!std::is_constructible_v<fcg::MoveOnlyFunction<int()>, int>);
static_assert(!std::is_constructible_v<fcg::MoveOnlyFunction<int()>, decltype([] {})>);
static_assert(!std::is_constructible_v<fcg::MoveOnlyFunction<void()>, decltype([] { return 1; })>);
static_assert(!std::is_constructible_v<fcg::MoveOnlyFunction<long()>, decltype([] { return 1; })>);
static_assert(!std::is_constructible_v<fcg::MoveOnlyFunction<const int&()>, decltype([] { return 1; })>);
static_assert(!std::is_constructible_v<fcg::MoveOnlyFunction<int(int)>, decltype([] { return 1; })>);
static_assert(!std::is_constructible_v<fcg::MoveOnlyFunction<int()>, RvalueOnly>);
static_assert(!std::is_invocable_v<const fcg::MoveOnlyFunction<int()> &>);
static_assert(std::is_same_v<std::invoke_result_t<fcg::MoveOnlyFunction<void()> &>, void>);
static_assert(std::is_same_v<std::invoke_result_t<fcg::MoveOnlyFunction<int&()> &>, int&>);
static_assert(SupportedSignature<int()> && !SupportedSignature<int() const>);
static_assert(!SupportedSignature<int() &> && !SupportedSignature<int() &&>);
static_assert(!SupportedSignature<int() noexcept> && !SupportedSignature<int>);

/// Exercise value/reference forwarding, result ownership, and invocation exceptions.
void invocation ()
{
	int value = 3;
	fcg::MoveOnlyFunction<void(int&, const int&)> add = [] (int &target, const int &amount) { target += amount; };
	const int amount = 4;
	add(value, amount);
	check(value == 7);
	fcg::MoveOnlyFunction<int&()> reference = [&] () -> int& { return value; };
	reference() = 9;
	check(value == 9 && &reference() == &value);
	fcg::MoveOnlyFunction<const int&(const int&)> borrow = [] (const int &arg) -> const int& { return arg; };
	check(&borrow(amount) == &amount);
	fcg::MoveOnlyFunction<int&&(int&&)> forward = [] (int &&arg) -> int&& { return std::move(arg); };
	int &&forwarded = forward(std::move(value));
	check(&forwarded == &value);

	fcg::MoveOnlyFunction<std::unique_ptr<int>(std::unique_ptr<int>)> transfer =
		[] (std::unique_ptr<int> arg) { return arg; };
	auto owned = std::make_unique<int>(42);
	auto result = transfer(std::move(owned));
	check(!owned && result && *result == 42);
	fcg::MoveOnlyFunction<void(std::unique_ptr<int>&&)> consume = [&] (std::unique_ptr<int> &&arg) {
		owned = std::move(arg);
	};
	consume(std::move(result));
	check(!result && owned && *owned == 42);

	int moves = 0;
	Argument argument(moves);
	fcg::MoveOnlyFunction<void(Argument)> receive = [] (const Argument &) {};
	receive(std::move(argument));
	check(moves == 1);

	fcg::MoveOnlyFunction<void()> throwing = [] { throw std::logic_error("invocation sentinel"); };
	bool caught = false;
	try { throwing(); }
	catch (const std::logic_error &error) { caught = std::string(error.what()) == "invocation sentinel"; }
	check(caught && bool(throwing));
}

/// Check ordinary and member pointers, empty calls, clearing, and moved-from state.
void pointersAndEmptyStates ()
{
	fcg::MoveOnlyFunction<int(int, int)> function = sum;
	check(function(2, 3) == 5);
	function = static_cast<int(*)(int, int)>(nullptr);
	checkEmpty(function, 2, 3);
	function = sum;
	auto moved = std::move(function);
	checkEmpty(function, 2, 3);
	check(moved(3, 4) == 7);

	Counter counter;
	fcg::MoveOnlyFunction<int(Counter&, int)> member = &Counter::add;
	check(member(counter, 5) == 5);
	member = static_cast<int(Counter::*)(int)>(nullptr);
	checkEmpty(member, counter, 5);
	fcg::MoveOnlyFunction<int&(Counter&)> data = &Counter::value;
	data(counter) = 8;
	check(counter.value == 8);
	data = static_cast<int Counter::*>(nullptr);
	checkEmpty(data, counter);

	fcg::MoveOnlyFunction<void()> empty;
	checkEmpty(empty);
	fcg::MoveOnlyFunction<void()> null = nullptr;
	checkEmpty(null);
	moved = nullptr;
	checkEmpty(moved, 3, 4);
}

// Anonymous namespace end
}



//////
//
// Function implementations
//

/// Run the GPU-independent callable ownership, invocation and compile-contract checks.
auto main () -> int
{
	ownership ();
	lvalueOwnership ();
	invocation ();
	pointersAndEmptyStates ();
	return 0;
}
