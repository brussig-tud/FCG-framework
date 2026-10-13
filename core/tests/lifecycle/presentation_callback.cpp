
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
#include <FCG/presentation.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Assert a contract independently of whether release assertions are enabled.
void check (bool condition) {
	if (!condition)
		throw std::runtime_error("Presentation callback contract failed");
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

	/// Return success without using GPU resources.
	[[nodiscard]] auto operator () (const fcg::PresentationContext &) -> std::expected<void, fcg::FullscreenError> {
		return {};
	}
};

static_assert(!std::is_copy_constructible_v<fcg::PresentationCallback>);
static_assert(!std::is_copy_assignable_v<fcg::PresentationCallback>);
static_assert(std::is_nothrow_move_constructible_v<fcg::PresentationCallback>);
static_assert(std::is_nothrow_move_assignable_v<fcg::PresentationCallback>);
static_assert(std::is_nothrow_default_constructible_v<fcg::PresentationCallback>);
static_assert(!std::is_constructible_v<fcg::PresentationCallback, int>);
static_assert(!std::is_constructible_v<fcg::PresentationCallback, decltype([] (const fcg::PresentationContext &) {})>);
static_assert(std::is_same_v<
	std::invoke_result_t<fcg::PresentationCallback &, const fcg::PresentationContext &>,
	std::expected<void, fcg::FullscreenError>>);
static_assert(!std::is_invocable_v<const fcg::PresentationCallback &, const fcg::PresentationContext &>);

/// Exercise ownership, moves, replacement and clearing without initializing SDL or a GPU.
void ownership ()
{
	int destroyed = 0;
	{
		auto callable = [capture = std::make_unique<Capture> (destroyed), count = 0]
			(const fcg::PresentationContext &) mutable -> std::expected<void, fcg::FullscreenError> {
			++count;
			return {};
		};
		static_assert(!std::is_constructible_v<fcg::PresentationCallback, decltype(callable) &>);
		fcg::PresentationCallback first(std::move (callable));
		check (bool(first));
		fcg::PresentationCallback second(std::move (first));
		check (!first && bool(second) && destroyed == 0);
		first = std::move (second);
		check (!second && bool(first) && destroyed == 0);
		ThrowingCopy source;
		try {
			first = source;
			check (false);
		} catch (const std::runtime_error &error) {
			check (std::string(error.what()) == "copy failed");
		}
		check (bool(first) && destroyed == 0);
		first = [capture = std::make_unique<Capture> (destroyed)]
			(const fcg::PresentationContext &) -> std::expected<void, fcg::FullscreenError> { return {}; };
		check (destroyed == 1);
		first = fcg::PresentationCallback{};
		check (!first && destroyed == 2);
		second = [capture = std::make_unique<Capture> (destroyed)]
			(const fcg::PresentationContext &) -> std::expected<void, fcg::FullscreenError> { return {}; };
	}
	check (destroyed == 3);
}

/// Verify copyable lvalue ownership without constructing GPU-only context resources.
void lvalueOwnership ()
{
	auto capture = std::make_shared<int> (42);
	auto callable = [capture] (const fcg::PresentationContext &) -> std::expected<void, fcg::FullscreenError> {
		return {};
	};
	check (capture.use_count() == 2);
	{
		fcg::PresentationCallback callback(callable);
		check (bool(callback) && capture.use_count() == 3);
		callback = fcg::PresentationCallback{};
		check (!callback && capture.use_count() == 2);
	}
	check (capture.use_count() == 2);
}

// Anonymous namespace end
}



//////
//
// Function implementations
//

/// Run the GPU-independent callback ownership and compile-contract checks.
auto main () -> int {
	ownership ();
	lvalueOwnership ();
	return 0;
}
