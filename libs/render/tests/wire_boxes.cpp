
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// GLM library
#include <glm/gtc/matrix_transform.hpp>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/texture.h>
#include <FCG/Render/line_renderer.h>
#include <FCG/Render/line_set_builder.h>
#include <FCG/Render/wire_box_renderer.h>
#include <FCG/Render/box_renderer.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Attribute shorthand for the test collections.
using A = fcg::Attribute;

static_assert(!std::is_copy_constructible_v<fcg::WireBoxRenderer>);
static_assert(std::is_nothrow_move_constructible_v<fcg::WireBoxRenderer>);
static_assert(std::is_nothrow_move_assignable_v<fcg::WireBoxRenderer>);

/// Fail independently of build-mode assertions.
void require (bool condition, const std::string &message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Preserve an operation's diagnostic when unwrapping its result.
template <class T, class E> auto take (std::expected<T, E> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Preserve the diagnostic of a void operation.
template <class E> void check (std::expected<void, E> result) {
	if (!result)
		throw std::runtime_error(result.error().message);
}

/// CPU-owned output image.
struct Image
{
	/// Physical dimensions of the output.
	glm::uvec2 size;

	/// Linear UNORM RGBA values in row-major order.
	std::vector<std::array<std::uint8_t, 4>> colors;

	/// Normalized depths, empty when not downloaded.
	std::vector<float> depths;
};

/// Offscreen targets and row-pitch-aware color/depth readback.
class Surface
{
public:

	////
	// Object construction

	/// Allocate a surface with optional depth and multisampling.
	Surface(
		fcg::Device &device, glm::uvec2 size={96, 64}, bool withDepth=true,
		SDL_GPUSampleCount samples=SDL_GPU_SAMPLECOUNT_1
	) : device(device), size(size), samples(samples), color(make(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, SDL_GPU_SAMPLECOUNT_1))
	{
		if (samples != SDL_GPU_SAMPLECOUNT_1)
			multisample.emplace(make(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
				SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, samples));
		if (withDepth)
			depth.emplace(make(SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, samples));
	}


	////
	// Accessors

	/// Pipeline-compatible attachment description.
	[[nodiscard]] auto target () const -> fcg::RenderTargetInfo {
		return {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			depth ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : SDL_GPU_TEXTUREFORMAT_INVALID, samples};
	}


	////
	// Methods

	/// Render the supplied commands, then download both attachments where supported.
	template <class Fn> auto render (Fn &&draw, const SDL_GPUViewport *viewport=nullptr) -> Image
	{
		auto *command = SDL_AcquireGPUCommandBuffer(device.handle());
		require(command, SDL_GetError());
		SDL_GPUColorTargetInfo colorInfo{};
		colorInfo.texture = multisample ? multisample->handle() : color.handle();
		colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorInfo.store_op = multisample ? SDL_GPU_STOREOP_RESOLVE : SDL_GPU_STOREOP_STORE;
		colorInfo.cycle = true;
		if (multisample) {
			colorInfo.resolve_texture = color.handle();
			colorInfo.cycle_resolve_texture = true;
		}
		SDL_GPUDepthStencilTargetInfo depthInfo{};
		depthInfo.texture = depth ? depth->handle() : nullptr;
		depthInfo.clear_depth = 1;
		depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		depthInfo.store_op = SDL_GPU_STOREOP_STORE;
		depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
		depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
		depthInfo.cycle = true;
		auto *pass = SDL_BeginGPURenderPass(command, &colorInfo, 1, depth ? &depthInfo : nullptr);
		require(pass, SDL_GetError());
		if (viewport)
			SDL_SetGPUViewport(pass, viewport);
		try {
			draw(command, pass);
		} catch (...) {
			SDL_EndGPURenderPass(pass);
			SDL_CancelGPUCommandBuffer(command);
			throw;
		}
		SDL_EndGPURenderPass(pass);
		require(SDL_SubmitGPUCommandBuffer(command), SDL_GetError());

		Image image{size, {}, {}};
		image.colors.resize(size.x * size.y);
		auto ticket = take(color.readback());
		check(ticket.wait());
		auto mapped = take(ticket.map());
		for (unsigned y = 0; y < size.y; ++y)
			for (unsigned x = 0; x < size.x; ++x)
				image.colors[y * size.x + x] = take(mapped.template readTexel<std::array<std::uint8_t, 4>>({x,y,0}));
		if (depth && samples == SDL_GPU_SAMPLECOUNT_1)
		{
			image.depths.resize(size.x * size.y);
			auto download = take(depth->readback());
			check(download.wait());
			auto pixels = take(download.map());
			for (unsigned y = 0; y < size.y; ++y)
				for (unsigned x = 0; x < size.x; ++x)
					image.depths[y * size.x + x] = take(pixels.template readTexel<float>({x,y,0}));
		}
		return image;
	}

	/// Allocate one attachment with this surface's physical dimensions.
	auto make (SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage, SDL_GPUSampleCount count)
		-> fcg::Texture
	{
		SDL_GPUTextureCreateInfo info{};
		info.type = SDL_GPU_TEXTURETYPE_2D;
		info.format = format;
		info.usage = usage;
		info.width = size.x;
		info.height = size.y;
		info.layer_count_or_depth = 1;
		info.num_levels = 1;
		info.sample_count = count;
		return take(fcg::Texture::create(device, info));
	}


	////
	// Fields

	/// Device shared by all attachments.
	fcg::Device &device;

	/// Physical attachment dimensions.
	glm::uvec2 size;

	/// Pipeline sample count.
	SDL_GPUSampleCount samples;

	/// Color output or resolve destination.
	fcg::Texture color;

	/// Optional multisampled color attachment.
	std::optional<fcg::Texture> multisample;

	/// Optional depth attachment.
	std::optional<fcg::Texture> depth;
};

/// A box record whose nontrivial stride also exercises borrowed layouts.
struct Shape
{
	/// Padding before the first consumed attribute.
	glm::vec4 padding{0};

	/// Homogeneous center.
	glm::vec4 position{0, 0, .5f, 1};

	/// Local half-extents.
	glm::vec3 extent{.45f, .35f, .2f};

	/// Local-to-model rotation.
	glm::quat orientation{1, 0, 0, 0};

	/// Linear RGBA color.
	glm::vec4 color{.8f, .3f, .6f, .7f};

	/// Physical pixel half-width.
	float radius = 2.f;
};

/// Build explicit model-space edges using double-precision CPU rotation, independent of shader tables.
auto reference (fcg::Device &device, std::span<const Shape> shapes) -> fcg::LineSet
{
	std::vector<glm::vec4> positions, colors;
	std::vector<float> radii;
	std::vector<std::uint32_t> indices;
	for (const auto &shape : shapes)
		for (int axis = 0; axis < 3; ++axis)
			for (int signs = 0; signs < 4; ++signs)
			{
				glm::dvec3 point(-1);
				int bit = 0;
				for (int other = 0; other < 3; ++other)
					if (other != axis)
						point[other] = (signs & (1 << bit++)) ? 1.0 : -1.0;
				for (int end = 0; end < 2; ++end)
				{
					point[axis] = end ? 1.0 : -1.0;
					const auto rotated = glm::normalize(glm::dquat(shape.orientation)) * (point * glm::dvec3(shape.extent));
					positions.emplace_back(glm::dvec3(shape.position) / double(shape.position.w) + rotated, 1);
					colors.push_back(shape.color);
					radii.push_back(shape.radius);
					indices.push_back((std::uint32_t)indices.size());
				}
			}
	fcg::PrimitiveAttributes attributes(device);
	check(attributes.setAttributes([&] (auto &update) {
		update.template set<A::Position>(std::span(positions));
		update.template set<A::Color>(std::span(colors));
		update.template set<A::Radius>(std::span(radii));
	}));
	fcg::PrimitiveConnectivity connectivity(device);
	check(connectivity.setIndices(indices));
	return fcg::LineSet(std::move(attributes), std::move(connectivity));
}

/// Compare output while allowing rounding at independently transformed boundaries.
void compareImages (const Image &actual, const Image &expected)
{
	unsigned differences = 0;
	for (std::size_t i = 0; i < actual.colors.size(); ++i)
	{
		bool differs = false;
		for (int channel = 0; channel < 4; ++channel)
			differs |= std::abs(int(actual.colors[i][channel]) - int(expected.colors[i][channel])) > 3;
		if (differs)
			++differences;
		if (!actual.depths.empty() && !differs)
			require(std::abs(actual.depths[i] - expected.depths[i]) < 2e-4f, "Wire depth differs from explicit edges");
	}
	require(differences <= 4, "Wire coverage/color differs from explicit edges: " + std::to_string(differences));
}

/// Compare one draw against independently built edges for arrays, borrowed records, constants or defaults.
void verify (
	fcg::Device &device, Surface &surface, std::vector<Shape> shapes, fcg::RenderState &state,
	fcg::RendererOptions options={}, fcg::LineStyle style={}, int source=0, fcg::InstanceRange range={},
	const SDL_GPUViewport *viewport=nullptr
)
{
	// Upload or borrow the selected per-box source layouts
	fcg::PrimitiveAttributes attributes(device);
	auto buffer = take(fcg::Buffer::create(device, std::span(shapes), SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	std::vector<glm::vec4> positions, colors;
	std::vector<glm::vec3> extents;
	std::vector<glm::quat> orientations;
	std::vector<float> radii;
	for (const auto &shape : shapes) {
		positions.push_back(shape.position);
		extents.push_back(shape.extent);
		orientations.push_back(shape.orientation);
		colors.push_back(shape.color);
		radii.push_back(shape.radius);
	}
	check(attributes.setAttributes([&] (auto &update)
	{
		if (source == 1) {
			update.template bind<A::Position>({&buffer, offsetof(Shape,position), sizeof(Shape), shapes.size()});
			update.template bind<A::Extent>({&buffer, offsetof(Shape,extent), sizeof(Shape), shapes.size()});
			update.template bind<A::Orientation>({&buffer, offsetof(Shape,orientation), sizeof(Shape), shapes.size()});
			update.template bind<A::Color>({&buffer, offsetof(Shape,color), sizeof(Shape), shapes.size()});
			update.template bind<A::Radius>({&buffer, offsetof(Shape,radius), sizeof(Shape), shapes.size()});
		}
		else {
			update.template set<A::Position>(std::span(positions));
			if (source == 0) {
				update.template set<A::Extent>(std::span(extents));
				update.template set<A::Orientation>(std::span(orientations));
				update.template set<A::Color>(std::span(colors));
				update.template set<A::Radius>(std::span(radii));
			}
			else if (source == 2) {
				update.template set<A::Extent>(shapes[0].extent);
				update.template set<A::Orientation>(shapes[0].orientation);
				update.template set<A::Color>(shapes[0].color);
				update.template set<A::Radius>(shapes[0].radius);
			}
		}
		update.template set<A::Normal>(std::span<const glm::vec3>{});
	}));

	// Compare through the polymorphic interface after moving the renderer
	auto original = take(fcg::WireBoxRenderer::create(device, surface.target(), options));
	auto wire = std::move(original);
	wire.setStyle(style);
	require(wire.style().defaultWidth == style.defaultWidth && wire.supports(A::Radius)
		&& wire.supports(A::Extent) && !wire.supports(A::Normal), "Wrong wire interface or style");
	const fcg::PrimitiveRenderer &renderer = wire;
	const auto actual = surface.render([&] (auto *command, auto *pass) {
		check(renderer.draw(attributes, state, command, pass, {}, range));
	}, viewport);
	const auto count = range.count == std::dynamic_extent ? shapes.size() - range.first : range.count;
	auto edges = reference(device, std::span(shapes).subspan(range.first, count));
	auto line = take(fcg::LineRenderer::create(device, surface.target(), options));
	line.setStyle(style);
	const auto expected = surface.render([&] (auto *command, auto *pass) {
		check(line.draw(edges, state, command, pass));
	}, viewport);
	compareImages(actual, expected);
}

/// Exercise placement, projection, sources, clipping, caps and degenerate edges.
void geometry (fcg::Device &device)
{
	Surface surface(device, {128, 96});
	fcg::RenderState state(device, surface.size);
	Shape shape;
	verify(device, surface, {shape}, state);
	for (int source : {1, 2})
		verify(device, surface, {shape}, state, {}, {}, source);
	shape.orientation = glm::angleAxis(.6f, glm::normalize(glm::vec3(1,2,3))) * 1e-30f;
	shape.position *= 2.f;
	shape.extent = {.4f, .2f, .3f};
	for (bool blending : {false, true})
		for (auto cap : {fcg::LineCap::Round, fcg::LineCap::Butt})
			verify(device, surface, {shape}, state, {.depthWrite=false, .alphaBlending=blending}, {.cap=cap});
	verify(device, surface, {shape}, state, {.alphaBlending=true}, {.filter=fcg::LineFilter::None});
	shape.orientation = glm::quat(1e30f, 2e30f, 3e30f, 4e30f); // Exercise a large finite quaternion
	verify(device, surface, {shape}, state);
	shape.orientation = glm::quat(1,0,0,0);
	for (const auto extent : {glm::vec3(0,.3f,.2f), glm::vec3(0,0,.2f), glm::vec3(0)})
		for (auto cap : {fcg::LineCap::Round, fcg::LineCap::Butt}) {
			shape.extent = extent;
			verify(device, surface, {shape}, state, {.depthWrite=false, .alphaBlending=true}, {.cap=cap});
		}
	shape = {};
	shape.radius = 0;
	verify(device, surface, {shape}, state);
	shape.extent = glm::vec3(1);
	shape.color = glm::vec4(1);
	shape.radius = 3;
	verify(device, surface, {shape}, state, {}, {.defaultWidth=6}, 3);

	// Distinct boxes expose box-based ranges and interleaved addressing
	std::vector<Shape> shapes(2);
	shapes[0].position.x = -.45f;
	shapes[1].position.x = .45f;
	shapes[1].color = {0,1,0,1};
	shapes[1].radius = 4;
	for (int source : {0, 1})
		for (auto range : {fcg::InstanceRange{}, fcg::InstanceRange{1,1}, fcg::InstanceRange{2,0}})
			verify(device, surface, shapes, state, {}, {}, source, range);

	// Perspective clipping includes near, far and eye-plane crossings
	state.loadProjectionMatrix(glm::perspectiveRH_ZO(1.f, float(surface.size.x)/surface.size.y, .1f, 10.f));
	for (float z : {-2.f, -.15f, .05f, -9.9f, 2.f}) {
		shape = {};
		shape.position.z = z;
		verify(device, surface, {shape}, state);
	}
	state.loadModelviewMatrix(glm::scale(glm::mat4(1), glm::vec3(.7f,1.2f,.8f)));
	shape.position.z = -2;
	verify(device, surface, {shape}, state);
}

/// Check target variants, feature errors and depth interoperability with a filled box.
void targets (fcg::Device &device)
{
	for (bool depth : {false, true})
		for (auto cull : {SDL_GPU_CULLMODE_NONE, SDL_GPU_CULLMODE_FRONT, SDL_GPU_CULLMODE_BACK})
		{
			Surface surface(device, {96,80}, depth);
			fcg::RenderState state(device, surface.size);
			verify(device, surface, {Shape{}}, state, {.cullMode=cull});
			const SDL_GPUViewport viewport{7,9,71,53,0,1};
			state.setViewportSize({71,53});
			verify(device, surface, {Shape{}}, state, {.depthTest=false, .depthWrite=false}, {}, 0, {}, &viewport);
		}
	if (SDL_GPUTextureSupportsSampleCount(device.handle(), SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_SAMPLECOUNT_4)) {
		Surface surface(device, {96,80}, false, SDL_GPU_SAMPLECOUNT_4);
		fcg::RenderState state(device, surface.size);
		verify(device, surface, {Shape{}}, state, {.alphaBlending=true});
	}
	Surface surface(device);
	fcg::RenderState state(device, surface.size);
	auto wire = take(fcg::WireBoxRenderer::create(device, surface.target()));
	fcg::PrimitiveAttributes empty(device);
	const auto image = surface.render([&] (auto *command, auto *pass) {
		fcg::DrawOptions options;
		options.lighting.enabled = true;
		auto result = wire.draw(empty, state, command, pass, options);
		require(!result && result.error().code == fcg::RenderErrorCode::InvalidArgument, "Lighting accepted");
		options.lighting.enabled = false;
		options.texture.emplace();
		result = wire.draw(empty, state, command, pass, options);
		require(!result && result.error().code == fcg::RenderErrorCode::InvalidArgument, "Texture accepted");
		check(wire.draw(empty, state, command, pass));
	});
	require(std::all_of(image.colors.begin(), image.colors.end(), [] (auto pixel) { return pixel[3] == 0; }),
		"Empty/feature-error draws changed the pass");

	// A nearer filled box occludes the farther wire using the same attribute collection
	fcg::PrimitiveAttributes attributes(device);
	const std::array position{glm::vec4(0,0,.5f,1)};
	check(attributes.setAttributes([&] (auto &update) {
		update.template set<A::Position>(std::span(position));
		update.template set<A::Extent>(glm::vec3(.4f,.3f,.1f));
	}));
	auto box = take(fcg::BoxRenderer::create(device, surface.target(), {.cullMode=SDL_GPU_CULLMODE_NONE}));
	const auto filled = surface.render([&] (auto *command, auto *pass) { check(box.draw(attributes,state,command,pass)); });
	const auto combined = surface.render([&] (auto *command, auto *pass) {
		check(box.draw(attributes,state,command,pass));
		state.pushModelviewMatrix();
		state.mulModelviewMatrix(glm::translate(glm::mat4(1), glm::vec3(0,0,.3f)));
		check(wire.draw(attributes,state,command,pass));
		state.popModelviewMatrix();
	});
	// The visible outside stroke and its occluded inside half agree with explicit depth-tested edges
	Shape behind;
	behind.position.z = .8f;
	behind.extent = {.4f,.3f,.1f};
	behind.color = glm::vec4(1);
	behind.radius = .5f;
	const std::array shape{behind};
	auto edges = reference(device, shape);
	auto lines = take(fcg::LineRenderer::create(device, surface.target()));
	const auto expected = surface.render([&] (auto *command, auto *pass) {
		check(box.draw(attributes,state,command,pass));
		check(lines.draw(edges,state,command,pass));
	});
	compareImages(combined, expected);
	const auto center = (surface.size.y/2)*surface.size.x + surface.size.x/2;
	require(combined.colors[center] == filled.colors[center] && combined.depths[center] == filled.depths[center],
		"Wire changed occluding surface interior");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run GPU comparisons against independently transformed explicit edges.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	int result = 0;
	try {
		auto device = fcg::Device::create();
		require(device.has_value(), SDL_GetError());
		geometry(*device);
		targets(*device);
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
