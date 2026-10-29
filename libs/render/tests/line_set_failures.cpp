
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <stdexcept>
#include <string>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/Render/primitive_attributes.h>
#include <FCG/Render/primitive_connectivity.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Operational boundary at which to fail a builder upload.
enum class Failure
{
	/// Forward every operation.
	None,

	/// Reject acquiring upload commands.
	Acquire,

	/// Reject opening the copy pass.
	Copy,

	/// Report an attribute allocation or staging failure.
	Attributes,

	/// Report a connectivity allocation or staging failure after attributes were recorded.
	Connectivity,

	/// Throw after attributes were recorded.
	Exception,

	/// Consume the command buffer and report submission failure.
	Submit
};

/// Selected failure for this upload.
Failure failurePoint = Failure::None;

/// Number of acquired builder commands.
unsigned acquired = 0;

/// Number of commands canceled by builder cleanup.
unsigned canceled = 0;

/// Number of copy passes ended by the builder.
unsigned ended = 0;

/// Number of builder submission attempts.
unsigned submitted = 0;

/// Fail independently of assertions.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Create an injected collection failure without violating an input contract.
auto collectionFailure () -> std::unexpected<fcg::RenderError> {
	return std::unexpected(fcg::RenderError{fcg::RenderErrorCode::SDLFailure, "Injected allocation/staging failure"});
}

/// Count acquisition and inject an SDL failure.
auto acquire (SDL_GPUDevice *device) -> SDL_GPUCommandBuffer*
{
	++acquired;
	if (failurePoint == Failure::Acquire) {
		SDL_SetError("Injected acquisition failure");
		return nullptr;
	}
	return SDL_AcquireGPUCommandBuffer(device);
}

/// Inject copy-pass creation failure.
auto beginCopy (SDL_GPUCommandBuffer *command) -> SDL_GPUCopyPass*
{
	if (failurePoint == Failure::Copy) {
		SDL_SetError("Injected copy-pass failure");
		return nullptr;
	}
	return SDL_BeginGPUCopyPass(command);
}

/// Observe pass cleanup.
void endCopy (SDL_GPUCopyPass *pass) {
	++ended;
	SDL_EndGPUCopyPass(pass);
}

/// Observe command cancellation.
auto cancel (SDL_GPUCommandBuffer *command) -> bool {
	++canceled;
	return SDL_CancelGPUCommandBuffer(command);
}

/// Simulate SDL consuming commands on submission failure.
auto submit (SDL_GPUCommandBuffer *command) -> bool
{
	++submitted;
	if (failurePoint == Failure::Submit) {
		SDL_CancelGPUCommandBuffer(command);
		SDL_SetError("Injected submission failure");
		return false;
	}
	return SDL_SubmitGPUCommandBuffer(command);
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Classes
//

/// Attributes forwarding real uploads except at the injected operational boundary.
class ProbedAttributes : public PrimitiveAttributes
{
public:

	////
	// Object construction

	/// Construct the production collection on the borrowed device.
	using PrimitiveAttributes::PrimitiveAttributes;


	////
	// Methods

	/// Forward automatic updates used to clear failed destinations.
	using PrimitiveAttributes::setAttributes;

	/// Fail before attributes are recorded, or forward the real copy-pass update.
	template<class Fn> auto setAttributes (SDL_GPUCopyPass *pass, Fn &&fn) -> std::expected<void, RenderError> {
		if (failurePoint == Failure::Attributes)
			return collectionFailure();
		return PrimitiveAttributes::setAttributes(pass, std::forward<Fn>(fn));
	}
};

/// Connectivity forwarding real uploads except after the attribute replacement has committed.
class ProbedConnectivity : public PrimitiveConnectivity
{
public:

	////
	// Object construction

	/// Construct the production collection on the borrowed device.
	using PrimitiveConnectivity::PrimitiveConnectivity;


	////
	// Methods

	/// Fail after attributes were recorded, or forward the real connectivity upload.
	auto setIndices (SDL_GPUCopyPass *pass, std::span<const std::uint32_t> indices)
		-> std::expected<void, RenderError>
	{
		if (failurePoint == Failure::Connectivity)
			return collectionFailure();
		if (failurePoint == Failure::Exception)
			throw std::runtime_error("Injected allocation exception");
		return PrimitiveConnectivity::setIndices(pass, indices);
	}
};



//////
//
// Module namespace close
//

} // namespace fcg



