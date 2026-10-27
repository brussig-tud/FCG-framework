
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



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Node attribute shorthand.
using A = fcg::Attribute;

static_assert(!std::is_copy_constructible_v<fcg::LineRenderer>);
static_assert(std::is_nothrow_move_constructible_v<fcg::LineRenderer>);
static_assert(std::is_nothrow_move_assignable_v<fcg::LineRenderer>);
static_assert(!std::is_move_constructible_v<fcg::PrimitiveConnectivity>);

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

/// Independent segment parameters for CPU geometry checks.
struct Segment
{
	/// Homogeneous endpoint positions.
	std::array<glm::vec4, 2> positions{{{-.6f, .13f, .3f, 1}, {.6f, .13f, .7f, 1}}};

	/// Endpoint pixel radii.
	std::array<float, 2> radii{3.f, 3.f};

	/// Endpoint linear RGBA values.
	std::array<glm::vec4, 2> colors{{{1, 0, 0, 1}, {0, 1, 0, 1}}};
};

/// CPU projection of a finite, depth-clipped stroke.
struct Projected
{
	/// Whether any projectable centerline survives.
	bool visible = false;

	/// Endpoint coordinates in framebuffer pixels.
	glm::dvec2 a{}, b{};

	/// Screen-linear endpoint half-widths.
	glm::dvec2 radius{};

	/// Endpoint normalized depths.
	glm::dvec2 depth{};

	/// Reciprocal endpoint clip W values.
	glm::dvec2 reciprocalW{};

	/// Colors at the clipped endpoints.
	glm::dvec4 colorA{}, colorB{};

	/// Whether each endpoint has an outward semicircle.
	std::array<bool, 2> round{};
};

/// Project and clip using a double-precision CPU interval calculation.
auto project (
	const Segment &segment, const glm::mat4 &matrix, glm::uvec2 size, fcg::LineCap cap
) -> Projected
{
	// Clip the original centerline to both homogeneous depth half-spaces
	const auto transform = glm::dmat4(matrix);
	const auto pa = glm::dvec4(segment.positions[0]);
	const auto pb = glm::dvec4(segment.positions[1]);
	const auto a = transform * glm::dvec4(glm::dvec3(pa) / pa.w, 1);
	const auto b = transform * glm::dvec4(glm::dvec3(pb) / pb.w, 1);
	double low = 0, high = 1;
	for (unsigned plane = 0; plane < 2; ++plane)
	{
		const double da = plane ? a.w - a.z : a.z;
		const double db = plane ? b.w - b.z : b.z;
		if (da < 0 && db < 0)
			return {};
		if (da < 0)
			low = std::max(low, da / (da - db));
		else if (db < 0)
			high = std::min(high, da / (da - db));
	}
	if (low > high)
		return {};
	const auto ca = glm::mix(a, b, low), cb = glm::mix(a, b, high);
	if (ca.w <= 0 || cb.w <= 0 || std::max(segment.radii[0], segment.radii[1]) == 0)
		return {};

	// Convert to top-left framebuffer coordinates, independently of the shader's stroke basis
	const auto pixel = [size] (glm::dvec4 p) {
		return glm::dvec2((p.x / p.w + 1) * .5 * size.x, (1 - p.y / p.w) * .5 * size.y);
	};
	Projected out;
	out.visible = true;
	out.a = pixel(ca);
	out.b = pixel(cb);
	out.radius = {std::lerp(segment.radii[0], segment.radii[1], low),
		std::lerp(segment.radii[0], segment.radii[1], high)};
	out.depth = {ca.z / ca.w, cb.z / cb.w};
	out.reciprocalW = {1 / ca.w, 1 / cb.w};
	out.colorA = glm::mix(glm::dvec4(segment.colors[0]), glm::dvec4(segment.colors[1]), low);
	out.colorB = glm::mix(glm::dvec4(segment.colors[0]), glm::dvec4(segment.colors[1]), high);
	out.round = {cap == fcg::LineCap::Round && low == 0, cap == fcg::LineCap::Round && high == 1};
	if (glm::length(out.b - out.a) < .0001)
	{
		if (!out.round[0] && !out.round[1])
			return {};
		if (out.depth.y < out.depth.x) {
			out.a = out.b;
			out.colorA = out.colorB;
			out.depth.x = out.depth.y;
			out.reciprocalW.x = out.reciprocalW.y;
		}
		out.b = out.a;
		out.colorB = out.colorA;
		out.depth.y = out.depth.x;
		out.reciprocalW.y = out.reciprocalW.x;
		out.radius = glm::dvec2(std::max(out.radius.x, out.radius.y));
		out.round = {true, true};
	}
	return out;
}

