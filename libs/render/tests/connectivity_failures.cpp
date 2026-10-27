
//////
//
// Includes
//

// C++ STL
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#define PrimitiveConnectivity ProbedConnectivity
#include <FCG/Render/primitive_connectivity.h>
#undef PrimitiveConnectivity



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Operational boundary at which the next upload fails.
enum class Failure
{
	/// Forward every operation.
	None,

	/// Reject acquiring the automatic upload command buffer.
	Acquire,

	/// Reject beginning its copy pass.
	Copy,

	/// Cancel recorded commands and report a failed submission.
	Submit
};

/// Selected operational failure, leaving all input contracts valid.
Failure failurePoint = Failure::None;

/// Number of automatic command buffers canceled during cleanup.
unsigned canceled = 0;

/// Fail even when assertions are disabled.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Inject acquisition failure while preserving the device argument.
auto acquire (SDL_GPUDevice *device) -> SDL_GPUCommandBuffer* {
	if (failurePoint == Failure::Acquire) {
		SDL_SetError("Injected command acquisition failure");
		return nullptr;
	}
	return SDL_AcquireGPUCommandBuffer(device);
}

/// Inject a copy-pass failure on an otherwise valid command buffer.
auto beginCopy (SDL_GPUCommandBuffer *command) -> SDL_GPUCopyPass* {
	if (failurePoint == Failure::Copy) {
		SDL_SetError("Injected copy-pass failure");
		return nullptr;
	}
	return SDL_BeginGPUCopyPass(command);
}

/// Observe automatic cleanup and forward cancellation to the real backend.
auto cancel (SDL_GPUCommandBuffer *command) -> bool {
	++canceled;
	return SDL_CancelGPUCommandBuffer(command);
}

/// Consume recorded commands before reporting an injected submission failure.
auto submit (SDL_GPUCommandBuffer *command) -> bool {
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
// Includes
//

// Exercise production upload control flow with only the SDL operational boundaries substituted
#define PrimitiveConnectivity ProbedConnectivity
#define SDL_AcquireGPUCommandBuffer acquire
#define SDL_BeginGPUCopyPass beginCopy
#define SDL_CancelGPUCommandBuffer cancel
#define SDL_SubmitGPUCommandBuffer submit
#include "../src/primitive_connectivity.cpp"
#undef SDL_SubmitGPUCommandBuffer
#undef SDL_CancelGPUCommandBuffer
#undef SDL_BeginGPUCopyPass
#undef SDL_AcquireGPUCommandBuffer
#undef PrimitiveConnectivity



//////
//
// Functions
//

/// Verify transactional source publication, failure state and recovery through production code.
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
		fcg::ProbedConnectivity connectivity(*device);
		const std::array<std::uint32_t, 2> original{0, 1};
		const std::array<std::uint32_t, 4> replacement{1, 0, 0, 1};
		require(connectivity.setIndices(original).has_value(), "Initial upload failed");
		const auto committed = connectivity.source();
		for (const auto point : {Failure::Acquire, Failure::Copy, Failure::Submit})
		{
			failurePoint = point;
			const auto beforeCanceled = canceled;
			const auto failed = connectivity.setIndices(replacement);
			require(!failed && failed.error().code == fcg::RenderErrorCode::SDLFailure,
				"Operational failure lost its category");
			require(failed.error().message.find("Injected") != std::string::npos, "Failure lost its diagnostic");
			require(connectivity.source().buffer == committed.buffer && connectivity.indexCount() == original.size(),
				"Failed upload published a new source");
			require(connectivity.usable() == (point != Failure::Submit), "Wrong upload failure usability");
			require(canceled == beforeCanceled + (point == Failure::Copy), "Wrong automatic command cleanup");
			failurePoint = Failure::None;
			require(connectivity.setIndices(original).has_value() && connectivity.usable(),
				"Successful replacement did not recover upload state");
		}

		// Empty replacement and borrowed binding also restore source usability after a failed submission
		failurePoint = Failure::Submit;
		require(!connectivity.setIndices(original) && !connectivity.usable(), "Submission failure was not recorded");
		connectivity.clear();
		require(connectivity.usable() && connectivity.indexCount() == 0, "Clear did not restore usability");
		failurePoint = Failure::None;
		require(connectivity.setIndices(original).has_value(), "Upload after clear failed");
		auto borrowed = connectivity.source();
		failurePoint = Failure::Submit;
		require(!connectivity.setIndices(original), "Expected a second submission failure");
		connectivity.bind(borrowed);
		require(connectivity.usable(), "Borrowed binding did not restore usability");

		// A canceled caller-owned upload is recovered by clearing before any draw accesses its contents
		failurePoint = Failure::None;
		auto *command = SDL_AcquireGPUCommandBuffer(device->handle());
		auto *copy = SDL_BeginGPUCopyPass(command);
		require(connectivity.setIndices(copy, original).has_value(), "Caller copy recording failed");
		SDL_EndGPUCopyPass(copy);
		require(SDL_CancelGPUCommandBuffer(command), SDL_GetError());
		connectivity.clear();
		require(connectivity.setIndices(original).has_value(), "Caller cancellation recovery failed");
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
