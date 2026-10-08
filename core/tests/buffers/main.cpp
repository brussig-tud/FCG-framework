//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <type_traits>
#include <stdexcept>
#include <utility>
#include <vector>

// SDL3 and shadercross (compute pipeline creation deliberately remains an SDL operation)
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

// FCG Framework and embedded test shaders
#include <FCG/buffer.h>
#include <FCG/res.h>
#include <FCG/player.h>
#include <buffer-resources.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

// Resource ownership cannot start empty; optional slots can.
static_assert(!std::is_default_constructible_v<fcg::Buffer>);
static_assert(!std::is_default_constructible_v<fcg::TransferBuffer>);
static_assert(!std::is_copy_constructible_v<fcg::Buffer> && !std::is_copy_assignable_v<fcg::Buffer>);
static_assert(!std::is_copy_constructible_v<fcg::TransferBuffer> && !std::is_copy_assignable_v<fcg::TransferBuffer>);
static_assert(std::is_nothrow_move_constructible_v<fcg::Buffer> && std::is_nothrow_move_assignable_v<fcg::Buffer>);
static_assert(std::is_nothrow_move_constructible_v<fcg::TransferBuffer>
	&& std::is_nothrow_move_assignable_v<fcg::TransferBuffer>);
static_assert(std::is_default_constructible_v<std::optional<fcg::Buffer>>);
static_assert(std::is_default_constructible_v<std::optional<fcg::TransferBuffer>>);

/// Test assertions must also run in release builds.
void require (bool value, const char *message) {
	if (!value)
		throw std::runtime_error(message);
}

/// Turn a failed wrapper operation into a readable test failure.
void check (std::expected<void, fcg::BufferError> result) {
	if (!result)
		throw std::runtime_error(result.error().message);
}