/// Geometric inclusion test without using the shader's distance-to-outline algorithm.
auto inside (const Projected &stroke, glm::dvec2 pixel) -> bool
{
	if (!stroke.visible)
		return false;
	const auto delta = stroke.b - stroke.a;
	const double length = glm::length(delta);
	if (length == 0)
		return glm::length(pixel - stroke.a) <= stroke.radius.x;
	const auto tangent = delta / length;
	const double along = glm::dot(pixel - stroke.a, tangent);
	const double across = std::abs(glm::dot(pixel - stroke.a, glm::dvec2(-tangent.y, tangent.x)));
	if (along < 0)
		return stroke.round[0] && glm::length(pixel - stroke.a) <= stroke.radius.x;
	if (along > length)
		return stroke.round[1] && glm::length(pixel - stroke.b) <= stroke.radius.y;
	return across <= std::lerp(stroke.radius.x, stroke.radius.y, along / length);
}

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

/// Assign typed node values using the real attribute upload path.
void assign (fcg::PrimitiveAttributes &attributes, const Segment &segment)
{
	check(attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.set<A::Position>(std::span(segment.positions));
		update.set<A::Radius>(std::span(segment.radii));
		update.set<A::Color>(std::span(segment.colors));
	}));
}

/// Compare hard coverage, interpolated colors and depth to independent projected geometry.
void verifyHard (const Image &image, const Projected &stroke, const std::string &label)
{
	std::size_t visible = 0;
	for (unsigned y = 0; y < image.size.y; ++y)
		for (unsigned x = 0; x < image.size.x; ++x)
		{
			const auto index = y * image.size.x + x;
			const auto pixel = image.colors[index];
			const glm::dvec2 point(x + .5, y + .5);
			const bool expected = inside(stroke, point);
			bool boundary = false;
			for (const auto offset : {glm::dvec2(.04, 0), glm::dvec2(-.04, 0),
				glm::dvec2(0, .04), glm::dvec2(0, -.04)})
				boundary = boundary || inside(stroke, point + offset) != expected;
			if (boundary)
				continue;
			const auto location = label + " at " + std::to_string(x) + "," + std::to_string(y);
			require((pixel[3] != 0) == expected, location + ": coverage mismatch");
			if (!expected) {
				if (!image.depths.empty())
					require(image.depths[index] == 1.f, location + ": outside stroke wrote depth");
				continue;
			}
			++visible;
			const auto delta = stroke.b - stroke.a;
			const auto squared = glm::dot(delta, delta);
			const double t = squared == 0 ? 0 : glm::clamp(glm::dot(point - stroke.a, delta) / squared, 0., 1.);
			const auto weights = glm::dvec2(1 - t, t) * stroke.reciprocalW;
			const auto color = (stroke.colorA * weights.x + stroke.colorB * weights.y) / (weights.x + weights.y);
			for (unsigned channel = 0; channel < 4; ++channel)
				require(std::abs(pixel[channel] - std::round(255 * color[channel])) <= 3,
					location + ": perspective color mismatch");
			if (!image.depths.empty())
				require(std::abs(image.depths[index] - std::lerp(stroke.depth.x, stroke.depth.y, t)) < 2e-4,
					location + ": centerline depth mismatch");
		}
	std::cout << label << ": " << visible << " checked interior pixels\n";
}

