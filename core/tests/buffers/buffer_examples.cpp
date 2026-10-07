//////
//
// Compiled examples used by the buffer module's Doxygen snippets.
//

// [includes]
#include <FCG/buffer.h>
#include <array>
#include <cstring>
#include <optional>
#include <utility>
// [includes]

namespace examples {

// [geometry]
struct Vertex {
	float x, y, z;
};
struct Geometry {
	fcg::Buffer vertices;
	fcg::Buffer indices;
};

auto createGeometry (fcg::Device &device, std::span<const Vertex> vertices, std::span<const Uint16> indices)
	-> std::expected<Geometry, fcg::BufferError>
{
	auto v = fcg::Buffer::create(device, vertices, SDL_GPU_BUFFERUSAGE_VERTEX);
	if (!v)
		return std::unexpected(v.error());
	auto i = fcg::Buffer::create(device, indices, SDL_GPU_BUFFERUSAGE_INDEX);
	if (!i)
		return std::unexpected(i.error());
	return Geometry{std::move(*v), std::move(*i)};
}

// The caller has begun a compatible render pass and bound its graphics pipeline.
auto bindGeometry (SDL_GPURenderPass *pass, const Geometry &geometry) -> std::expected<void, fcg::BufferError> {
	if (auto result = geometry.vertices.bindVertex(pass, 0); !result)
		return result;
	return geometry.indices.bindIndex(pass, SDL_GPU_INDEXELEMENTSIZE_16BIT);
}
// [geometry]

// [optional_ownership]
/// Delayed ownership: absence is a property of the slot, not a default-constructed buffer.
struct MeshSlot {
	std::optional<Geometry> geometry;

	/// Allocate and upload first. Failure leaves the previous geometry unchanged.
	auto replace (fcg::Device &device, std::span<const Vertex> vertices, std::span<const Uint16> indices)
		-> std::expected<void, fcg::BufferError>
	{
		auto replacement = createGeometry(device, vertices, indices);
		if (!replacement)
			return std::unexpected(replacement.error());
		geometry.emplace(std::move(*replacement));
		return {};
	}

	/// Check for absence before using the required buffers inside the aggregate.
	auto bind (SDL_GPURenderPass *pass) const -> std::expected<void, fcg::BufferError> {
		if (!geometry)
			return {}; // Nothing to draw; the caller also skips its draw command.
		return bindGeometry(pass, *geometry);
	}