//////
//
// Includes
//

// Exercise production builder control flow with operational boundaries substituted
#define PrimitiveAttributes ProbedAttributes
#define PrimitiveConnectivity ProbedConnectivity
#define LineSet ProbedLineSet
#define LineSetBuilder ProbedLineSetBuilder
#define SDL_AcquireGPUCommandBuffer acquire
#define SDL_BeginGPUCopyPass beginCopy
#define SDL_EndGPUCopyPass endCopy
#define SDL_CancelGPUCommandBuffer cancel
#define SDL_SubmitGPUCommandBuffer submit
#include "../src/line_set_builder.cpp"
#undef SDL_SubmitGPUCommandBuffer
#undef SDL_CancelGPUCommandBuffer
#undef SDL_EndGPUCopyPass
#undef SDL_BeginGPUCopyPass
#undef SDL_AcquireGPUCommandBuffer
#undef LineSetBuilder
#undef LineSet
#undef PrimitiveConnectivity
#undef PrimitiveAttributes



//////
//
// Functions
//

/// Verify cleanup, empty destinations, single submission, and retry after each operational failure.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	int result = 0;
	try
	{
		auto device = fcg::Device::create();
		require(device.has_value(), SDL_GetError());
		fcg::ProbedLineSetBuilder builder;
		builder.addPolyline({{{0, 0, 0}}, {{1, 0, 0}}});
		auto initial = builder.upload(*device);
		require(initial.has_value(), "Initial upload failed");
		auto set = std::move(*initial);
		for (const auto point : {Failure::Acquire, Failure::Copy, Failure::Attributes,
			Failure::Connectivity, Failure::Exception, Failure::Submit})
		{
			failurePoint = point;
			const auto beforeCanceled = canceled;
			const auto beforeEnded = ended;
			const auto beforeSubmitted = submitted;
			try {
				auto failed = builder.upload(set);
				require(!failed && failed.error().code == fcg::RenderErrorCode::SDLFailure,
					"Operational failure lost its category");
				require(failed.error().message.find("Injected") != std::string::npos, "Diagnostic was lost");
				require(point != Failure::Exception, "Exception was swallowed");
			} catch (const std::runtime_error &error) {
				require(point == Failure::Exception && std::string(error.what()) == "Injected allocation exception",
					"Wrong exception propagated");
			}
			require(set.attributes.instanceCount() == 0 && set.connectivity.indexCount() == 0
				&& set.attributes.usable<fcg::Attribute::Position>() && set.connectivity.usable(),
				"Failed replacement did not leave an empty usable set");
			require(canceled == beforeCanceled + (point != Failure::Acquire && point != Failure::Submit),
				"Wrong command cancellation count");
			require(ended == beforeEnded + (point != Failure::Acquire && point != Failure::Copy),
				"Wrong copy-pass cleanup count");
			require(submitted == beforeSubmitted + (point == Failure::Submit), "Wrong submission count");
			failurePoint = Failure::None;
			const auto beforeRetry = submitted;
			require(builder.upload(set).has_value() && set.connectivity.indexCount() == 2,
				"Retry did not recover geometry");
			require(submitted == beforeRetry + 1, "Successful upload did not submit exactly once");
		}
		builder.clear();
		const auto beforeEmpty = acquired;
		require(builder.upload(set).has_value() && set.connectivity.indexCount() == 0,
			"Empty replacement failed");
		require(acquired == beforeEmpty, "Empty replacement acquired commands");
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