/// Exercise projection, tapering, finite caps, clipping, zero widths and collapsed segments.
void geometry (fcg::Device &device)
{
	Surface surface(device);
	auto renderer = take(fcg::LineRenderer::create(device, surface.target()));
	fcg::PrimitiveAttributes attributes(device);
	fcg::PrimitiveConnectivity connectivity(device);
	const std::array<std::uint32_t, 2> pair{0, 1};
	check(connectivity.setIndices(pair));
	fcg::RenderState state(device, surface.size);
	require(renderer.nodesPerPrimitive() == 2 && renderer.supports(A::Radius) && !renderer.supports(A::Normal),
		"Wrong line renderer interface");
	const auto draw = [&] (const Segment &segment, const std::string &label)
	{
		assign(attributes, segment);
		auto image = surface.render([&] (auto *command, auto *pass) {
			check(renderer.draw(attributes, connectivity, state, command, pass));
		});
		verifyHard(image, project(segment, state.modelviewProjectionMatrix(), surface.size, renderer.style().cap), label);
	};
	Segment segment;
	draw(segment, "horizontal");
	segment.positions = {{{-.63f, -.5f, .2f, 1}, {.58f, .57f, .8f, 1}}};
	segment.radii = {1.5f, 7.f};
	draw(segment, "diagonal taper");
	std::swap(segment.positions[0], segment.positions[1]);
	std::swap(segment.radii[0], segment.radii[1]);
	std::swap(segment.colors[0], segment.colors[1]);
	draw(segment, "reversed taper");
	renderer.setStyle({.cap = fcg::LineCap::Butt});
	draw(segment, "butt taper");
	segment.positions = {{{.13f, -.6f, .2f, 1}, {.13f, .6f, .8f, 1}}};
	draw(segment, "vertical butt");
	segment.positions[1] = segment.positions[0];
	draw(segment, "collapsed butt");
	renderer.setStyle({});
	segment.radii = {2.f, 5.f};
	segment.positions[1].z = .1f;
	draw(segment, "collapsed round");
	segment.radii = {0.f, 0.f};
	draw(segment, "zero radii");
	segment.positions = {{{-.65f, .12f, .3f, 1}, {.6f, .17f, .6f, 1}}};
	segment.radii = {0.f, 5.f};
	draw(segment, "zero-radius taper");
	segment.positions = {{{-.03f, -.02f, .3f, 1}, {.02f, .02f, .6f, 1}}};
	segment.radii = {7.f, 2.f};
	draw(segment, "short steep taper");
	segment.positions[0] *= 2.f;
	segment.positions[1] *= .5f;
	state.loadModelviewMatrix(glm::scale(glm::mat4(1), glm::vec3(.7f, 1.3f, .5f)));
	draw(segment, "homogeneous positions and model scale");
	state.loadModelviewMatrix(glm::mat4(1));

	// Clip perspective lines near the camera and preserve their surviving endpoint attributes
	state.loadProjectionMatrix(glm::perspectiveRH_ZO(1.f, float(surface.size.x) / surface.size.y, .1f, 20.f));
	segment.positions = {{{-.4f, -.2f, -.7f, 1}, {.7f, .3f, -4.f, 1}}};
	segment.radii = {3.f, 6.f};
	draw(segment, "perspective");
	segment.positions = {{{-.04f, .02f, -.02f, 1}, {.5f, .15f, -2.f, 1}}};
	draw(segment, "near clipping");
	segment.positions[0] = {-.05f, -.02f, .5f, 1};
	draw(segment, "eye crossing");
	segment.positions = {{{-.5f, 0, -2.f, 1}, {.5f, .3f, -30.f, 1}}};
	draw(segment, "far clipping");
	segment.positions = {{{-.2f, .2f, .5f, 1}, {.3f, -.2f, 1.f, 1}}};
	draw(segment, "behind camera");
}

