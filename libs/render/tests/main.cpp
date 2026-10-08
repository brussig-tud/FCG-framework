
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

// GLM library
#include <glm/gtx/quaternion.hpp>

// SDL3 library and shader translation
#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

// FCG Framework and embedded resources
#include <FCG/player.h>
#include <FCG/window.h>
#include <FCG/res.h>
#include <FCG/Render/quad_renderer.h>
#include <FCG/Render/box_renderer.h>
#include <fcg-render-shaders.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

using A = fcg::Attribute;
using Update = fcg::PrimitiveAttributes::Update;

/// Detect whether a constant assignment is available.
template <A Attribute> concept Constant = requires (Update &update, typename fcg::AttributeTraits<Attribute>::Value value) {
	update.template set<Attribute>(value);
};
static_assert(!Constant<A::Position>);
static_assert(Constant<A::Normal> && Constant<A::Radius> && Constant<A::Extent>
	&& Constant<A::Orientation> && Constant<A::Color>);
static_assert(!std::is_copy_constructible_v<fcg::PrimitiveAttributes>);
static_assert(!std::is_move_constructible_v<fcg::PrimitiveAttributes>);
static_assert(!std::is_move_constructible_v<Update>);
static_assert(!std::is_copy_constructible_v<fcg::QuadRenderer> && std::is_nothrow_move_constructible_v<fcg::QuadRenderer>);
static_assert(!std::is_copy_constructible_v<fcg::BoxRenderer> && std::is_nothrow_move_constructible_v<fcg::BoxRenderer>);

