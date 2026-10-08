
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cmath>
#include <limits>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/Render/primitive_attributes.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Erased layout for validation without assumptions about GLM padding or quaternion order.
struct Layout
{
	////
	// Fields

	/// Complete object size.
	std::size_t size;

	/// Offsets in shader component order.
	std::span<const std::size_t> offsets;
};

/// Obtain an attribute's fixed object representation.
template <fcg::Attribute A> auto layout () -> Layout {
	return {sizeof(typename fcg::AttributeTraits<A>::Value), fcg::AttributeTraits<A>::componentOffsets};
}

/// Validate semantic transform components; normal, radius, and color are unrestricted.
auto validValue (fcg::Attribute attribute, const glm::vec4 &value) -> bool
{
	if (attribute != fcg::Attribute::Position && attribute != fcg::Attribute::Extent
		&& attribute != fcg::Attribute::Orientation)
		return true;
	const int components = attribute == fcg::Attribute::Extent ? 3 : 4;
	for (int i = 0; i < components; ++i)
		if (!std::isfinite(value[i]))
			return false;
	if (attribute == fcg::Attribute::Position)
		return value.w != 0.f;
	if (attribute == fcg::Attribute::Extent)
		return value.x >= 0.f && value.y >= 0.f && value.z >= 0.f;
	return value != glm::vec4(0.f);
}