/// Compare filtered coverage to an independent 8x8 geometric supersampling reference.
void filtering (fcg::Device &device)
{
	Surface surface(device, {96, 64}, false);
	auto renderer = take(fcg::LineRenderer::create(device, surface.target(), {.alphaBlending = true}));
	fcg::PrimitiveAttributes attributes(device);
	fcg::PrimitiveConnectivity connectivity(device);
	const std::array<std::uint32_t, 2> pair{0, 1};
	check(connectivity.setIndices(pair));
	fcg::RenderState state(device, surface.size);
	Segment segment;
	segment.positions = {{{-.65f, -.2f, .3f, 1}, {.63f, .28f, .7f, 1}}};
	segment.radii = {2.f, 5.f};
	assign(attributes, segment);
	for (const auto cap : {fcg::LineCap::Round, fcg::LineCap::Butt})
	{
		renderer.setStyle({.cap = cap});
		const auto stroke = project(segment, state.modelviewProjectionMatrix(), surface.size, cap);
		const auto image = surface.render([&] (auto *command, auto *pass) {
			check(renderer.draw(attributes, connectivity, state, command, pass));
		});
		unsigned fractional = 0;
		double error = 0;
		for (unsigned y = 0; y < surface.size.y; ++y)
			for (unsigned x = 0; x < surface.size.x; ++x)
			{
				double covered = 0;
				for (unsigned sy = 0; sy < 8; ++sy)
					for (unsigned sx = 0; sx < 8; ++sx)
						covered += inside(stroke, {x + (sx + .5) / 8, y + (sy + .5) / 8});
				const auto alpha = image.colors[y * surface.size.x + x][3];
				fractional += alpha > 0 && alpha < 255;
				const double difference = std::abs(alpha / 255. - covered / 64.);
				require(difference < .36, "Filtered coverage differs excessively from geometric reference");
				error += difference;
			}
		require(fractional > 30, "Filtered edges have no fractional coverage");
		require(error / image.colors.size() < .01, "Filtered stroke has excessive total coverage error");
	}
	renderer.setStyle({.filter = fcg::LineFilter::None});
	const auto hard = surface.render([&] (auto *command, auto *pass) {
		check(renderer.draw(attributes, connectivity, state, command, pass));
	});
	verifyHard(hard, project(segment, state.modelviewProjectionMatrix(), surface.size, fcg::LineCap::Round),
		"filter disabled");
}