	/// Release both allocations; a later `replace()` can create new geometry.
	void release () {
		geometry.reset();
	}
};
// [optional_ownership]

// [instances]
struct Instance {
	float offsetX, offsetY;
};

// Include this description and matching vertex attributes when creating the graphics pipeline.
auto instanceLayout () -> SDL_GPUVertexBufferDescription {
	return {.slot = 1, .pitch = sizeof(Instance), .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
		.instance_step_rate = 0};
}

auto createInstances (fcg::Device &device, std::span<const Instance> values)
	-> std::expected<fcg::Buffer, fcg::BufferError> {
	return fcg::Buffer::create(device, values, SDL_GPU_BUFFERUSAGE_VERTEX);
}

auto bindInstances (SDL_GPURenderPass *pass, const fcg::Buffer &instances)
	-> std::expected<void, fcg::BufferError> {
	return instances.bindVertex(pass, 1); // A subsequent indexed draw supplies num_instances.
}
// [instances]

// [batch]
// Construct once: fcg::UploadBatch uploads(device);
// Both fixed-size destinations must fit the supplied arrays and survive until record() returns.
auto updateGeometry (
	fcg::UploadBatch &uploads, Geometry &geometry, std::span<const Vertex> vertices,
	std::span<const Uint16> indices, SDL_GPUCopyPass *copyPass
) -> std::expected<void, fcg::BufferError>
{
	auto result = uploads.upload(geometry.vertices, vertices, 0, true);
	if (result)
		result = uploads.upload(geometry.indices, indices, 0, true);
	if (!result) {
		uploads.clear();
		return result;
	}
	return uploads.record(copyPass); // Caller ends this pass and submits its command buffer.
	// Alternatively, outside any caller-owned pass: return uploads.submit();
}
// [batch]

// [partial]
auto patch (fcg::Buffer &buffer, std::span<const float> values, std::size_t firstElement)
	-> std::expected<void, fcg::BufferError> {
	if (firstElement > buffer.size() / sizeof(float))
		return std::unexpected(fcg::BufferError{fcg::BufferErrorCode::InvalidArgument, "Element offset too large"});
	return buffer.upload(values, firstElement * sizeof(float), false); // Preserve all other bytes.
}

// The two arrays together replace all subsequently used bytes in this buffer.
auto replaceInParts (
	fcg::UploadBatch &uploads, fcg::Buffer &buffer, std::span<const float> first, std::span<const float> second
) -> std::expected<void, fcg::BufferError>
{
	auto result = uploads.upload(buffer, first, 0, true); // Cycle only the FIRST destination write.
	if (result)
		result = uploads.upload(buffer, second, first.size_bytes(), false);
	if (!result) {
		uploads.clear();
		return result;
	}
	return uploads.submit();
}
// [partial]

// [mapped]
// Reuse an UPLOAD TransferBuffer with at least 16 floats. copyPass belongs to the same device.
auto generateIntoStaging (fcg::TransferBuffer &staging, fcg::Buffer &destination, SDL_GPUCopyPass *copyPass)
	-> std::expected<void, fcg::BufferError>
{
	constexpr auto bytes = 16 * sizeof(float);
	if (staging.size() < bytes)
		return std::unexpected(fcg::BufferError{fcg::BufferErrorCode::InvalidArgument, "Staging is too small"});
	{
		auto mapping = staging.map(true); // Protect staging used by earlier uploads.
		if (!mapping)
			return std::unexpected(mapping.error());
		for (unsigned i = 0; i < 16; ++i) {
			const float value = float(i) / 15.f;
			std::memcpy(mapping->data().data() + i * sizeof(value), &value, sizeof(value));
		}
	} // Must unmap BEFORE recording the upload.
	return destination.uploadFrom(copyPass, staging, bytes, 0, 0, true);
}
// [mapped]

// [uniforms]
struct alignas(16) Material {
	std::array<float, 4> color; // GLSL: layout(std140, set=3, binding=0) uniform Material { vec4 color; };
};
static_assert(sizeof(Material) == 16);

auto pushMaterial (SDL_GPUCommandBuffer *commands, const Material &material)
	-> std::expected<void, fcg::BufferError> {
	return fcg::pushUniforms(commands, fcg::ShaderStage::FRAGMENT, 0, material);
}
// [uniforms]

// [graphics_storage]
// GLSL vertex shader: layout(std430, set=0, binding=0) readonly buffer Positions { vec4 positions[]; };
// It may also declare a uniform block at set=1, binding=0.
auto createStorageShader (fcg::Device &device, std::span<const std::byte> spirv) -> SDL_GPUShader* {
	return device.createShader(fcg::ShaderStage::VERTEX, spirv,
		fcg::ShaderResources{.uniformBuffers = 1, .storageBuffers = 1});
	// Caller checks nullptr and releases the shader after pipeline creation.
}

auto bindPositions (SDL_GPURenderPass *pass, const fcg::Buffer &positions)
	-> std::expected<void, fcg::BufferError> {
	// positions was created with SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ.
	return positions.bindStorage(pass, fcg::ShaderStage::VERTEX, 0);
}
// [graphics_storage]

// [compute]
// input uses COMPUTE_STORAGE_READ; output uses COMPUTE_STORAGE_WRITE.
// The supplied pipeline declares one read-only and one read-write storage buffer, with compatible GLSL bindings.
auto compute (
	SDL_GPUCommandBuffer *commands, SDL_GPUComputePipeline *pipeline,
	const fcg::Buffer &input, const fcg::Buffer &output
) -> std::expected<void, fcg::BufferError>
{
	auto write = output.readWriteBinding(true);
	if (!write)
		return std::unexpected(write.error());
	auto *pass = SDL_BeginGPUComputePass(commands, nullptr, 0, &*write, 1);
	if (!pass)
		return std::unexpected(fcg::BufferError{fcg::BufferErrorCode::SDLFailure, SDL_GetError()});
	SDL_BindGPUComputePipeline(pass, pipeline);
	auto result = input.bindStorage(pass, 0);
	if (result)
		SDL_DispatchGPUCompute(pass, 1, 1, 1); // Workgroup size/count must match the shader and allocated ranges.
	SDL_EndGPUComputePass(pass); // Required before a dependent dispatch reads output.
	return result;
}
// [compute]

// [indirect]
auto createDrawArguments (fcg::Device &device) -> std::expected<fcg::Buffer, fcg::BufferError> {
	const SDL_GPUIndexedIndirectDrawCommand args {
		.num_indices = 3, .num_instances = 2, .first_index = 0, .vertex_offset = 0, .first_instance = 0
	};
	return fcg::Buffer::create(device, std::span(&args, 1), SDL_GPU_BUFFERUSAGE_INDIRECT);
}

// Preconditions: args contains SDL_GPUIndexedIndirectDrawCommand, and pipeline/vertex/index buffers are bound.
void drawIndirect (SDL_GPURenderPass *pass, const fcg::Buffer &args) {
	SDL_DrawGPUIndexedPrimitivesIndirect(pass, args.handle(), 0, 1);
}

// Preconditions: args contains SDL_GPUIndirectDispatchCommand, with INDIRECT usage, and compute resources are bound.
void dispatchIndirect (SDL_GPUComputePass *pass, const fcg::Buffer &args) {
	SDL_DispatchGPUComputeIndirect(pass, args.handle(), 0);
}
// [indirect]

// [copy]
auto copyPrefix (SDL_GPUCopyPass *pass, const fcg::Buffer &source, fcg::Buffer &destination, std::size_t bytes)
	-> std::expected<void, fcg::BufferError> {
	return source.copyTo(pass, destination, bytes); // Caller ends/submits this pass; no CPU readback needed.
}
// [copy]

// [readback]
auto requestReadback (const fcg::Buffer &buffer) -> std::expected<fcg::BufferReadback, fcg::BufferError> {
	return buffer.readback(); // Call after submitting the writes whose results should be observed.
}

// Here the source buffer is known to contain exactly four Uint32 values.
auto collect (
	fcg::BufferReadback &ticket, bool block
) -> std::expected<std::optional<std::array<Uint32, 4>>, fcg::BufferError>
{
	if (block) {
		if (auto result = ticket.wait(); !result)
			return std::unexpected(result.error());
	} else if (!ticket.ready()) {
		return std::optional<std::array<Uint32, 4>>{}; // Do other work, then poll again.
	}
	auto mapping = ticket.map();
	if (!mapping)
		return std::unexpected(mapping.error());
	std::array<Uint32, 4> values;
	if (mapping->data().size() != sizeof(values))
		return std::unexpected(fcg::BufferError{fcg::BufferErrorCode::InvalidArgument, "Unexpected download size"});
	std::memcpy(values.data(), mapping->data().data(), sizeof(values));
	return values; // Mapping ends here; values are an independent CPU copy.
}
// [readback]

} // namespace examples