/// Translate Core failures with update context.
auto translate (const fcg::BufferError &error) -> fcg::RenderError {
	const auto code = error.code == fcg::BufferErrorCode::SDLFailure
		? fcg::RenderErrorCode::SDLFailure : error.code == fcg::BufferErrorCode::InvalidState
		? fcg::RenderErrorCode::InvalidState : fcg::RenderErrorCode::InvalidArgument;
	return {code, "Primitive attributes: " + error.message};
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

/// Persistent GPU state; sources never retain uploaded CPU spans.
struct PrimitiveAttributes::State
{
	////
	// Object construction/destruction

	/// Construct without GPU allocations.
	explicit State(Device &device) : device(device), uploads(device) {
		for (auto &source : sources)
			source.kind = Kind::Absent;
		usable.fill(true);
	}


	////
	// Fields

	/// Borrowed device.
	Device &device;

	/// Stable owning buffer slots, retaining capacity even when not currently used.
	std::array<std::unique_ptr<Buffer>, (std::size_t)Attribute::Count> buffers;

	/// Committed metadata; host-byte spans are always empty.
	std::array<Descriptor, (std::size_t)Attribute::Count> sources;

	/// Submission usability for committed sources.
	std::array<bool, (std::size_t)Attribute::Count> usable;

	/// Retained direct staging pages.
	UploadBatch uploads;

	/// Recursion guard, externally serialized.
	bool updating = false;
};



//////
//
// Class implementations
//

////
// PrimitiveAttributes

PrimitiveAttributes::PrimitiveAttributes(Device &device) : state(std::make_unique<State>(device)) {}

PrimitiveAttributes::~PrimitiveAttributes() = default;

auto PrimitiveAttributes::device () const -> Device& {
	return state->device;
}

auto PrimitiveAttributes::instanceCount () const -> std::size_t {
	return state->sources[(std::size_t)Attribute::Position].count;
}

auto PrimitiveAttributes::descriptor (Attribute attribute) const -> const Descriptor& {
	return state->sources[(std::size_t)attribute];
}

auto PrimitiveAttributes::isUsable (Attribute attribute) const -> bool {
	return state->usable[(std::size_t)attribute];
}

auto PrimitiveAttributes::beginUpdate () -> std::expected<void, RenderError>
{
	if (!state->device.handle())
		return std::unexpected(RenderError{RenderErrorCode::InvalidState, "Attribute device has been moved from"});
	if (state->updating)
		return std::unexpected(RenderError{RenderErrorCode::InvalidState, "Recursive attribute update"});
	state->updating = true;
	return {};
}

void PrimitiveAttributes::cancelUpdate () noexcept {
	state->uploads.clear();
	state->updating = false;
}

auto PrimitiveAttributes::finishUpdate (SDL_GPUCopyPass *pass, const Update &update)
	-> std::expected<void, RenderError>
{
	const std::array layouts{
		layout<Attribute::Position>(), layout<Attribute::Normal>(), layout<Attribute::Radius>(),
		layout<Attribute::Extent>(), layout<Attribute::Orientation>(), layout<Attribute::Color>()
	};
	const auto invalid = [] (const char *message) -> std::expected<void, RenderError> {
		return std::unexpected(RenderError{RenderErrorCode::InvalidArgument, message});
	};
	const std::size_t limit = std::numeric_limits<Uint32>::max();

	// Validate every final descriptor before allocating or staging. Earlier assignments are irrelevant.
	for (std::size_t i = 0; i < layouts.size(); ++i)
	{
		const auto &pending = update.descriptors[i];
		if (pending.kind == Kind::Array || pending.kind == Kind::View)
			if (pending.overflow || pending.count > limit || pending.stride > limit || pending.offset > limit)
				return invalid("Attribute layout exceeds SDL's byte or instance limits");
		if (pending.kind == Kind::View)
		{
			const auto *buffer = pending.buffer;
			if (!buffer || !buffer->handle() || buffer->device() != &state->device
				|| !(buffer->usage() & SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ))
				return invalid("Attribute view requires graphics storage on the collection's device");
			if (pending.offset % sizeof(float) || pending.stride % sizeof(float) || pending.stride < layouts[i].size)
				return invalid("Attribute view offset/stride must be word aligned and cover its value type");
			const auto available = std::min(buffer->size(), limit);
			if (pending.offset > available)
				return invalid("Attribute view starts outside its buffer");
			const auto remaining = available - pending.offset;
			if (pending.count && (remaining < layouts[i].size
				|| pending.count - 1 > (remaining - layouts[i].size) / pending.stride))
				return invalid("Attribute view exceeds its buffer");
		}
		if (pending.kind == Kind::Array)
		{
			if (pending.bytes.size() > limit)
				return invalid("Attribute array exceeds SDL's byte limit");
			if (i != (std::size_t)Attribute::Position && i != (std::size_t)Attribute::Extent
				&& i != (std::size_t)Attribute::Orientation)
				continue;
			for (std::size_t element = 0; element < pending.count; ++element)
			{
				glm::vec4 value{};
				for (std::size_t component = 0; component < layouts[i].offsets.size(); ++component)
					std::memcpy(&value[(int)component], pending.bytes.data() + element * pending.stride
						+ layouts[i].offsets[component], sizeof(float));
				if (!validValue((Attribute)i, value))
					return invalid("Attribute array contains a nonfinite or invalid transform");
			}
		}
		if (pending.kind == Kind::Constant && !validValue((Attribute)i, pending.constant))
			return invalid("Attribute constant contains a nonfinite or invalid transform");
	}

	// Prepare growth in separate owners. Existing source pointers stay stable on any pre-recording failure.
	std::array<std::unique_ptr<Buffer>, (std::size_t)Attribute::Count> replacements;
	for (std::size_t i = 0; i < layouts.size(); ++i)
	{
		const auto &pending = update.descriptors[i];
		if (pending.kind != Kind::Array || pending.bytes.empty())
			continue;
		if (!state->buffers[i] || state->buffers[i]->size() < pending.bytes.size())
		{
			const auto previous = state->buffers[i] ? state->buffers[i]->size() : 0;
			const auto maximum = limit - limit % 4;
			const auto growth = previous <= maximum / 2 ? previous * 2 : maximum;
			const auto capacity = std::max(pending.bytes.size(), growth);
			auto buffer = Buffer::create(state->device, capacity, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
			if (!buffer)
				return std::unexpected(translate(buffer.error()));
			replacements[i] = std::make_unique<Buffer>(std::move(*buffer));
		}
	}
	for (std::size_t i = 0; i < layouts.size(); ++i)
	{
		const auto &pending = update.descriptors[i];
		if (pending.kind != Kind::Array || pending.bytes.empty())
			continue;
		auto &destination = replacements[i] ? *replacements[i] : *state->buffers[i];
		if (auto result = state->uploads.upload(destination, pending.bytes, 0, true); !result) {
			state->uploads.clear();
			return std::unexpected(translate(result.error()));
		}
	}

	// Automatic submission uses one command buffer. A failed submission invalidates changed arrays conservatively.
	bool recorded = false;
	std::expected<void, BufferError> result;
	const bool hasArrays = std::any_of(update.descriptors.begin(), update.descriptors.end(), [] (const Descriptor &data) {
		return data.kind == Kind::Array && !data.bytes.empty();
	});
	if (pass)
		result = state->uploads.record(pass);
	else if (hasArrays)
	{
		auto *command = SDL_AcquireGPUCommandBuffer(state->device.handle());
		if (!command)
			return std::unexpected(RenderError{RenderErrorCode::SDLFailure,
				std::string("Acquiring attribute uploads: ") + SDL_GetError()});
		auto *copy = SDL_BeginGPUCopyPass(command);
		if (!copy) {
			const auto message = std::string("Beginning attribute uploads: ") + SDL_GetError();
			SDL_CancelGPUCommandBuffer(command);
			return std::unexpected(RenderError{RenderErrorCode::SDLFailure, message});
		}
		result = state->uploads.record(copy);
		SDL_EndGPUCopyPass(copy);
		if (!result)
			SDL_CancelGPUCommandBuffer(command);
		else {
			recorded = true;
			if (!SDL_SubmitGPUCommandBuffer(command))
				result = std::unexpected(BufferError{BufferErrorCode::SDLFailure,
					std::string("Submitting attribute uploads: ") + SDL_GetError()});
		}
	}
	if (!result)
	{
		state->uploads.clear();
		if (!pass && recorded)
			for (std::size_t i = 0; i < layouts.size(); ++i)
				if (update.descriptors[i].kind == Kind::Array && !update.descriptors[i].bytes.empty())
					state->usable[i] = false;
		return std::unexpected(translate(result.error()));
	}

	for (std::size_t i = 0; i < layouts.size(); ++i)
	{
		auto pending = update.descriptors[i];
		if (pending.kind == Kind::Unchanged)
			continue;
		if (pending.kind == Kind::Array) {
			if (replacements[i])
				state->buffers[i] = std::move(replacements[i]);
			pending.kind = Kind::View;
			pending.buffer = state->buffers[i].get();
			pending.bytes = {};
		}
		state->sources[i] = pending;
		state->usable[i] = true;
	}
	return {};
}



//////
//
// Module namespace close
//

} // namespace fcg