/// Exercise shared/borrowed nodes, selected ranges, copy passes and both retained feature errors.
void connectivity (fcg::Device &device)
{
	Surface surface(device);
	auto renderer = take(fcg::LineRenderer::create(device, surface.target()));
	fcg::PrimitiveAttributes attributes(device);
	fcg::PrimitiveConnectivity pairs(device);
	require(pairs.indexCount() == 0 && pairs.usable() && &pairs.device() == &device, "Wrong empty connectivity");
	const std::array<std::uint32_t, 6> indices{0, 1, 1, 0, 0, 1};
	check(pairs.setIndices(indices));
	auto source = pairs.source();
	auto ticket = take(source.buffer->readback(source.offset, indices.size() * sizeof(std::uint32_t)));
	check(ticket.wait());
	auto bytes = take(ticket.map());
	require(std::equal(indices.begin(), indices.end(), (const std::uint32_t*)bytes.data().data()),
		"Connectivity upload changed indices");

	// Select valid references from shorter attribute arrays without requiring full position coverage
	Segment segment;
	assign(attributes, segment);
	std::array<glm::vec4, 4> positions{segment.positions[0], segment.positions[1],
		glm::vec4(.8f, .8f, .5f, 1), glm::vec4(-.8f, -.8f, .5f, 1)};
	check(attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.set<A::Position>(std::span(positions));
		update.clear<A::Radius>();
	}));
	renderer.setStyle({.defaultWidth = 6.f});
	fcg::RenderState state(device, surface.size);
	auto image = surface.render([&] (auto *command, auto *pass) {
		check(renderer.draw(attributes, pairs, state, command, pass, {}, {1, 1}));
	});
	verifyHard(image, project(segment, state.modelviewProjectionMatrix(), surface.size, fcg::LineCap::Round),
		"range and fallback width");

	// Bind caller-owned connectivity with a nonzero offset and caller-owned interleaved node attributes
	const std::array<std::uint32_t, 4> borrowed{17, 0, 1, 23};
	auto buffer = take(fcg::Buffer::create(device, std::as_bytes(std::span(borrowed)),
		SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	pairs.bind({&buffer, sizeof(std::uint32_t), 2});
	struct Node {
		/// Homogeneous position within an interleaved record.
		glm::vec4 position;

		/// Linear RGBA color within the same record.
		glm::vec4 color;

		/// Physical pixel half-width within the same record.
		float radius;
	};
	const std::array nodes{Node{segment.positions[0], segment.colors[0], 3},
		Node{segment.positions[1], segment.colors[1], 3}};
	auto nodeBuffer = take(fcg::Buffer::create(device, std::as_bytes(std::span(nodes)),
		SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	check(attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.bind<A::Position>({&nodeBuffer, offsetof(Node, position), sizeof(Node), nodes.size()});
		update.bind<A::Color>({&nodeBuffer, offsetof(Node, color), sizeof(Node), nodes.size()});
		update.bind<A::Radius>({&nodeBuffer, offsetof(Node, radius), sizeof(Node), nodes.size()});
	}));
	image = surface.render([&] (auto *command, auto *pass) {
		check(renderer.draw(attributes, pairs, state, command, pass));
	});
	verifyHard(image, project(segment, state.modelviewProjectionMatrix(), surface.size, fcg::LineCap::Round),
		"borrowed sources");

	// Record index uploads in a supplied copy pass, then submit before drawing
	auto *command = SDL_AcquireGPUCommandBuffer(device.handle());
	auto *copy = SDL_BeginGPUCopyPass(command);
	check(pairs.setIndices(copy, std::span(indices).first(2)));
	SDL_EndGPUCopyPass(copy);
	require(SDL_SubmitGPUCommandBuffer(command), SDL_GetError());
	auto sampled = surface.make(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER,
		SDL_GPU_SAMPLECOUNT_1);
	const SDL_GPUSamplerCreateInfo samplerInfo{};
	const auto releaseSampler = [&] (SDL_GPUSampler *sampler) { SDL_ReleaseGPUSampler(device.handle(), sampler); };
	const std::unique_ptr<SDL_GPUSampler, decltype(releaseSampler)> sampler(
		SDL_CreateGPUSampler(device.handle(), &samplerInfo), releaseSampler);
	require(sampler != nullptr, SDL_GetError());
	image = surface.render([&] (auto *cmd, auto *pass) {
		const auto missing = renderer.draw(attributes, state, cmd, pass);
		require(!missing && missing.error().code == fcg::RenderErrorCode::InvalidArgument, "Missing connectivity accepted");
		fcg::DrawOptions lighting;
		lighting.lighting.enabled = true;
		const auto lit = renderer.draw(attributes, pairs, state, cmd, pass, lighting);
		require(!lit && lit.error().code == fcg::RenderErrorCode::InvalidArgument, "Lighting accepted");
		fcg::DrawOptions texture;
		texture.texture = fcg::PrimitiveTexture{sampled.handle(), sampler.get()};
		const auto textured = renderer.draw(attributes, pairs, state, cmd, pass, texture);
		require(!textured && textured.error().code == fcg::RenderErrorCode::InvalidArgument, "Texture accepted");
	});
	require(std::all_of(image.colors.begin(), image.colors.end(), [] (const auto &pixel) { return pixel[3] == 0; }),
		"Feature errors recorded drawing");
	pairs.clear();
	fcg::RenderState unspecified(device);
	surface.render([&] (auto *cmd, auto *pass) {
		check(renderer.draw(attributes, pairs, unspecified, cmd, pass));
	});
	require(pairs.indexCount() == 0 && pairs.usable(), "Clear did not reset connectivity");
}

/// Check resizing, translated viewports, blending opacity and optional target multisampling.
void targets (fcg::Device &device)
{
	for (const auto size : {glm::uvec2(64, 96), glm::uvec2(144, 80)})
	{
		Surface surface(device, size);
		auto renderer = take(fcg::LineRenderer::create(device, surface.target()));
		fcg::PrimitiveAttributes attributes(device);
		fcg::PrimitiveConnectivity pairs(device);
		const std::array<std::uint32_t, 2> indices{0, 1};
		check(pairs.setIndices(indices));
		fcg::RenderState state(device, size);
		Segment segment;
		assign(attributes, segment);
		const auto image = surface.render([&] (auto *cmd, auto *pass) {
			check(renderer.draw(attributes, pairs, state, cmd, pass));
		});
		verifyHard(image, project(segment, state.modelviewProjectionMatrix(), size, fcg::LineCap::Round),
			"resized target");

		// A sub-viewport's physical width is independent of its origin and attachment dimensions
		const glm::uvec2 viewSize(size.x / 2, size.y / 2);
		const SDL_GPUViewport viewport{11, 7, (float)viewSize.x, (float)viewSize.y, 0, 1};
		state.setViewportSize(viewSize);
		const auto sub = surface.render([&] (auto *cmd, auto *pass) {
			check(renderer.draw(attributes, pairs, state, cmd, pass));
		}, &viewport);
		const auto stroke = project(segment, state.modelviewProjectionMatrix(), viewSize, fcg::LineCap::Round);
		unsigned thickness = 0;
		const unsigned column = 11 + viewSize.x / 2;
		for (unsigned y = 0; y < size.y; ++y)
			thickness += sub.colors[y * size.x + column][3] != 0;
		require(std::abs(int(thickness) - 6) <= 1, "Sub-viewport changed physical width");
	}
	for (const auto samples : {SDL_GPU_SAMPLECOUNT_2, SDL_GPU_SAMPLECOUNT_4})
	{
		if (!SDL_GPUTextureSupportsSampleCount(device.handle(), SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, samples))
			continue;
		Surface surface(device, {64, 64}, false, samples);
		auto renderer = take(fcg::LineRenderer::create(device, surface.target(), {.alphaBlending = true}));
		fcg::PrimitiveAttributes attributes(device);
		fcg::PrimitiveConnectivity pairs(device);
		const std::array<std::uint32_t, 2> indices{0, 1};
		check(pairs.setIndices(indices));
		Segment segment;
		segment.colors = {glm::vec4(1, 0, 0, .4f), glm::vec4(1, 0, 0, .4f)};
		assign(attributes, segment);
		fcg::RenderState state(device, surface.size);
		const auto image = surface.render([&] (auto *cmd, auto *pass) {
			check(renderer.draw(attributes, pairs, state, cmd, pass));
		});
		const auto maximum = std::max_element(image.colors.begin(), image.colors.end(),
			[] (const auto &a, const auto &b) { return a[3] < b[3]; });
		require(std::abs(int((*maximum)[3]) - 102) <= 2 && std::abs(int((*maximum)[0]) - 102) <= 2,
			"Multisampled straight-alpha output is incorrect");
	}
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run GPU geometry and API contract tests using valid inputs.
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
		filtering(*device);
		connectivity(*device);
		targets(*device);
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