/// Move a successful owning result into test scope, preserving its RAII cleanup.
template <class T>
auto take (std::expected<T, fcg::BufferError> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Assert the error category without invoking SDL with invalid handles.
template <class T>
void fails (const std::expected<T, fcg::BufferError> &result, fcg::BufferErrorCode code) {
	require(!result && result.error().code == code, "Missing or incorrect error category");
}

/// Own a test command buffer and any active pass, including assertion-failure cleanup.
class Commands
{
public:
	explicit Commands (fcg::Device &device) : handle(SDL_AcquireGPUCommandBuffer(device.handle())) {
		require(handle, SDL_GetError());
	}
	~Commands () {
		end();
		if (handle)
			SDL_CancelGPUCommandBuffer(handle);
	}
	Commands (const Commands&) = delete;
	auto operator= (const Commands&) -> Commands& = delete;
	void end () {
		if (copy) SDL_EndGPUCopyPass(std::exchange(copy, nullptr));
		if (compute) SDL_EndGPUComputePass(std::exchange(compute, nullptr));
		if (render) SDL_EndGPURenderPass(std::exchange(render, nullptr));
	}
	auto beginCopy () -> SDL_GPUCopyPass* {
		end();
		copy = SDL_BeginGPUCopyPass(handle);
		require(copy, SDL_GetError());
		return copy;
	}
	void submit () {
		end();
		require(SDL_SubmitGPUCommandBuffer(std::exchange(handle, nullptr)), SDL_GetError());
	}
	auto submitFence () -> SDL_GPUFence* {
		end();
		auto *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(std::exchange(handle, nullptr));
		require(fence, SDL_GetError());
		return fence;
	}
	SDL_GPUCommandBuffer *handle = nullptr;
	SDL_GPUCopyPass *copy = nullptr;
	SDL_GPUComputePass *compute = nullptr;
	SDL_GPURenderPass *render = nullptr;
};

/// Collect a ticket into an independently owned CPU array, then release its mapping before its owner.
template <class T>
auto read (const fcg::Buffer &buffer) -> std::vector<T> {
	auto ticket = take(buffer.readback());
	check(ticket.wait());
	require(ticket.ready(), "Waited readback is not ready");
	auto mapping = take(ticket.map());
	require(mapping.data().size() % sizeof(T) == 0, "Readback element size mismatch");
	std::vector<T> result(mapping.data().size() / sizeof(T));
	std::memcpy(result.data(), mapping.data().data(), mapping.data().size());
	return result;
}

/// Exercise range validation, scoped mappings, staging reuse, copying, cycling, and asynchronous ownership.
void transfers (fcg::Device &device)
{
	using Code = fcg::BufferErrorCode;
	constexpr auto storageUsage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
	fails(fcg::Buffer::create(device, 0, storageUsage), Code::InvalidArgument);
	fails(fcg::Buffer::create(device, std::numeric_limits<std::size_t>::max(), storageUsage), Code::InvalidArgument);
	fails(fcg::Buffer::create(device, 16, 0), Code::InvalidArgument);
	fails(fcg::Buffer::create(device, 16, SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_INDEX), Code::InvalidArgument);
	fails(fcg::TransferBuffer::create(device, 0, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD), Code::InvalidArgument);

	// A player can exist before viewport/readback storage has been created.
	{
		fcg::Player player(device, nullptr, {});
		bool rejected = false;
		try {
			(void)player.scheduleDepthReadback();
		} catch (const std::runtime_error&) {
			rejected = true;
		}
		require(rejected, "Player accepted readback without a frame or storage");
	}

	std::optional<fcg::Buffer> optionalBuffer;
	std::optional<fcg::TransferBuffer> optionalTransfer;
	require(!optionalBuffer && !optionalTransfer, "Optional resource slots started engaged");
	for (unsigned i = 0; i < 2; ++i) {
		optionalBuffer.emplace(take(fcg::Buffer::create(device, 16, storageUsage)));
		optionalTransfer.emplace(take(fcg::TransferBuffer::create(device, 16, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD)));
		require(optionalBuffer->handle() && optionalBuffer->device() == &device
			&& optionalBuffer->size() == 16 && optionalBuffer->usage() == storageUsage,
			"GPU factory did not initialize resource metadata");
		require(optionalTransfer->handle() && optionalTransfer->device() == &device
			&& optionalTransfer->size() == 16 && optionalTransfer->usage() == SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			"Transfer factory did not initialize resource metadata");
		{
			auto mapping = take(optionalTransfer->map(true));
			std::memset(mapping.data().data(), 0, mapping.data().size());
		} // End the borrow before resetting the owner.
		auto transferred = std::move(*optionalBuffer);
		require(optionalBuffer.has_value() && !optionalBuffer->handle() && transferred.handle(),
			"Moving out of an optional changed its engagement");
		optionalBuffer.reset();
		optionalTransfer.reset();
		require(!optionalBuffer && !optionalTransfer, "Reset did not disengage resource slots");
	}

	std::array<Uint32, 8> values {1, 2, 3, 4, 5, 6, 7, 8};
	auto buffer = take(fcg::Buffer::create(device, std::span(values), storageUsage));
	require(read<Uint32>(buffer) == std::vector<Uint32>(values.begin(), values.end()), "Initial upload differs");
	check(buffer.upload(std::span<const std::byte>{}, buffer.size()));
	fails(buffer.upload(std::span(values), 1), Code::InvalidArgument);
	fails(buffer.upload(std::span<const std::byte>{}, std::numeric_limits<std::size_t>::max()), Code::InvalidArgument);
	fails(buffer.readback(buffer.size()), Code::InvalidArgument);
	fails(buffer.readback(0, 0), Code::InvalidArgument);
	fails(buffer.readback(0, buffer.size() + 1), Code::InvalidArgument);
	fails(buffer.binding(), Code::InvalidArgument);
	fails(buffer.readWriteBinding(), Code::InvalidArgument);
	fails(buffer.bindVertex(nullptr), Code::InvalidArgument);

	const std::array<Uint32, 2> patch {21, 22};
	check(buffer.upload(std::span(patch), 2 * sizeof(Uint32)));
	values[2] = 21; values[3] = 22;
	require(read<Uint32>(buffer) == std::vector<Uint32>(values.begin(), values.end()), "Partial upload lost untouched bytes");

	// Move construction and assignment retain the resource and empty the source.
	auto moved = std::move(buffer);
	require(!buffer.handle() && !buffer.device() && buffer.size() == 0, "Moved buffer still owns storage");
	fails(buffer.readback(), Code::InvalidState);
	fails(buffer.upload(std::span<const std::byte>{}), Code::InvalidState);
	buffer = std::move(moved);
	require(!moved.handle(), "Move-assigned source still owns storage");

	auto upload = take(fcg::TransferBuffer::create(device, sizeof(values), SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD));
	auto download = take(fcg::TransferBuffer::create(device, sizeof(values), SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD));
	fails(download.map(true), Code::InvalidArgument);
	{
		auto mapping = take(upload.map(true));
		fails(upload.map(), Code::InvalidState);
		std::memcpy(mapping.data().data(), values.data(), sizeof(values));
		auto movedMapping = std::move(mapping);
		require(mapping.data().empty(), "Moved mapping still exposes memory");
		Commands commands(device);
		fails(buffer.uploadFrom(commands.beginCopy(), upload, sizeof(values)), Code::InvalidState);
		movedMapping.unmap();
		movedMapping.unmap();
		check(buffer.uploadFrom(commands.copy, upload, sizeof(values)));
		fails(buffer.uploadFrom(commands.copy, download, sizeof(values)), Code::InvalidArgument);
		fails(buffer.downloadTo(commands.copy, upload, sizeof(values)), Code::InvalidArgument);
		fails(buffer.downloadTo(commands.copy, download, 0), Code::InvalidArgument);
		check(buffer.downloadTo(commands.copy, download, sizeof(values)));
		auto *fence = commands.submitFence();
		const bool waited = SDL_WaitForGPUFences(device.handle(), true, &fence, 1);
		SDL_ReleaseGPUFence(device.handle(), fence);
		require(waited, SDL_GetError());
	}
	{
		auto mapping = take(download.map());
		require(std::memcmp(mapping.data().data(), values.data(), sizeof(values)) == 0, "Explicit download differs");
		Commands commands(device);
		fails(buffer.downloadTo(commands.beginCopy(), download, sizeof(values)), Code::InvalidState);
	}
	auto uploadMoved = std::move(upload);
	require(!upload.handle() && !upload.device() && !upload.size() && !upload.mapped(),
		"Moved staging retained ownership");
	fails(upload.map(), Code::InvalidState);
	upload = std::move(uploadMoved);

	auto destination = take(fcg::Buffer::create(device, sizeof(values), storageUsage));
	{
		Commands commands(device);
		auto *pass = commands.beginCopy();
		fails(buffer.copyTo(pass, buffer, 16, 0, 4), Code::InvalidArgument);
		fails(buffer.copyTo(pass, buffer, 4, 0, 16, true), Code::InvalidArgument);
		check(buffer.copyTo(pass, destination, sizeof(values)));
		commands.submit();
	}
	require(read<Uint32>(destination) == read<Uint32>(buffer), "GPU copy differs");

	// Each readback must retain the version it downloaded even while the source/staging cycle repeatedly.
	fcg::UploadBatch batch(device);
	std::vector<fcg::BufferReadback> tickets;
	for (Uint32 i = 0; i < 12; ++i) {
		values.fill(i);
		check(batch.upload(buffer, std::span(values).first<4>(), 0, true));
		check(batch.upload(buffer, std::span(values).last<4>(), sizeof(values) / 2, false));
		if (i % 2) {
			Commands commands(device);
			check(batch.record(commands.beginCopy()));
			commands.submit();
		} else {
			check(batch.submit());
		}
		tickets.push_back(take(buffer.readback()));
	}
	for (Uint32 i = 0; i < tickets.size(); ++i) {
		auto &ticket = tickets[i];
		auto early = ticket.map();
		if (!early)
			fails(early, Code::NotReady);
		else
			early->unmap(); // The GPU is allowed to complete before the first poll.
		check(ticket.wait());
		auto mapping = take(ticket.map());
		fails(ticket.map(), Code::InvalidState);
		std::array<Uint32, 8> actual;
		std::memcpy(actual.data(), mapping.data().data(), sizeof(actual));
		require(std::all_of(actual.begin(), actual.end(), [i](auto x) { return x == i; }), "Cycling corrupted a snapshot");
	}
	auto ticket = std::move(tickets.back());
	fails(tickets.back().wait(), Code::InvalidState);
	fails(tickets.back().map(), Code::InvalidState);
	check(ticket.wait());
	for (unsigned i = 0; i < 4; ++i)
		ticket = take(buffer.readback()); // Replacing a pending ticket must defer its old fence too.
	check(ticket.wait());
	{
		auto abandoned = take(buffer.readback()); // Pending destruction must not wait or invalidate later work.
	}

	// Force multiple staging pages and reuse the batch, including its move path and explicit discard.
	std::vector<Uint32> large(40000, 0xabcdef12);
	auto largeBuffer = take(fcg::Buffer::create(device, large.size() * sizeof(Uint32), storageUsage));
	check(batch.upload(buffer, std::span(values)));
	check(batch.upload(largeBuffer, std::span(large)));
	auto movedBatch = std::move(batch);
	fails(batch.submit(), Code::InvalidState);
	check(movedBatch.submit());
	require(read<Uint32>(largeBuffer) == large, "Staging page growth corrupted bytes");
	check(movedBatch.upload(buffer, std::span(patch)));
	movedBatch.clear();
	check(movedBatch.submit());
	require(read<Uint32>(buffer).front() == 11, "Discarded batch was submitted");

	const std::array<std::byte, 1> tinyData {std::byte{42}};
	auto tiny = take(fcg::Buffer::create(device, std::span(tinyData), storageUsage));
	require(tiny.size() == 1 && read<std::byte>(tiny)[0] == tinyData[0], "Small logical allocation failed");
}

/// Own SDL-only test resources, keeping them alive through all recorded commands.
struct Pipelines {
	fcg::Device &device;
	SDL_GPUComputePipeline *compute = nullptr;
	SDL_GPUGraphicsPipeline *graphics = nullptr;
	SDL_GPUTexture *color = nullptr;
	~Pipelines () {
		if (compute) SDL_ReleaseGPUComputePipeline(device.handle(), compute);
		if (graphics) SDL_ReleaseGPUGraphicsPipeline(device.handle(), graphics);
		if (color) SDL_ReleaseGPUTexture(device.handle(), color);
	}
};

/// Compute storage reads/writes, pushed uniforms, pass boundaries, and indirect dispatch produce known integers.
void compute (fcg::Device &device)
{
	auto shader = fcg::res::shader(buffer_test::embedded::FS, "buffer_compute");
	require(shader.has_value(), "Missing compute shader");
	const auto stage = shader->stage(fcg::ShaderStage::COMPUTE);
	require(stage.has_value(), "Missing compute stage");
	Pipelines pipelines{device};
	if (SDL_GetGPUShaderFormats(device.handle()) & SDL_GPU_SHADERFORMAT_SPIRV) {
		const SDL_GPUComputePipelineCreateInfo info {
			.code_size = stage->spirv.size(), .code = reinterpret_cast<const Uint8*>(stage->spirv.data()),
			.entrypoint = "main", .format = SDL_GPU_SHADERFORMAT_SPIRV,
			.num_readonly_storage_buffers = 1, .num_readwrite_storage_buffers = 1, .num_uniform_buffers = 1,
			.threadcount_x = 1, .threadcount_y = 1, .threadcount_z = 1
		};
		pipelines.compute = SDL_CreateGPUComputePipeline(device.handle(), &info);
	} else {
		require(SDL_ShaderCross_Init(), SDL_GetError());
		const SDL_ShaderCross_SPIRV_Info info {
			.bytecode = reinterpret_cast<const Uint8*>(stage->spirv.data()), .bytecode_size = stage->spirv.size(),
			.entrypoint = "main", .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE
		};
		auto *meta = SDL_ShaderCross_ReflectComputeSPIRV(info.bytecode, info.bytecode_size, 0);
		require(meta, SDL_GetError());
		pipelines.compute = SDL_ShaderCross_CompileComputePipelineFromSPIRV(device.handle(), &info, meta, 0);
		SDL_free(meta);
	}
	require(pipelines.compute, SDL_GetError());
	const std::array<Uint32, 4> initial {1, 3, 5, 7};
	auto input = take(fcg::Buffer::create(device, std::span(initial), SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ));
	const std::array<Uint32, 4> existing {2, 2, 2, 2};
	auto output = take(fcg::Buffer::create(device, std::span(existing),
		SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE));
	auto second = take(fcg::Buffer::create(device, std::span(existing), SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE));
	const SDL_GPUIndirectDispatchCommand args {.groupcount_x = 4, .groupcount_y = 1, .groupcount_z = 1};
	auto indirect = take(fcg::Buffer::create(device, std::span(&args, 1), SDL_GPU_BUFFERUSAGE_INDIRECT));
	Commands commands(device);
	const std::array<Uint32, 4> params {2, 1, 0, 0};
	check(fcg::pushUniforms(commands.handle, fcg::ShaderStage::COMPUTE, 0, params));
	fails(fcg::pushUniforms(commands.handle, fcg::ShaderStage::COMPUTE, 4, params), fcg::BufferErrorCode::InvalidArgument);
	fails(fcg::pushUniforms(commands.handle, static_cast<fcg::ShaderStage>(99), 0, params), fcg::BufferErrorCode::InvalidArgument);
	for (unsigned step = 0; step < 2; ++step) {
		auto binding = take((step ? second : output).readWriteBinding());
		commands.compute = SDL_BeginGPUComputePass(commands.handle, nullptr, 0, &binding, 1);
		require(commands.compute, SDL_GetError());
		SDL_BindGPUComputePipeline(commands.compute, pipelines.compute);
		check((step ? output : input).bindStorage(commands.compute));
		if (step)
			SDL_DispatchGPUComputeIndirect(commands.compute, indirect.handle(), 0);
		else
			SDL_DispatchGPUCompute(commands.compute, 4, 1, 1);
		commands.end(); // Dependency between the two dispatches requires separate passes.
	}
	commands.submit();
	require(read<Uint32>(output) == std::vector<Uint32>({5, 9, 13, 17}), "Compute storage/uniform result differs");
	require(read<Uint32>(second) == std::vector<Uint32>({13, 21, 29, 37}), "Indirect dependent dispatch differs");
}

/// Indexed instancing, storage in both graphics stages, uniform pushes, and indirect draws render verified pixels.
void graphics (fcg::Device &device)
{
	auto shader = fcg::res::shader(buffer_test::embedded::FS, "buffer_graphics");
	require(shader.has_value(), "Missing graphics shader");
	const auto vertex = shader->stage(fcg::ShaderStage::VERTEX);
	const auto fragment = shader->stage(fcg::ShaderStage::FRAGMENT);
	require(vertex.has_value() && fragment.has_value(), "Missing graphics stages");
	const fcg::ShaderResources resources {.uniformBuffers = 1, .storageBuffers = 1};
	auto *vs = device.createShader(fcg::ShaderStage::VERTEX, vertex->spirv, resources);
	auto *fs = device.createShader(fcg::ShaderStage::FRAGMENT, fragment->spirv, resources);
	if (!vs || !fs) {
		SDL_ReleaseGPUShader(device.handle(), vs);
		SDL_ReleaseGPUShader(device.handle(), fs);
		throw std::runtime_error("Creating graphics storage shaders failed");
	}
	const SDL_GPUVertexBufferDescription descriptions[] {
		{.slot = 0, .pitch = 2 * sizeof(float), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX},
		{.slot = 1, .pitch = 2 * sizeof(float), .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE}
	};
	const SDL_GPUVertexAttribute attributes[] {
		{.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0},
		{.location = 1, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0}
	};
	const SDL_GPUColorTargetDescription target {.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
	const SDL_GPUGraphicsPipelineCreateInfo pipelineInfo {
		.vertex_shader = vs, .fragment_shader = fs,
		.vertex_input_state = {.vertex_buffer_descriptions = descriptions, .num_vertex_buffers = 2,
			.vertex_attributes = attributes, .num_vertex_attributes = 2},
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.target_info = {.color_target_descriptions = &target, .num_color_targets = 1}
	};
	Pipelines pipelines{device};
	pipelines.graphics = SDL_CreateGPUGraphicsPipeline(device.handle(), &pipelineInfo);
	SDL_ReleaseGPUShader(device.handle(), vs);
	SDL_ReleaseGPUShader(device.handle(), fs);
	require(pipelines.graphics, SDL_GetError());
	const SDL_GPUTextureCreateInfo textureInfo {
		.type = SDL_GPU_TEXTURETYPE_2D, .format = target.format, .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
		.width = 8, .height = 4, .layer_count_or_depth = 1, .num_levels = 1
	};
	pipelines.color = SDL_CreateGPUTexture(device.handle(), &textureInfo);
	require(pipelines.color, SDL_GetError());
	const std::array<float, 8> positions {-1, -1, 0, -1, 0, 1, -1, 1};
	const std::array<float, 4> instances {0, 0, 1, 0};
	const std::array<Uint16, 6> indices {0, 1, 2, 0, 2, 3};
	const std::array<float, 4> zero {0, 0, 0, 0}, one {1, 1, 1, 1};
	auto vertices = take(fcg::Buffer::create(device, std::span(positions), SDL_GPU_BUFFERUSAGE_VERTEX));
	auto offsets = take(fcg::Buffer::create(device, std::span(instances), SDL_GPU_BUFFERUSAGE_VERTEX));
	auto index = take(fcg::Buffer::create(device, std::span(indices), SDL_GPU_BUFFERUSAGE_INDEX));
	auto vsStorage = take(fcg::Buffer::create(device, std::span(zero), SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	auto fsStorage = take(fcg::Buffer::create(device, std::span(one), SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	const SDL_GPUIndexedIndirectDrawCommand args {
		.num_indices = 6, .num_instances = 2, .first_index = 0, .vertex_offset = 0, .first_instance = 0
	};
	auto indirect = take(fcg::Buffer::create(device, std::span(&args, 1), SDL_GPU_BUFFERUSAGE_INDIRECT));
	// A padded row pitch also works on Metal's stricter texture-to-buffer copy alignment.
	constexpr Uint32 rowPixels = 64;
	auto download = take(fcg::TransferBuffer::create(device, rowPixels * 4 * 4, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD));
	for (unsigned draw = 0; draw < 2; ++draw) {
		Commands commands(device);
		const SDL_GPUColorTargetInfo color {.texture = pipelines.color, .clear_color = {0, 0, 0, 1},
			.load_op = SDL_GPU_LOADOP_CLEAR, .store_op = SDL_GPU_STOREOP_STORE};
		commands.render = SDL_BeginGPURenderPass(commands.handle, &color, 1, nullptr);
		require(commands.render, SDL_GetError());
		SDL_BindGPUGraphicsPipeline(commands.render, pipelines.graphics);
		check(vertices.bindVertex(commands.render, 0));
		check(offsets.bindVertex(commands.render, 1));
		check(index.bindIndex(commands.render, SDL_GPU_INDEXELEMENTSIZE_16BIT));
		fails(index.bindIndex(commands.render, SDL_GPU_INDEXELEMENTSIZE_16BIT, 1), fcg::BufferErrorCode::InvalidArgument);
		check(vsStorage.bindStorage(commands.render, fcg::ShaderStage::VERTEX));
		check(fsStorage.bindStorage(commands.render, fcg::ShaderStage::FRAGMENT));
		check(fcg::pushUniforms(commands.handle, fcg::ShaderStage::VERTEX, 0, std::as_bytes(std::span(zero))));
		const std::array<float, 4> material {draw == 0 ? 1.f : 0.f, draw == 1 ? 1.f : 0.f, 0, 1};
		check(fcg::pushUniforms(commands.handle, fcg::ShaderStage::FRAGMENT, 0, material));
		if (draw)
			SDL_DrawGPUIndexedPrimitivesIndirect(commands.render, indirect.handle(), 0, 1);
		else
			SDL_DrawGPUIndexedPrimitives(commands.render, 6, 2, 0, 0, 0);
		commands.end();
		const SDL_GPUTextureRegion region {.texture = pipelines.color, .w = 8, .h = 4, .d = 1};
		const SDL_GPUTextureTransferInfo destination {
			.transfer_buffer = download.handle(), .pixels_per_row = rowPixels, .rows_per_layer = 4
		};
		SDL_DownloadFromGPUTexture(commands.beginCopy(), &region, &destination);
		auto *fence = commands.submitFence();
		const bool waited = SDL_WaitForGPUFences(device.handle(), true, &fence, 1);
		SDL_ReleaseGPUFence(device.handle(), fence);
		require(waited, SDL_GetError());
		auto mapping = take(download.map());
		for (unsigned y = 0; y < 4; ++y) {
			for (unsigned x = 0; x < 8; ++x) {
				const auto *pixel = mapping.data().data() + (y * rowPixels + x) * 4;
				require(pixel[draw] == std::byte{255} && pixel[1 - draw] == std::byte{0}
					&& pixel[2] == std::byte{0} && pixel[3] == std::byte{255}, "Indexed instanced/storage draw produced wrong pixels");
			}
		}
	}
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Execute GPU tests with the same device creation policy as the framework.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	int result = 0;
	try {
		auto device = fcg::Device::create();
		require(device.has_value(), "Creating test GPU device failed");
		transfers(*device);
		compute(*device);
		graphics(*device);
		std::cout << "Buffer transfers, compute results, and rendered pixels verified\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