/// Assertions also run in release builds.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Report owned diagnostics from either Core or Render.
template <class T, class Error> auto take (std::expected<T, Error> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Report errors for operations without returned values.
template <class Error> void check (std::expected<void, Error> result) {
	if (!result)
		throw std::runtime_error(result.error().message);
}

/// Assert a recoverable error category.
template <class T> void fails (const std::expected<T, fcg::RenderError> &result, fcg::RenderErrorCode code) {
	require(!result && result.error().code == code, "Wrong Render error category");
}

/// Compare actual GPU bytes to the original typed objects, including padding.
template <A Attribute, class T, std::size_t N>
void verifyBytes (const fcg::PrimitiveAttributes &attributes, std::span<T, N> expected)
{
	const auto source = attributes.source<Attribute>();
	const auto view = std::get<fcg::AttributeBufferView<Attribute>>(source);
	require(view.count == expected.size() && view.stride == sizeof(T), "Wrong uploaded view layout");
	if (expected.empty())
		return;
	auto ticket = take(view.buffer->readback(view.offset, expected.size_bytes()));
	check(ticket.wait());
	auto mapped = take(ticket.map());
	require(std::memcmp(mapped.data().data(), expected.data(), expected.size_bytes()) == 0, "Typed bytes changed in staging");
}

/// Scoped command ownership with cancellation on failure.
class Commands
{
public:

	////
	// Object construction/destruction

	/// Acquire on the current thread.
	explicit Commands(fcg::Device &device) : handle(SDL_AcquireGPUCommandBuffer(device.handle())) {
		require(handle, SDL_GetError());
	}

	/// Cancel unused commands, ending any copy pass first.
	~Commands() {
		endCopy();
		if (handle)
			SDL_CancelGPUCommandBuffer(handle);
	}

	/// Commands have one owner.
	Commands(const Commands&) = delete;

	/// Commands cannot be reassigned.
	auto operator= (const Commands&) -> Commands& = delete;


	////
	// Methods

	/// Begin a copy pass.
	auto beginCopy () -> SDL_GPUCopyPass* {
		copy = SDL_BeginGPUCopyPass(handle);
		require(copy, SDL_GetError());
		return copy;
	}

	/// End an active copy pass.
	void endCopy () {
		if (copy)
			SDL_EndGPUCopyPass(std::exchange(copy, nullptr));
	}

	/// Submit without waiting.
	void submit () {
		endCopy();
		require(SDL_SubmitGPUCommandBuffer(std::exchange(handle, nullptr)), SDL_GetError());
	}


	////
	// Fields

	/// Owned command handle.
	SDL_GPUCommandBuffer *handle;

	/// Active copy pass, if any.
	SDL_GPUCopyPass *copy = nullptr;
};

/// All six types, final-assignment semantics, failures, recursion, and retained storage.
void attributes (fcg::Device &device, fcg::Device &other)
{
	fcg::PrimitiveAttributes data(device);
	require(&data.device() == &device && data.instanceCount() == 0, "New collection is not empty");
	require(std::holds_alternative<std::monostate>(data.source<A::Color>()), "New collection has a color");
	std::array positions{glm::vec4(-.5f, 0, .5f, 1), glm::vec4(.5f, 0, .5f, 1)};
	std::array normals{glm::vec3(1,2,3), glm::vec3(4,5,6)};
	std::array radii{1.f, 2.f};
	std::array extents{glm::vec3(.25f), glm::vec3(.3f)};
	std::array rotations{glm::quat(1,0,0,0), glm::quat(2,3,4,5)};
	std::array colors{glm::vec4(1,0,0,1), glm::vec4(0,1,0,1)};
	check(data.setAttributes([&] (Update &update)
	{
		update.set<A::Position>(std::span(positions));
		update.set<A::Normal>(std::span(normals));
		update.set<A::Radius>(std::span(radii));
		update.set<A::Extent>(std::span(extents));
		update.set<A::Orientation>(std::span(rotations));
		update.set<A::Color>(std::span(colors));
	}));
	verifyBytes<A::Position>(data, std::span(positions));
	verifyBytes<A::Normal>(data, std::span(normals));
	verifyBytes<A::Radius>(data, std::span(radii));
	verifyBytes<A::Extent>(data, std::span(extents));
	verifyBytes<A::Orientation>(data, std::span(rotations));
	verifyBytes<A::Color>(data, std::span(colors));
	const auto positionView = std::get<fcg::AttributeBufferView<A::Position>>(data.source<A::Position>());
	const auto *owner = positionView.buffer;
	check(data.setAttributes([&] (Update &update)
	{
		update.clear<A::Position>();
		update.set<A::Position>(std::span(positions).first(1));
		update.set<A::Normal>(glm::vec3(1));
		update.set<A::Radius>(3.f);
		update.set<A::Extent>(glm::vec3(1));
		update.set<A::Orientation>(glm::quat(1,2,3,4));
		update.set<A::Color>(glm::vec4(1));
	}));
	require(data.instanceCount() == 1, "Position did not determine the instance count");
	require(std::get<fcg::AttributeBufferView<A::Position>>(data.source<A::Position>()).buffer == owner,
		"Smaller replacement did not reuse allocation");
	require(std::get<glm::quat>(data.source<A::Orientation>()) == glm::quat(1,2,3,4), "Quaternion constant order changed");
	require(std::get<float>(data.source<A::Radius>()) == 3.f, "Scalar constant changed");
	check(data.setAttributes([] (Update&) {}));
	require(data.instanceCount() == 1, "Omitted attribute was not retained");

	// Validation failures happen before recording and preserve all previous sources.
	const auto invalid = fcg::RenderErrorCode::InvalidArgument;
	auto foreign = take(fcg::Buffer::create(other, 64, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	auto vertex = take(fcg::Buffer::create(device, 64, SDL_GPU_BUFFERUSAGE_VERTEX));
	auto storage = take(fcg::Buffer::create(device, 64, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	for (const auto view : std::array<fcg::AttributeBufferView<A::Position>, 8>{{
		{&foreign,0,16,1}, {&vertex,0,16,1}, {nullptr,0,16,1}, {&storage,1,16,1},
		{&storage,0,15,1}, {&storage,0,12,1}, {&storage,0,16,5},
		{&storage,0,16,std::numeric_limits<std::size_t>::max()}
	}})
		fails(data.setAttributes([&] (Update &update) { update.bind<A::Position>(view); }), invalid);
	fails(data.setAttributes([&] (Update &update) {
		update.set<A::Position>(std::span(positions));
		update.set<A::Extent>(glm::vec3(-1));
	}), invalid);
	fails(data.setAttributes([] (Update &update) { update.set<A::Orientation>(glm::quat(0,0,0,0)); }), invalid);
	fails(data.setAttributes([] (Update &update) {
		update.set<A::Extent>(glm::vec3(std::numeric_limits<float>::infinity()));
	}), invalid);
	require(data.instanceCount() == 1, "Failed update changed metadata");
	verifyBytes<A::Position>(data, std::span(positions).first(1));
	bool caught = false;
	try {
		(void)data.setAttributes([&] (Update &update) {
			update.set<A::Position>(std::span(positions));
			throw std::runtime_error("callback exception");
		});
	} catch (const std::runtime_error&) {
		caught = true;
	}
	require(caught && data.instanceCount() == 1, "Callback exception did not preserve metadata");
	check(data.setAttributes([&] (Update&) {
		fails(data.setAttributes([] (Update&) {}), fcg::RenderErrorCode::InvalidState);
	}));
	fails(data.setAttributes((SDL_GPUCopyPass*)nullptr, [] (Update&) {}), invalid);

	// Both last-assignment directions discard superseded descriptors, including invalid values.
	check(data.setAttributes([&] (Update &update)
	{
		update.set<A::Extent>(glm::vec3(-1));
		update.set<A::Extent>(glm::vec3(1));
		update.bind<A::Position>({&foreign,0,16,1});
		update.set<A::Position>(std::span(positions));
		update.set<A::Color>(std::span(colors));
		update.set<A::Color>(glm::vec4(1));
	}));
	std::array largePositions{positions[0], positions[1], positions[0], positions[1], positions[0]};
	check(data.setAttributes([&] (Update &update) { update.set<A::Position>(std::span(largePositions)); }));
	const auto *grownOwner = std::get<fcg::AttributeBufferView<A::Position>>(data.source<A::Position>()).buffer;
	require(grownOwner != owner && data.instanceCount() == 5, "Growing array did not grow capacity");
	verifyBytes<A::Position>(data, std::span(largePositions));
	Commands commands(device);
	check(data.setAttributes(commands.beginCopy(), [&] (Update &update) {
		update.set<A::Position>(std::span(positions));
		update.set<A::Color>(std::span(colors));
	}));
	commands.submit();
	verifyBytes<A::Color>(data, std::span(colors));
	check(data.setAttributes([] (Update &update) { update.clear<A::Position>(); }));
	require(data.instanceCount() == 0, "Cleared position retained instances");
	check(data.setAttributes([] (Update &update) { update.set<A::Position>(std::span<const glm::vec4>{}); }));
	require(data.instanceCount() == 0, "Empty position retained instances");
	check(data.setAttributes([&] (Update &update) { update.set<A::Position>(std::span(positions)); }));
	require(std::get<fcg::AttributeBufferView<A::Position>>(data.source<A::Position>()).buffer == grownOwner,
		"Clearing did not retain GPU capacity");
}

/// Scoped offscreen attachment ownership.
class Canvas
{
public:

	////
	// Object construction/destruction

	/// Create a small RGBA target with optional depth.
	explicit Canvas(fcg::Device &device, bool withDepth=false) : device(device)
	{
		SDL_GPUTextureCreateInfo info{};
		info.type = SDL_GPU_TEXTURETYPE_2D;
		info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		info.width = side;
		info.height = side;
		info.layer_count_or_depth = 1;
		info.num_levels = 1;
		info.sample_count = SDL_GPU_SAMPLECOUNT_1;
		color = SDL_CreateGPUTexture(device.handle(), &info);
		require(color, SDL_GetError());
		if (withDepth) {
			info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
			info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
			depth = SDL_CreateGPUTexture(device.handle(), &info);
			require(depth, SDL_GetError());
		}
	}

	/// Retire textures safely after their submitted use.
	~Canvas() {
		if (depth)
			SDL_ReleaseGPUTexture(device.handle(), depth);
		SDL_ReleaseGPUTexture(device.handle(), color);
	}


	////
	// Accessors

	/// Matching pipeline description.
	auto target () const -> fcg::RenderTargetInfo {
		return {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			depth ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : SDL_GPU_TEXTUREFORMAT_INVALID, SDL_GPU_SAMPLECOUNT_1};
	}


	////
	// Fields

	/// Pixel dimension.
	static constexpr Uint32 side = 64;

	/// Borrowed device.
	fcg::Device &device;

	/// Owned color target.
	SDL_GPUTexture *color = nullptr;

	/// Optional owned depth target.
	SDL_GPUTexture *depth = nullptr;
};

/// Submitted screenshot whose mapping waits only when the test explicitly inspects it.
class Snapshot
{
public:

	////
	// Object construction/destruction

	/// Submit a draw and download in the same command buffer.
	Snapshot(
		Canvas &canvas, const fcg::PrimitiveRenderer &renderer, const fcg::PrimitiveAttributes &data,
		fcg::RenderState &state, const fcg::DrawOptions &options={}, fcg::InstanceRange range={},
		SDL_FColor clear={0,0,0,0}
	) : device(canvas.device), transfer(take(fcg::TransferBuffer::create(device,
		Canvas::side * Canvas::side * 4, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD)))
	{
		Commands commands(device);
		SDL_GPUColorTargetInfo color{};
		color.texture = canvas.color;
		color.clear_color = clear;
		color.load_op = SDL_GPU_LOADOP_CLEAR;
		color.store_op = SDL_GPU_STOREOP_STORE;
		color.cycle = true;
		SDL_GPUDepthStencilTargetInfo depth{};
		depth.texture = canvas.depth;
		depth.clear_depth = 1.f;
		depth.load_op = SDL_GPU_LOADOP_CLEAR;
		depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
		depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
		depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
		depth.cycle = true;
		auto *pass = SDL_BeginGPURenderPass(commands.handle, &color, 1, canvas.depth ? &depth : nullptr);
		require(pass, SDL_GetError());
		const auto result = renderer.draw(data, state, commands.handle, pass, options, range);
		SDL_EndGPURenderPass(pass);
		check(result);
		auto *copy = commands.beginCopy();
		const SDL_GPUTextureRegion source{canvas.color,0,0,0,0,0,Canvas::side,Canvas::side,1};
		const SDL_GPUTextureTransferInfo destination{transfer.handle(),0,Canvas::side,Canvas::side};
		SDL_DownloadFromGPUTexture(copy, &source, &destination);
		commands.endCopy();
		fence = SDL_SubmitGPUCommandBufferAndAcquireFence(std::exchange(commands.handle, nullptr));
		require(fence, SDL_GetError());
	}

	/// Finish test-owned downloads before releasing their fence.
	~Snapshot() {
		if (fence) {
			SDL_WaitForGPUFences(device.handle(), true, &fence, 1);
			SDL_ReleaseGPUFence(device.handle(), fence);
		}
	}


	////
	// Methods

	/// Compare RGBA at projected coordinates, allowing UNORM rounding.
	void pixel (float x, float y, std::array<int, 4> expected)
	{
		require(SDL_WaitForGPUFences(device.handle(), true, &fence, 1), SDL_GetError());
		auto mapping = take(transfer.map());
		const auto column = (Uint32)((x + 1.f) * .5f * Canvas::side);
		const auto row = (Uint32)((1.f - y) * .5f * Canvas::side);
		const auto *bytes = (const Uint8*)mapping.data().data() + (row * Canvas::side + column) * 4;
		for (std::size_t i = 0; i < 4; ++i)
			if (std::abs((int)bytes[i] - expected[i]) > 3)
				throw std::runtime_error("Pixel mismatch at (" + std::to_string(x) + "," + std::to_string(y)
					+ "), channel " + std::to_string(i) + ": " + std::to_string(bytes[i])
					+ " expected " + std::to_string(expected[i]));
	}


	////
	// Fields

	/// Borrowed device.
	fcg::Device &device;

	/// Owning download storage.
	fcg::TransferBuffer transfer;

	/// Submission fence.
	SDL_GPUFence *fence = nullptr;
};

/// Caller-created texture and sampler with identifiable UV quadrants.
class Texture
{
public:

	////
	// Object construction/destruction

	/// Upload a two-by-two nearest-filtered RGBA texture.
	explicit Texture(fcg::Device &device) : device(device)
	{
		SDL_GPUTextureCreateInfo info{};
		info.type = SDL_GPU_TEXTURETYPE_2D;
		info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		info.width = 2;
		info.height = 2;
		info.layer_count_or_depth = 1;
		info.num_levels = 1;
		texture = SDL_CreateGPUTexture(device.handle(), &info);
		require(texture, SDL_GetError());
		SDL_GPUSamplerCreateInfo settings{};
		settings.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		settings.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		sampler = SDL_CreateGPUSampler(device.handle(), &settings);
		require(sampler, SDL_GetError());
		const std::array<Uint8, 16> pixels{255,0,0,128, 0,255,0,128, 0,0,255,128, 255,255,0,128};
		auto staging = take(fcg::TransferBuffer::create(device, pixels.size(), SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD));
		{
			auto mapping = take(staging.map());
			std::memcpy(mapping.data().data(), pixels.data(), pixels.size());
		}
		Commands commands(device);
		const SDL_GPUTextureTransferInfo source{staging.handle(),0,2,2};
		const SDL_GPUTextureRegion destination{texture,0,0,0,0,0,2,2,1};
		SDL_UploadToGPUTexture(commands.beginCopy(), &source, &destination, false);
		commands.submit();
	}

	/// Retire caller-owned sampling resources.
	~Texture() {
		SDL_ReleaseGPUSampler(device.handle(), sampler);
		SDL_ReleaseGPUTexture(device.handle(), texture);
	}


	////
	// Fields

	/// Borrowed device.
	fcg::Device &device;

	/// Owned sampled texture.
	SDL_GPUTexture *texture = nullptr;

	/// Owned sampler.
	SDL_GPUSampler *sampler = nullptr;
};

/// Offscreen geometry, winding, shared collections, lighting, texture seams, and ranges.
void pixels (fcg::Device &device)
{
	Canvas canvas(device);
	fcg::RenderState state(device);
	auto quad = take(fcg::QuadRenderer::create(device, canvas.target()));
	auto culled = take(fcg::QuadRenderer::create(device, canvas.target(), {.cullMode = SDL_GPU_CULLMODE_BACK}));
	auto box = take(fcg::BoxRenderer::create(device, canvas.target()));
	require(quad.supports(A::Position) && quad.supports(A::Color) && quad.supports(A::Extent)
		&& quad.supports(A::Orientation) && !quad.supports(A::Radius) && !quad.supports(A::Normal), "Wrong support list");
	fcg::PrimitiveAttributes data(device);
	std::array center{glm::vec4(0,0,.5f,1)};
	check(data.setAttributes([&] (Update &update) {
		update.set<A::Position>(std::span(center));
		update.set<A::Extent>(glm::vec3(.5f,.5f,.1f));
	}));
	Snapshot initial(canvas, culled, data, state);
	initial.pixel(-.3f,-.3f,{255,255,255,255});
	initial.pixel(.3f,.3f,{255,255,255,255});
	initial.pixel(.7f,0,{0,0,0,0});
	Snapshot solid(canvas, box, data, state);
	solid.pixel(0,0,{255,255,255,255});

	// Homogeneous division, anisotropic scaling, quaternion order and normalization, Core matrices.
	center[0] = glm::vec4(.8f,0,1,2);
	check(data.setAttributes([&] (Update &update) {
		update.set<A::Position>(std::span(center));
		update.set<A::Extent>(glm::vec3(.3f,.1f,9));
		update.set<A::Orientation>(glm::angleAxis(glm::radians(90.f), glm::vec3(0,0,1)) * 7.f);
		update.set<A::Color>(glm::vec4(1,0,0,.5f));
	}));
	Snapshot transformed(canvas, quad, data, state);
	transformed.pixel(.4f,.2f,{255,0,0,128});
	transformed.pixel(.6f,0,{0,0,0,0});
	state.loadModelviewMatrix(glm::translate(glm::mat4(1), glm::vec3(-.4f,0,0)));
	Snapshot viewed(canvas, quad, data, state);
	viewed.pixel(0,.2f,{255,0,0,128});
	state.loadModelviewMatrix(glm::mat4(1));

	center[0] = glm::vec4(0,0,.5f,1);
	std::array<glm::vec3, 1> ignoredNormal{glm::vec3(0)};
	std::array<float, 0> ignoredRadius{};
	check(data.setAttributes([&] (Update &update)
	{
		update.set<A::Position>(std::span(center));
		update.set<A::Extent>(glm::vec3(.5f,.5f,.1f));
		update.clear<A::Orientation>();
		update.clear<A::Color>();
		update.set<A::Normal>(std::span(ignoredNormal));
		update.set<A::Radius>(std::span(ignoredRadius));
	}));
	Texture texture(device);
	fcg::DrawOptions textured{.texture = fcg::PrimitiveTexture{texture.texture, texture.sampler}};
	Snapshot uv(canvas, quad, data, state, textured);
	uv.pixel(-.3f,-.3f,{255,0,0,128});
	uv.pixel(.3f,-.3f,{0,255,0,128});
	uv.pixel(-.3f,.3f,{0,0,255,128});
	uv.pixel(.3f,.3f,{255,255,0,128});
	Snapshot boxUV(canvas, box, data, state, textured);
	boxUV.pixel(-.3f,-.3f,{255,0,0,128});
	boxUV.pixel(.3f,.3f,{255,255,0,128});

	fcg::DrawOptions light;
	light.lighting = {.enabled = true, .direction = {0,0,1}, .ambient = {.1f,.2f,.3f}, .diffuse = {.2f,.3f,.4f}};
	Snapshot lit(canvas, quad, data, state, light);
	lit.pixel(0,0,{77,128,179,255});
	check(data.setAttributes([] (Update &update) {
		update.set<A::Orientation>(glm::angleAxis(glm::radians(180.f), glm::vec3(0,1,0)));
	}));
	Snapshot back(canvas, quad, data, state, light);
	back.pixel(0,0,{77,128,179,255});
	Snapshot hidden(canvas, culled, data, state);
	hidden.pixel(-.3f,-.3f,{0,0,0,0});
	hidden.pixel(.3f,.3f,{0,0,0,0});
	check(data.setAttributes([] (Update &update) { update.clear<A::Orientation>(); }));

	// Every box face must preserve its specified U/V directions after orientation.
	const std::array<glm::vec3, 6> faceNormals{{{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}}};
	const std::array<glm::vec3, 6> faceU{{{0,0,-1}, {0,0,1}, {1,0,0}, {1,0,0}, {1,0,0}, {-1,0,0}}};
	const std::array<glm::vec3, 6> faceV{{{0,1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {0,1,0}, {0,1,0}}};
	for (std::size_t face = 0; face < 6; ++face)
	{
		const auto orientation = glm::rotation(faceNormals[face], glm::vec3(0,0,1));
		check(data.setAttributes([&] (Update &update) {
			update.set<A::Extent>(glm::vec3(.25f));
			update.set<A::Orientation>(orientation);
		}));
		Snapshot faceUV(canvas, box, data, state, textured);
		for (unsigned corner = 0; corner < 4; ++corner)
		{
			const float u = corner % 2 ? .15f : -.15f;
			const float v = corner / 2 ? .15f : -.15f;
			const auto point = orientation * (faceNormals[face] * .25f + faceU[face] * u + faceV[face] * v);
			const std::array<std::array<int,4>,4> expected{{{255,0,0,128}, {0,255,0,128}, {0,0,255,128}, {255,255,0,128}}};
			faceUV.pixel(point.x, point.y, expected[corner]);
		}
		Snapshot normal(canvas, box, data, state, light);
		normal.pixel(0,0,{77,128,179,255});
	}

	// Normal-matrix transformation and orientation both affect lighting.
	const auto tilted = glm::angleAxis(glm::radians(45.f), glm::vec3(0,1,0));
	check(data.setAttributes([&] (Update &update) {
		update.set<A::Extent>(glm::vec3(.3f));
		update.set<A::Orientation>(tilted);
	}));
	state.loadModelviewMatrix(glm::scale(glm::mat4(1), glm::vec3(1.2f,.8f,.7f)));
	const auto transformedNormal = glm::normalize(state.normalMatrix() * (tilted * glm::vec3(0,0,1)));
	const auto intensity = light.lighting.ambient + light.lighting.diffuse * transformedNormal.z;
	Snapshot normalMatrix(canvas, quad, data, state, light);
	normalMatrix.pixel(0,0,{(int)std::lround(255 * intensity.r), (int)std::lround(255 * intensity.g),
		(int)std::lround(255 * intensity.b), 255});
	state.loadModelviewMatrix(glm::mat4(1));
	check(data.setAttributes([] (Update &update) { update.clear<A::Orientation>(); }));

	Canvas cullingCanvas(device, true);
	auto defaultBox = take(fcg::BoxRenderer::create(device, cullingCanvas.target()));
	auto twoSidedBox = take(fcg::BoxRenderer::create(device, cullingCanvas.target(), {.cullMode = SDL_GPU_CULLMODE_NONE}));
	Snapshot defaultCulling(cullingCanvas, defaultBox, data, state, light);
	defaultCulling.pixel(0,0,{77,128,179,255});
	Snapshot explicitCulling(cullingCanvas, twoSidedBox, data, state, light);
	explicitCulling.pixel(0,0,{26,51,77,255});

	// Borrowed interleaved position/color/extent/orientation with padding and arbitrary supported stride.
	struct Record
	{
		////
		// Fields

		/// Deliberate leading padding.
		glm::vec4 padding;
		/// Homogeneous point.
		glm::vec4 position;
		/// Half sizes.
		glm::vec3 extent;
		/// Quaternion in native object order.
		glm::quat rotation;
		/// RGBA color.
		glm::vec4 color;
	};
	std::array<Record, 2> records{{
		{{}, {-.5f,0,.5f,1}, {.2f,.3f,.1f}, glm::quat(1,0,0,0), {1,0,0,1}},
		{{}, {.5f,0,.5f,1}, {.2f,.3f,.1f}, glm::angleAxis(glm::radians(90.f), glm::vec3(0,0,1)) * 2.f, {0,1,0,1}}
	}};
	auto buffer = take(fcg::Buffer::create(device, std::span(records), SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	check(data.setAttributes([&] (Update &update) {
		update.bind<A::Position>({&buffer,offsetof(Record,position),sizeof(Record),2});
		update.bind<A::Extent>({&buffer,offsetof(Record,extent),sizeof(Record),2});
		update.bind<A::Orientation>({&buffer,offsetof(Record,rotation),sizeof(Record),2});
		update.bind<A::Color>({&buffer,offsetof(Record,color),sizeof(Record),2});
	}));
	Snapshot all(canvas, quad, data, state);
	all.pixel(-.5f,0,{255,0,0,255});
	all.pixel(.5f,0,{0,255,0,255});
	all.pixel(.75f,0,{0,255,0,255});
	all.pixel(.5f,.25f,{0,0,0,0});
	Snapshot second(canvas, box, data, state, {}, {.first = 1});
	second.pixel(-.5f,0,{0,0,0,0});
	second.pixel(.5f,0,{0,255,0,255});
	Snapshot none(canvas, quad, data, state, {}, {.first = 2});
	none.pixel(.5f,0,{0,0,0,0});
	Commands validation(device);
	const auto invalid = fcg::RenderErrorCode::InvalidArgument;
	fails(quad.draw(data, state, validation.handle, (SDL_GPURenderPass*)nullptr), invalid);

	// Real active passes permit testing rejection before any invalid SDL calls.
	SDL_GPUColorTargetInfo attachment{};
	attachment.texture = canvas.color;
	attachment.load_op = SDL_GPU_LOADOP_CLEAR;
	attachment.store_op = SDL_GPU_STOREOP_STORE;
	auto *pass = SDL_BeginGPURenderPass(validation.handle, &attachment, 1, nullptr);
	require(pass, SDL_GetError());
	fails(quad.draw(data, state, validation.handle, pass, {}, {.first = 3}), invalid);
	fails(quad.draw(data, state, validation.handle, pass, {}, {.first = 1, .count = 2}), invalid);
	check(data.setAttributes([] (Update &update) { update.set<A::Color>(std::span<const glm::vec4>{}); }));
	fails(quad.draw(data, state, validation.handle, pass), invalid);
	check(quad.draw(data, state, validation.handle, pass, {}, {.count = 0}));
	std::array oneColor{glm::vec4(1)};
	check(data.setAttributes([&] (Update &update) { update.set<A::Color>(std::span(oneColor)); }));
	check(quad.draw(data, state, validation.handle, pass, {}, {.count = 1}));
	fails(quad.draw(data, state, validation.handle, pass), invalid);
	fcg::PrimitiveAttributes empty(device);
	check(quad.draw(empty, state, validation.handle, pass));
	fcg::DrawOptions badLight;
	badLight.lighting.enabled = true;
	badLight.lighting.direction = glm::vec3(0);
	fails(quad.draw(data, state, validation.handle, pass, badLight), invalid);
	fails(quad.draw(data, state, validation.handle, pass,
		{.texture = fcg::PrimitiveTexture{texture.texture, nullptr}}), invalid);
	SDL_EndGPURenderPass(pass);
	validation.submit();

	// Replacements are cycled while screenshots from earlier submissions remain in flight.
	std::vector<std::unique_ptr<Snapshot>> snapshots;
	std::array cyclingColor{glm::vec4(1)};
	for (unsigned i = 0; i < 8; ++i)
	{
		center[0] = glm::vec4(0,0,.5f,1);
		check(data.setAttributes([&] (Update &update) {
			update.set<A::Position>(std::span(center));
			update.set<A::Extent>(glm::vec3(.5f));
			update.clear<A::Orientation>();
			cyclingColor[0] = glm::vec4((float)i / 7.f, 0, 1, 1);
			update.set<A::Color>(std::span(cyclingColor));
		}));
		snapshots.push_back(std::make_unique<Snapshot>(canvas, quad, data, state));
	}
	for (unsigned i = 0; i < snapshots.size(); ++i)
		snapshots[i]->pixel(0,0,{(int)std::lround(255.f * i / 7.f),0,255,255});

	auto blend = take(fcg::QuadRenderer::create(device, canvas.target(), {.alphaBlending = true}));
	check(data.setAttributes([] (Update &update) { update.set<A::Color>(glm::vec4(1,0,0,.5f)); }));
	Snapshot blended(canvas, blend, data, state, {}, {}, {0,0,1,1});
	blended.pixel(0,0,{128,0,128,255});
	Snapshot textureAlpha(canvas, quad, data, state, textured);
	textureAlpha.pixel(-.3f,-.3f,{255,0,0,64});

	check(data.setAttributes([] (Update &update) { update.set<A::Extent>(glm::vec3(0)); }));
	Snapshot degenerate(canvas, quad, data, state);
	degenerate.pixel(0,0,{0,0,0,0});
	check(data.setAttributes([] (Update &update) { update.set<A::Extent>(glm::vec3(.5f)); }));
	Canvas depthCanvas(device, true);
	auto depthQuad = take(fcg::QuadRenderer::create(device, depthCanvas.target()));
	Snapshot depthDraw(depthCanvas, depthQuad, data, state);
	depthDraw.pixel(0,0,{255,0,0,128});
	std::array overlapping{glm::vec4(0,0,.3f,1), glm::vec4(0,0,.7f,1)};
	std::array depthColors{glm::vec4(1,0,0,1), glm::vec4(0,1,0,1)};
	check(data.setAttributes([&] (Update &update) {
		update.set<A::Position>(std::span(overlapping));
		update.set<A::Color>(std::span(depthColors));
	}));
	Snapshot depthOrder(depthCanvas, depthQuad, data, state);
	depthOrder.pixel(0,0,{255,0,0,255});
	auto noTest = take(fcg::QuadRenderer::create(device, depthCanvas.target(), {.depthTest = false}));
	Snapshot noTestDraw(depthCanvas, noTest, data, state);
	noTestDraw.pixel(0,0,{0,255,0,255});
	auto noWrite = take(fcg::QuadRenderer::create(device, depthCanvas.target(), {.depthWrite = false}));
	Snapshot noWriteDraw(depthCanvas, noWrite, data, state);
	noWriteDraw.pixel(0,0,{0,255,0,255});
	auto always = take(fcg::QuadRenderer::create(device, depthCanvas.target(), {.depthCompare = SDL_GPU_COMPAREOP_ALWAYS}));
	Snapshot alwaysDraw(depthCanvas, always, data, state);
	alwaysDraw.pixel(0,0,{0,255,0,255});

	auto moved = std::move(depthQuad);
	Commands unused(device);
	fails(depthQuad.draw(data, state, unused.handle, (SDL_GPURenderPass*)nullptr), fcg::RenderErrorCode::InvalidState);
	Snapshot movedDraw(depthCanvas, moved, data, state);
	movedDraw.pixel(0,0,{255,0,0,255});
}

/// Resource reflection and MSL/HLSL translation exercise non-Vulkan shader contracts on Linux.
void shaders ()
{
	require(SDL_ShaderCross_Init(), SDL_GetError());
	for (const auto name : {"primitive", "textured"})
	{
		const auto shader = fcg::res::shader(fcg::render::embedded::FS, name);
		require(shader.has_value(), "Missing Render shader archive");
		for (const auto stage : {fcg::ShaderStage::VERTEX, fcg::ShaderStage::FRAGMENT})
		{
			const auto data = shader->stage(stage);
			require(data.has_value(), "Missing shader stage");
			const SDL_ShaderCross_SPIRV_Info info{
				.bytecode = (const Uint8*)data->spirv.data(), .bytecode_size = data->spirv.size(), .entrypoint = "main",
				.shader_stage = stage == fcg::ShaderStage::VERTEX ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT
			};
			auto *metadata = SDL_ShaderCross_ReflectGraphicsSPIRV(info.bytecode, info.bytecode_size, 0);
			require(metadata, SDL_GetError());
			const auto resources = metadata->resource_info;
			SDL_free(metadata);
			require(resources.num_storage_buffers == (stage == fcg::ShaderStage::VERTEX ? 4u : 0u)
				&& resources.num_uniform_buffers == (stage == fcg::ShaderStage::VERTEX ? 2u : 1u), "Wrong shader resources");
			auto *msl = SDL_ShaderCross_TranspileMSLFromSPIRV(&info);
			require(msl, SDL_GetError());
			SDL_free(msl);
			auto *hlsl = SDL_ShaderCross_TranspileHLSLFromSPIRV(&info);
			require(hlsl, SDL_GetError());
			SDL_free(hlsl);
		}
	}
	SDL_ShaderCross_Quit();
}

/// Missing, unclaimed, and claimed windows expose consistent target metadata before any frame.
void targets (fcg::Device &device)
{
	fails(fcg::QuadRenderer::create(device, {}), fcg::RenderErrorCode::InvalidArgument);
	fails(fcg::BoxRenderer::create(device,
		{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM}),
		fcg::RenderErrorCode::InvalidArgument);
	fcg::Player absent(device, nullptr, {});
	const auto &constantPlayer = absent;
	require(&absent.device() == &device && &constantPlayer.device() == &device, "Player changed its borrowed device");
	require(!absent.mainRenderTargetInfo(), "Windowless player has a target");
	fails(fcg::QuadRenderer::create(absent), fcg::RenderErrorCode::InvalidState);
	fails(fcg::BoxRenderer::create(absent), fcg::RenderErrorCode::InvalidState);
	auto window = fcg::Window::create({.width = 128, .height = 128});
	require(window != nullptr, SDL_GetError());
	fcg::Player player(device, window.get(), {});
	require(!window->renderTargetInfo() && !player.mainRenderTargetInfo(), "Unclaimed window has a target");
	fails(fcg::QuadRenderer::create(player), fcg::RenderErrorCode::InvalidState);
	require(device.claimWindow(window), SDL_GetError());
	const auto target = player.mainRenderTargetInfo();
	require(target && target->colorFormat == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
		&& target->depthStencilFormat == SDL_GPU_TEXTUREFORMAT_D32_FLOAT
		&& target->sampleCount == SDL_GPU_SAMPLECOUNT_1, "Incorrect pre-frame target");
	{
		auto quad = take(fcg::QuadRenderer::create(player));
		auto box = take(fcg::BoxRenderer::create(player, {.cullMode = SDL_GPU_CULLMODE_NONE}));
		require(quad.supports(A::Position) && box.supports(A::Position), "Player factories did not create renderers");
		require(SDL_MinimizeWindow(window->handle()), SDL_GetError());
		require(player.mainRenderTargetInfo().has_value(), "Minimized window lost target metadata");
	}
	device.unclaimWindow(window);
	require(!window->renderTargetInfo(), "Released window retained target metadata");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Execute GPU-backed attribute, shader, pixel, and target checks.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	int result = 0;
	try
	{
		shaders();
		auto device = fcg::Device::create();
		auto other = fcg::Device::create();
		require(device.has_value() && other.has_value(), SDL_GetError());
		attributes(*device, *other);
		pixels(*device);
		targets(*device);
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
