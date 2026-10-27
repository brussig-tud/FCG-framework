
//////
//
// Includes
//

// C++ STL
#include <string>
#include <utility>

// Implemented header
#include "FCG/Render/primitive_connectivity.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Translate a Core upload failure while retaining its diagnostic.
auto failure (const fcg::BufferError &error) -> std::unexpected<fcg::RenderError> {
	return std::unexpected(fcg::RenderError{fcg::RenderErrorCode::SDLFailure,
		"Primitive connectivity: " + error.message});
}

/// Capture an SDL operational failure before cleanup can change the error string.
auto sdlFailure (const char *operation) -> std::unexpected<fcg::RenderError> {
	return std::unexpected(fcg::RenderError{fcg::RenderErrorCode::SDLFailure,
		std::string(operation) + ": " + SDL_GetError()});
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
// Structs and enums
//

/// Connectivity resources; uploaded storage stays at a stable address while borrowed by a source view.
struct PrimitiveConnectivity::State
{
	////
	// Object construction

	/// Construct empty state on a borrowed device.
	explicit State(Device &device) : device(device), uploads(device) {}


	////
	// Fields

	/// The owning device, borrowed for this collection's lifetime.
	Device &device;

	/// Collection-owned storage retained for reuse while a different source is bound.
	std::unique_ptr<Buffer> owned;

	/// Reusable staging for index uploads.
	UploadBatch uploads;

	/// The committed source view, empty when there are no indices.
	ConnectivityBufferView view;

	/// Cleared when an automatic submission fails after recording.
	bool usable = true;
};



//////
//
// Class implementations
//

////
// PrimitiveConnectivity

PrimitiveConnectivity::PrimitiveConnectivity(Device &device) : state(std::make_unique<State>(device)) {}

PrimitiveConnectivity::~PrimitiveConnectivity() = default;

auto PrimitiveConnectivity::device () const -> Device& {
	return state->device;
}

auto PrimitiveConnectivity::indexCount () const -> std::size_t {
	return state->view.count;
}

auto PrimitiveConnectivity::source () const -> ConnectivityBufferView {
	return state->view;
}

auto PrimitiveConnectivity::usable () const -> bool {
	return state->usable;
}

void PrimitiveConnectivity::bind (ConnectivityBufferView view) {
	state->view = view;
	state->usable = true;
}

void PrimitiveConnectivity::clear () {
	bind({});
}

auto PrimitiveConnectivity::setIndices (std::span<const std::uint32_t> indices)
	-> std::expected<void, RenderError> {
	return upload(nullptr, indices);
}

auto PrimitiveConnectivity::setIndices (SDL_GPUCopyPass *copyPass, std::span<const std::uint32_t> indices)
	-> std::expected<void, RenderError> {
	return upload(copyPass, indices);
}

auto PrimitiveConnectivity::upload (SDL_GPUCopyPass *pass, std::span<const std::uint32_t> indices)
	-> std::expected<void, RenderError>
{
	// Empty arrays replace the source without allocating or submitting
	if (indices.empty()) {
		clear();
		return {};
	}

	// Allocate growth separately so pre-recording failures preserve the previous source
	const auto bytes = std::as_bytes(indices);
	std::unique_ptr<Buffer> replacement;
	if (!state->owned || state->owned->size() < bytes.size())
	{
		auto storage = Buffer::create(state->device, bytes.size(), SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
		if (!storage)
			return failure(storage.error());
		replacement = std::make_unique<Buffer>(std::move(*storage));
	}
	auto &destination = replacement ? *replacement : *state->owned;
	try {
		if (auto result = state->uploads.upload(destination, bytes, 0, true); !result) {
			state->uploads.clear();
			return failure(result.error());
		}
	} catch (...) {
		state->uploads.clear();
		throw;
	}

	// Record into the supplied pass or submit one automatically acquired command buffer
	std::expected<void, BufferError> result;
	if (pass)
		result = state->uploads.record(pass);
	else
	{
		auto *command = SDL_AcquireGPUCommandBuffer(state->device.handle());
		if (!command) {
			state->uploads.clear();
			return sdlFailure("Acquiring connectivity upload");
		}
		auto *copy = SDL_BeginGPUCopyPass(command);
		if (!copy) {
			auto error = sdlFailure("Beginning connectivity upload");
			SDL_CancelGPUCommandBuffer(command);
			state->uploads.clear();
			return error;
		}
		result = state->uploads.record(copy);
		SDL_EndGPUCopyPass(copy);
		if (!result)
			SDL_CancelGPUCommandBuffer(command);
		else if (!SDL_SubmitGPUCommandBuffer(command)) {
			state->usable = false;
			return sdlFailure("Submitting connectivity upload");
		}
	}
	if (!result) {
		state->uploads.clear();
		return failure(result.error());
	}

	// Publish the source only after recording or automatic submission succeeds
	if (replacement)
		state->owned = std::move(replacement);
	bind({state->owned.get(), 0, indices.size()});
	return {};
}



//////
//
// Module namespace close
//

} // namespace fcg
