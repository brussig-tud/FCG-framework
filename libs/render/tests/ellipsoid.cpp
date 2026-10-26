
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// GLM library
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/texture.h>
#include <FCG/Render/box_renderer.h>
#include <FCG/Render/ellipsoid_renderer.h>
#include <FCG/Render/quad_renderer.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Attribute shorthand for the test scenes.
using A = fcg::Attribute;

/// Resolution used by the CPU reference and offscreen render targets.
constexpr Uint32 side = 64;

static_assert(!std::is_copy_constructible_v<fcg::EllipsoidRenderer>);
static_assert(std::is_nothrow_move_constructible_v<fcg::EllipsoidRenderer>);
static_assert(std::is_nothrow_move_assignable_v<fcg::EllipsoidRenderer>);

/// Fail with a useful test diagnostic.
void require (bool condition, const std::string &message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Unwrap a framework result, preserving its error diagnostic.
template <class T, class Error> auto take (std::expected<T, Error> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Check an operation without discarding its diagnostic.
template <class Error> void check (std::expected<void, Error> result) {
	if (!result)
		throw std::runtime_error(result.error().message);
}

/// Require a specific Render error.
template <class T> void fails (const std::expected<T, fcg::RenderError> &result, fcg::RenderErrorCode code) {
	require(!result && result.error().code == code, "Unexpected ellipsoid validation result");
}

/// Create a test texture with the requested format, usage, and sample count.
auto texture (
	fcg::Device &device, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage,
	SDL_GPUSampleCount samples=SDL_GPU_SAMPLECOUNT_1, Uint32 size=side, Uint32 levels=1
) -> fcg::Texture
{
	SDL_GPUTextureCreateInfo info{};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = format;
	info.usage = usage;
	info.width = size;
	info.height = size;
	info.layer_count_or_depth = 1;
	info.num_levels = levels;
	info.sample_count = samples;
	return take(fcg::Texture::create(device, info));
}

/// CPU-owned render result, including surface depth for single-sample targets.
struct Image
{
	/// RGBA pixels in top-to-bottom row order.
	std::vector<std::array<std::uint8_t, 4>> colors;

	/// Floating-point depth values, or empty for targets without downloadable depth.
	std::vector<float> depths;
};

/// An independently parameterized test ellipsoid.
struct Ellipsoid
{
	/// Object-space homogeneous center.
	glm::vec4 position{0, 0, -4, 1};

	/// Local semi-axis lengths.
	glm::vec3 extent{1};

	/// Local-to-object rotation.
	glm::quat orientation{1, 0, 0, 0};

	/// Linear surface color.
	glm::vec4 color{.7f, .4f, .2f, 1};
};

/// Independent CPU reference intersection at one pixel.
struct Hit
{
	/// Whether a surface survives clipping and culling.
	bool visible = false;

	/// Actual projected surface depth.
	double depth = 1;

	/// Unit sphere coordinate, also used for UV reference values.
	glm::dvec3 sphere{1, 0, 0};

	/// Outward unit normal in eye space.
	glm::dvec3 normal{0, 0, 1};
};

/// Minimal command ownership that cancels incomplete recording on exceptions.
class Commands
{
public:

	////
	// Object construction/destruction

	/// Acquire one command buffer on the current thread.
	explicit Commands(fcg::Device &device) : handle(SDL_AcquireGPUCommandBuffer(device.handle())) {
		require(handle != nullptr, SDL_GetError());
	}

	/// End the pass and cancel commands that were not submitted.
	~Commands() {
		if (pass)
			SDL_EndGPURenderPass(pass);
		if (handle)
			SDL_CancelGPUCommandBuffer(handle);
	}

	/// Commands have one owner.
	Commands(const Commands&) = delete;

	/// Command ownership cannot be reassigned.
	auto operator= (const Commands&) -> Commands& = delete;


	////
	// Methods

	/// End recording and transfer command ownership to SDL.
	void submit () {
		SDL_EndGPURenderPass(std::exchange(pass, nullptr));
		require(SDL_SubmitGPUCommandBuffer(std::exchange(handle, nullptr)), SDL_GetError());
	}


	////
	// Fields

	/// Owned until submission or cancellation.
	SDL_GPUCommandBuffer *handle;

	/// Active pass, when recording draws.
	SDL_GPURenderPass *pass = nullptr;
};

/// Offscreen attachments with optional multisample resolve and exact depth readback.
class Scene
{
public:

	////
	// Object construction/destruction

	/// Allocate render targets for one scene configuration.
	explicit Scene(fcg::Device &device, SDL_GPUSampleCount samples=SDL_GPU_SAMPLECOUNT_1, bool withDepth=true)
		: device(device), samples(samples),
		color(texture(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET))
	{
		if (samples != SDL_GPU_SAMPLECOUNT_1)
			multisample.emplace(texture(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
				SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, samples));
		if (withDepth)
			depth.emplace(texture(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
				SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, samples));
	}


	////
	// Accessors

	/// Pipeline compatibility description.
	[[nodiscard]] auto target () const -> fcg::RenderTargetInfo {
		return {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			depth ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : SDL_GPU_TEXTUREFORMAT_INVALID, samples};
	}


	////
	// Methods

	/// Record caller-selected draws and download the resulting attachments.
	template <class Fn> [[nodiscard]] auto render (Fn &&draw) -> Image
	{
		// Render and optionally resolve the multisample color attachment
		Commands commands(device);
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
		depthInfo.clear_depth = 1.f;
		depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		depthInfo.store_op = SDL_GPU_STOREOP_STORE;
		depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
		depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
		depthInfo.cycle = true;
		commands.pass = SDL_BeginGPURenderPass(commands.handle, &colorInfo, 1, depth ? &depthInfo : nullptr);
		require(commands.pass != nullptr, SDL_GetError());
		draw(commands.handle, commands.pass);
		commands.submit();

		// Download with Core's row-pitch-aware accessors
		Image image;
		image.colors.resize(side * side);
		auto colorReadback = take(color.readback());
		check(colorReadback.wait());
		auto colors = take(colorReadback.map());
		for (Uint32 y = 0; y < side; ++y)
			for (Uint32 x = 0; x < side; ++x)
				image.colors[y * side + x] = take(colors.template readTexel<std::array<std::uint8_t, 4>>({x,y,0}));
		if (depth && samples == SDL_GPU_SAMPLECOUNT_1)
		{
			auto depthReadback = take(depth->readback());
			check(depthReadback.wait());
			auto depths = take(depthReadback.map());
			image.depths.resize(side * side);
			for (Uint32 y = 0; y < side; ++y)
				for (Uint32 x = 0; x < side; ++x)
					image.depths[y * side + x] = take(depths.template readTexel<float>({x,y,0}));
		}
		return image;
	}


	////
	// Fields

	/// Device borrowed by attachments.
	fcg::Device &device;

	/// Pipeline sample count.
	SDL_GPUSampleCount samples;

	/// Single-sample output, also the resolve destination.
	fcg::Texture color;

	/// Optional multisample color attachment.
	std::optional<fcg::Texture> multisample;

	/// Optional depth attachment matching the sample count.
	std::optional<fcg::Texture> depth;
};

/// Assign a scene's transform and appearance through the public attribute API.
void assign (fcg::PrimitiveAttributes &attributes, const Ellipsoid &shape)
{
	check(attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.set<A::Position>(std::span(&shape.position, 1));
		update.set<A::Extent>(shape.extent);
		update.set<A::Orientation>(shape.orientation);
		update.set<A::Color>(shape.color);
	}));
}

/// Solve an ordinary quadratic in double precision, independently of both shader intersection paths.
auto reference (
	const Ellipsoid &shape, const glm::mat4 &modelview, const glm::mat4 &projection,
	Uint32 x, Uint32 y, SDL_GPUCullMode cull
) -> Hit
{
	// Derive the implicit ellipsoid transform from the public instance parameters
	Hit hit;
	if (shape.extent.x == 0 || shape.extent.y == 0 || shape.extent.z == 0)
		return hit;
	const auto mv = glm::dmat4(modelview);
	const auto proj = glm::dmat4(projection);
	const auto rotation = glm::mat3_cast(glm::normalize(glm::dquat(shape.orientation)));
	auto basis = glm::dmat3(mv) * rotation;
	for (int i = 0; i < 3; ++i)
		basis[i] *= shape.extent[i];
	const auto inverse = glm::inverse(basis);
	const auto center = glm::dvec3(mv * glm::dvec4(glm::dvec3(shape.position) / (double)shape.position.w, 1));

	// Unproject pixel centers and solve the ray equation with standard quadratic coefficients
	const auto invProjection = glm::inverse(proj);
	const auto ndc = glm::dvec2(2.0 * (x + .5) / side - 1, 1 - 2.0 * (y + .5) / side);
	const auto near4 = invProjection * glm::dvec4(ndc, 0, 1);
	const auto middle4 = invProjection * glm::dvec4(ndc, .5, 1);
	const auto near = glm::dvec3(near4) / near4.w;
	const auto middle = glm::dvec3(middle4) / middle4.w;
	const auto origin = inverse * (near - center);
	const auto direction = inverse * (middle - near);
	const auto a = glm::dot(direction, direction);
	const auto b = 2 * glm::dot(origin, direction);
	const auto c = glm::dot(origin, origin) - 1;
	const auto discriminant = b*b - 4*a*c;
	if (discriminant < 0)
		return hit;
	const auto root = std::sqrt(discriminant);
	for (const double t : {(-b-root)/(2*a), (-b+root)/(2*a)})
	{
		const auto sphere = origin + t * direction;
		const bool front = glm::dot(sphere, direction) <= 0;
		if (t < 0 || (front && cull == SDL_GPU_CULLMODE_FRONT) || (!front && cull == SDL_GPU_CULLMODE_BACK))
			continue;
		const auto clip = proj * glm::dvec4(center + basis * sphere, 1);
		if (clip.w <= 0 || clip.z < 0 || clip.z > clip.w)
			continue;
		hit.visible = true;
		hit.depth = clip.z / clip.w;
		hit.sphere = sphere;
		hit.normal = glm::normalize(glm::transpose(inverse) * sphere);
		break;
	}
	return hit;
}

/// Compare complete GPU images and actual surface depth with the independent reference.
void verify (
	const Image &image, const Ellipsoid &shape, const fcg::RenderState &state,
	const std::string &label, SDL_GPUCullMode cull=SDL_GPU_CULLMODE_BACK, const fcg::DrawOptions &options={}
)
{
	std::size_t visible = 0;
	for (Uint32 y = 0; y < side; ++y)
		for (Uint32 x = 0; x < side; ++x)
		{
			const auto hit = reference(shape, state.modelviewMatrix(), state.projectionMatrix(), x, y, cull);
			const auto pixel = image.colors[y * side + x];
			const auto location = label + " at " + std::to_string(x) + "," + std::to_string(y);
			require((pixel[3] != 0) == hit.visible, location + ": silhouette mismatch");
			if (!hit.visible) {
				if (!image.depths.empty())
					require(image.depths[y * side + x] == 1.f, location + ": discarded fragment wrote depth");
				continue;
			}
			++visible;
			// Proxy vertices can snap to the rasterizer subpixel grid, perturbing interpolants near silhouettes
			if (!image.depths.empty())
				require(std::abs(image.depths[y * side + x] - hit.depth) < 3e-4, location + ": surface depth mismatch: "
					+ std::to_string(image.depths[y * side + x]) + " expected " + std::to_string(hit.depth));
			auto expected = glm::dvec3(shape.color);
			if (options.lighting.enabled)
				expected *= glm::dvec3(options.lighting.ambient) + glm::dvec3(options.lighting.diffuse)
					* std::max(0.0, glm::dot(hit.normal, glm::normalize(glm::dvec3(options.lighting.direction))));
			for (int channel = 0; channel < 3; ++channel)
				require(std::abs((int)pixel[channel] - (int)std::round(255 * glm::clamp(expected[channel], 0.0, 1.0))) <= 3,
					location + ": color or transformed normal mismatch");
		}
	std::cout << label << ": " << visible << " reference surface pixels\n";
}

/// Check both camera types, difficult clipping, affine transforms, and camera-inside culling.
void analytic (fcg::Device &device)
{
	Scene scene(device);
	auto renderer = take(fcg::EllipsoidRenderer::create(device, scene.target()));
	fcg::PrimitiveAttributes attributes(device);
	fcg::RenderState state(device);
	Ellipsoid shape;
	fcg::DrawOptions options;
	options.lighting = {.enabled = true, .direction = {.3f,.4f,1}, .ambient = {.1f,.2f,.15f}, .diffuse = {.7f,.6f,.8f}};
	const auto draw = [&] (const std::string &label, SDL_GPUCullMode cull=SDL_GPU_CULLMODE_BACK)
	{
		assign(attributes, shape);
		auto selected = take(fcg::EllipsoidRenderer::create(device, scene.target(), {.cullMode = cull}));
		auto image = scene.render([&] (auto *commands, auto *pass) {
			check(selected.draw(attributes, state, commands, pass, options));
		});
		verify(image, shape, state, label, cull, options);
	};

	// The paper's perspective path, including rotation and homogeneous centers
	state.loadProjectionMatrix(glm::perspectiveRH_ZO(glm::radians(60.f), 1.f, .1f, 100.f));
	draw("perspective sphere");

	// Missing optional attributes produce an opaque white unit sphere
	check(attributes.setAttributes([] (fcg::PrimitiveAttributes::Update &update) {
		update.clear<A::Extent>();
		update.clear<A::Orientation>();
		update.clear<A::Color>();
	}));
	shape.color = glm::vec4(1);
	const auto defaults = scene.render([&] (auto *commands, auto *pass) {
		check(renderer.draw(attributes, state, commands, pass, options));
	});
	verify(defaults, shape, state, "default unit sphere", SDL_GPU_CULLMODE_BACK, options);
	shape = {};

	// Analytic back faces and asymmetric projections use the same surface contracts
	draw("perspective back surface", SDL_GPU_CULLMODE_FRONT);
	state.loadProjectionMatrix(glm::frustumRH_ZO(-.06f,.1f,-.08f,.06f,.1f,100.f));
	draw("asymmetric perspective camera");
	state.loadProjectionMatrix(glm::perspectiveRH_ZO(glm::radians(60.f), 1.f, .1f, 100.f));
	shape.position = {.4f,-.2f,-8,2};
	shape.extent = {1.2f,.55f,.8f};
	shape.orientation = glm::angleAxis(.8f, glm::normalize(glm::vec3(.2f,1,.4f))) * 1e-20f;
	draw("rotated homogeneous ellipsoid");
	glm::mat4 affine(1);
	affine[0][0] = -1.3f;
	affine[1][0] = .4f;
	affine[1][1] = .7f;
	affine[2][2] = 1.1f;
	state.loadModelviewMatrix(affine);
	draw("reflected sheared ellipsoid");
	state.loadModelviewMatrix(glm::lookAtRH(glm::vec3(3,2,1), glm::vec3(0,0,-4), glm::vec3(0,1,0)));
	draw("oblique translated camera");
	state.loadModelviewMatrix(glm::mat4(1));
	shape = {};
	shape.position.z = -10000;
	shape.extent = {1700,1200,300};
	state.loadProjectionMatrix(glm::perspectiveRH_ZO(glm::radians(60.f), 1.f, 1.f, 20000.f));
	draw("distant ellipsoid");
	shape = {};
	shape.extent = {1,.003f,.6f};
	shape.orientation = glm::angleAxis(.31f, glm::vec3(0,0,1));
	state.loadProjectionMatrix(glm::perspectiveRH_ZO(glm::radians(60.f), 1.f, .1f, 100.f));
	draw("thin ellipsoid");

	// Orthographic fallback and clipping through each side of the surface
	shape = {};
	shape.extent = {1.1f,.65f,.9f};
	state.loadProjectionMatrix(glm::orthoRH_ZO(-2.f,2.f,-2.f,2.f,.1f,10.f));
	draw("orthographic ellipsoid");
	draw("orthographic back surface", SDL_GPU_CULLMODE_FRONT);
	state.loadProjectionMatrix(glm::orthoRH_ZO(-2.f,2.f,-2.f,2.f,3.8f,10.f));
	draw("near-clipped front faces");
	draw("near-clipped exit surface", SDL_GPU_CULLMODE_NONE);
	state.loadProjectionMatrix(glm::orthoRH_ZO(-2.f,2.f,-2.f,2.f,.1f,3.8f));
	draw("far-clipped front surface");
	draw("far-clipped back surface", SDL_GPU_CULLMODE_FRONT);

	// The proxy can cross clipping planes while its visible surface remains in range
	shape = {};
	shape.position = {1.2f,0,-.2f,1};
	state.loadProjectionMatrix(glm::perspectiveRH_ZO(glm::radians(85.f), 1.f, .05f, 10.f));
	draw("eye-plane crossing");
	shape.position = {0,0,0,1};
	draw("inside culled sphere");
	draw("inside sphere exit", SDL_GPU_CULLMODE_NONE);
	shape.position.z = -1.0001f;
	draw("eye just outside sphere", SDL_GPU_CULLMODE_NONE);
	shape.position.z = -1.01f;
	draw("silhouette path near eye", SDL_GPU_CULLMODE_NONE);
	shape.position = {10,0,-4,1};
	draw("offscreen ellipsoid");
	shape.position = {0,0,4,1};
	draw("ellipsoid behind eye");
	shape = {};
	shape.extent.y = 0;
	draw("collapsed ellipsoid");

	// Empty draws and malformed matrices fail before any invalid GPU command is emitted
	assign(attributes, Ellipsoid{});
	(void)scene.render([&] (auto *commands, auto *pass)
	{
		state.loadModelviewMatrix(glm::mat4(0));
		check(renderer.draw(attributes, state, commands, pass, {}, {.count = 0}));
		fails(renderer.draw(attributes, state, commands, pass), fcg::RenderErrorCode::InvalidArgument);
		state.loadModelviewMatrix(glm::mat4(1));
		state.loadProjectionMatrix(glm::mat4(0));
		fails(renderer.draw(attributes, state, commands, pass), fcg::RenderErrorCode::InvalidArgument);
		state.loadProjectionMatrix(glm::mat4(1));
		affine[0][3] = .1f;
		state.loadModelviewMatrix(affine);
		fails(renderer.draw(attributes, state, commands, pass), fcg::RenderErrorCode::InvalidArgument);
		state.loadModelviewMatrix(glm::mat4(1));
		affine = glm::mat4(1);
		affine[0][0] = std::numeric_limits<float>::quiet_NaN();
		state.loadProjectionMatrix(affine);
		fails(renderer.draw(attributes, state, commands, pass), fcg::RenderErrorCode::InvalidArgument);
	});
}

/// Check depth interaction with rasterized geometry in either submission order.
void interoperability (fcg::Device &device)
{
	Scene scene(device);
	auto ellipsoids = take(fcg::EllipsoidRenderer::create(device, scene.target()));
	auto quads = take(fcg::QuadRenderer::create(device, scene.target()));
	auto boxes = take(fcg::BoxRenderer::create(device, scene.target()));
	fcg::RenderState state(device);
	state.loadProjectionMatrix(glm::orthoRH_ZO(-2.f,2.f,-2.f,2.f,.1f,10.f));
	fcg::PrimitiveAttributes sphereData(device), quadData(device), boxData(device);
	Ellipsoid sphere;
	sphere.color = {1,0,0,1};
	assign(sphereData, sphere);
	Ellipsoid quad;
	quad.position.z = -3.5f;
	quad.extent = {.75f,.75f,0};
	quad.color = {0,0,1,1};
	assign(quadData, quad);
	Ellipsoid box;
	box.position = {.6f,0,-3.5f,1};
	box.extent = {.2f,.6f,.2f};
	box.color = {0,1,0,1};
	assign(boxData, box);
	for (bool reverse : {false, true})
	{
		auto image = scene.render([&] (auto *commands, auto *pass) {
			if (!reverse)
				check(ellipsoids.draw(sphereData, state, commands, pass));
			check(quads.draw(quadData, state, commands, pass));
			check(boxes.draw(boxData, state, commands, pass));
			if (reverse)
				check(ellipsoids.draw(sphereData, state, commands, pass));
		});
		for (Uint32 y = 0; y < side; ++y)
			for (Uint32 x = 0; x < side; ++x)
			{
				const auto hit = reference(sphere, state.modelviewMatrix(), state.projectionMatrix(), x, y, SDL_GPU_CULLMODE_BACK);
				double depth = hit.depth;
				std::array<std::uint8_t, 4> expected = hit.visible
					? std::array<std::uint8_t, 4>{255,0,0,255} : std::array<std::uint8_t, 4>{0,0,0,0};
				const double ex = 4.0 * (x + .5) / side - 2;
				const double ey = 2 - 4.0 * (y + .5) / side;
				for (int primitive = 0; primitive < 2; ++primitive)
				{
					const bool covers = primitive == 0 ? std::abs(ex) < .75 && std::abs(ey) < .75
						: ex > .4 && ex < .8 && std::abs(ey) < .6;
					const auto clip = state.projectionMatrix() * glm::vec4(0,0,primitive == 0 ? -3.5f : -3.3f,1);
					if (covers && clip.z / clip.w < depth) {
						depth = clip.z / clip.w;
						expected = primitive == 0 ? std::array<std::uint8_t, 4>{0,0,255,255}
							: std::array<std::uint8_t, 4>{0,255,0,255};
					}
				}
				require(image.colors[y * side + x] == expected, "Surface depth disagrees with boxes or quads");
			}
	}

	// Interleaved borrowed sources and reversed instance ranges must preserve opaque visibility
	std::array<Ellipsoid, 2> shapes;
	shapes[0].position.x = -.4f;
	shapes[0].color = {1,0,0,1};
	shapes[1].position.x = .4f;
	shapes[1].extent = {.7f,1.1f,1.3f};
	shapes[1].color = {0,1,0,1};
	auto buffer = take(fcg::Buffer::create(device, std::span(shapes), SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ));
	fcg::PrimitiveAttributes instances(device);
	check(instances.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
		update.bind<A::Position>({&buffer,offsetof(Ellipsoid,position),sizeof(Ellipsoid),2});
		update.bind<A::Extent>({&buffer,offsetof(Ellipsoid,extent),sizeof(Ellipsoid),2});
		update.bind<A::Orientation>({&buffer,offsetof(Ellipsoid,orientation),sizeof(Ellipsoid),2});
		update.bind<A::Color>({&buffer,offsetof(Ellipsoid,color),sizeof(Ellipsoid),2});
		update.set<A::Normal>(std::span<const glm::vec3>{});
		update.set<A::Radius>(std::span<const float>{});
	}));
	for (bool reverse : {false, true})
	{
		auto image = scene.render([&] (auto *commands, auto *pass) {
			if (!reverse)
				check(ellipsoids.draw(instances, state, commands, pass));
			else {
				check(ellipsoids.draw(instances, state, commands, pass, {}, {.first = 1}));
				check(ellipsoids.draw(instances, state, commands, pass, {}, {.count = 1}));
			}
		});
		for (Uint32 y = 0; y < side; ++y)
			for (Uint32 x = 0; x < side; ++x)
			{
				const auto a = reference(shapes[0], state.modelviewMatrix(), state.projectionMatrix(), x, y, SDL_GPU_CULLMODE_BACK);
				const auto b = reference(shapes[1], state.modelviewMatrix(), state.projectionMatrix(), x, y, SDL_GPU_CULLMODE_BACK);
				const std::array<std::uint8_t, 4> expected = b.visible && b.depth < a.depth
					? std::array<std::uint8_t, 4>{0,255,0,255} : a.visible
					? std::array<std::uint8_t, 4>{255,0,0,255} : std::array<std::uint8_t, 4>{0,0,0,0};
				require(image.colors[y * side + x] == expected, "Intersecting ellipsoid order or interleaved source mismatch");
			}
	}

	// Depth switches and straight alpha are independent of proxy geometry
	sphere.color = {.8f,.4f,.2f,.5f};
	assign(sphereData, sphere);
	auto blended = take(fcg::EllipsoidRenderer::create(device, scene.target(),
		{.depthWrite = false, .cullMode = SDL_GPU_CULLMODE_NONE, .alphaBlending = true}));
	auto alpha = scene.render([&] (auto *commands, auto *pass) {
		check(blended.draw(sphereData, state, commands, pass));
	});
	const auto center = alpha.colors[(side / 2) * side + side / 2];
	require(std::abs((int)center[0] - 102) <= 1 && std::abs((int)center[1] - 51) <= 1
		&& std::abs((int)center[2] - 26) <= 1 && std::abs((int)center[3] - 128) <= 1,
		"Alpha blending contributed more than one surface");
	require(alpha.depths[(side / 2) * side + side / 2] == 1, "Disabled depth writes changed the depth buffer");
	for (const auto options : {fcg::RendererOptions{.depthTest = false}, fcg::RendererOptions{.depthWrite = false},
		fcg::RendererOptions{.depthCompare = SDL_GPU_COMPAREOP_ALWAYS}})
	{
		auto unsorted = take(fcg::EllipsoidRenderer::create(device, scene.target(), options));
		auto image = scene.render([&] (auto *commands, auto *pass) {
			check(unsorted.draw(instances, state, commands, pass, {}, {.first = 1}));
			check(unsorted.draw(instances, state, commands, pass, {}, {.count = 1}));
		});
		require(image.colors[(side / 2) * side + side / 2] == std::array<std::uint8_t, 4>{255,0,0,255},
			"Depth pipeline option was not applied");
	}

	// Moved-from and invalid sources fail; unused attributes do not restrict the draw range
	auto moved = std::move(ellipsoids);
	require(moved.supports(A::Position) && moved.supports(A::Extent) && moved.supports(A::Orientation)
		&& moved.supports(A::Color) && !moved.supports(A::Radius) && !moved.supports(A::Normal), "Wrong ellipsoid attributes");
	(void)scene.render([&] (auto *commands, auto *pass) {
		fails(ellipsoids.draw(instances, state, commands, pass), fcg::RenderErrorCode::InvalidState);
		fails(moved.draw(instances, state, commands, pass, {}, {.first = 3}), fcg::RenderErrorCode::InvalidArgument);
		check(moved.draw(instances, state, commands, pass, {}, {.first = 2}));
		check(moved.draw(instances, state, commands, pass));
	});
	check(instances.setAttributes([] (fcg::PrimitiveAttributes::Update &update) {
		update.set<A::Extent>(std::span<const glm::vec3>{});
	}));
	(void)scene.render([&] (auto *commands, auto *pass) {
		fails(moved.draw(instances, state, commands, pass), fcg::RenderErrorCode::InvalidArgument);
		check(moved.draw(instances, state, commands, pass, {}, {.count = 0}));
	});
}

/// Check spherical UVs, orientation, pole behavior, color multiplication, and mip gradients at the seam.
void textures (fcg::Device &device)
{
	Scene scene(device);
	auto renderer = take(fcg::EllipsoidRenderer::create(device, scene.target()));
	fcg::PrimitiveAttributes attributes(device);
	fcg::RenderState state(device);
	state.loadProjectionMatrix(glm::orthoRH_ZO(-2.f,2.f,-2.f,2.f,.1f,10.f));
	auto sampled = texture(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
	std::vector<std::array<std::uint8_t, 4>> texels(side * side);
	for (Uint32 y = 0; y < side; ++y)
		for (Uint32 x = 0; x < side; ++x)
			texels[y * side + x] = {(std::uint8_t)(4*x), (std::uint8_t)(4*y), 128, 128};
	check(sampled.upload(std::as_bytes(std::span(texels))));
	SDL_GPUSamplerCreateInfo settings{};
	settings.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	settings.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	auto sampler = take(fcg::Sampler::create(device, settings));
	fcg::DrawOptions options{.texture = fcg::PrimitiveTexture{sampled.handle(), sampler.handle()}};
	Ellipsoid shape;
	shape.color = {.5f,.75f,1, .5f};
	for (int view = 0; view < 4; ++view)
	{
		shape.orientation = glm::angleAxis(view * glm::half_pi<float>(), glm::vec3(0,1,0));
		if (view == 3) {
			shape.orientation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(1,0,0));
			shape.position.x = .03125f;
			shape.position.y = -.03125f;
		}
		assign(attributes, shape);
		auto image = scene.render([&] (auto *commands, auto *pass) {
			check(renderer.draw(attributes, state, commands, pass, options));
		});
		for (Uint32 y = 20; y < 44; ++y)
			for (Uint32 x = 20; x < 44; ++x)
			{
				const auto hit = reference(shape, state.modelviewMatrix(), state.projectionMatrix(), x, y, SDL_GPU_CULLMODE_BACK);
				if (!hit.visible)
					continue;
				const auto sphere = glm::normalize(hit.sphere);
				const double u = .5 + (glm::dot(glm::dvec2(sphere.x, sphere.z), glm::dvec2(sphere.x, sphere.z)) > 1e-12
					? std::atan2(sphere.z, sphere.x) : 0.0) / glm::two_pi<double>();
				const double v = std::acos(glm::clamp(sphere.y, -1.0, 1.0)) / glm::pi<double>();
				const auto tx = std::min(side - 1, (Uint32)((u - std::floor(u)) * side));
				const auto ty = std::min(side - 1, (Uint32)(v * side));
				const auto texel = texels[ty * side + tx];
				for (int channel = 0; channel < 4; ++channel)
				{
					// Exact seam points can select either edge of this deliberately discontinuous test texture
					if (channel == 0 && (u < 1e-6 || u > 1.0 - 1e-6))
						continue;
					require(std::abs((int)image.colors[y * side + x][channel]
						- (int)std::round(texel[channel] * shape.color[channel])) <= 3,
						"Spherical UV mismatch in view " + std::to_string(view) + " at " + std::to_string(x) + ","
						+ std::to_string(y) + " channel " + std::to_string(channel) + ": "
						+ std::to_string(image.colors[y * side + x][channel]) + " expected "
						+ std::to_string(texel[channel] * shape.color[channel]));
				}
			}
	}

	// Coarse mip levels are blue, so an unwrapped seam derivative incorrectly selects blue
	auto mipmapped = texture(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER,
		SDL_GPU_SAMPLECOUNT_1, 256, 9);
	for (Uint32 level = 0; level < 9; ++level)
	{
		const auto size = 256u >> level;
		std::vector<std::array<std::uint8_t, 4>> pixels(size * size,
			level < 5 ? std::array<std::uint8_t, 4>{255,0,0,255} : std::array<std::uint8_t, 4>{0,0,255,255});
		check(mipmapped.upload(std::as_bytes(std::span(pixels)), {.mipLevel = level, .extent = {size,size,1}}));
	}
	settings.min_filter = SDL_GPU_FILTER_LINEAR;
	settings.mag_filter = SDL_GPU_FILTER_LINEAR;
	settings.max_lod = 8;
	auto mipSampler = take(fcg::Sampler::create(device, settings));
	options.texture = fcg::PrimitiveTexture{mipmapped.handle(), mipSampler.handle()};
	shape = {};
	shape.color = glm::vec4(1);
	shape.orientation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0,1,0));
	assign(attributes, shape);
	auto image = scene.render([&] (auto *commands, auto *pass) {
		check(renderer.draw(attributes, state, commands, pass, options));
	});
	for (Uint32 y = 30; y < 34; ++y)
		for (Uint32 x = 30; x < 34; ++x)
			require(image.colors[y * side + x] == std::array<std::uint8_t, 4>{255,0,0,255}, "Seam selected an excessive mip level");
}

/// Exercise per-sample analytic silhouettes and rendering without a depth attachment.
void multisampling (fcg::Device &device)
{
	fcg::PrimitiveAttributes attributes(device);
	fcg::RenderState state(device);
	state.loadProjectionMatrix(glm::perspectiveRH_ZO(glm::radians(60.f), 1.f, .1f, 100.f));
	Ellipsoid shape;
	shape.color = glm::vec4(1);
	assign(attributes, shape);
	for (auto samples : {SDL_GPU_SAMPLECOUNT_1, SDL_GPU_SAMPLECOUNT_2, SDL_GPU_SAMPLECOUNT_4, SDL_GPU_SAMPLECOUNT_8})
	{
		if (!SDL_GPUTextureSupportsSampleCount(device.handle(), SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, samples)
			|| !SDL_GPUTextureSupportsSampleCount(device.handle(), SDL_GPU_TEXTUREFORMAT_D32_FLOAT, samples))
			continue;
		Scene scene(device, samples);
		auto renderer = take(fcg::EllipsoidRenderer::create(device, scene.target()));
		auto image = scene.render([&] (auto *commands, auto *pass) {
			check(renderer.draw(attributes, state, commands, pass));
		});
		std::size_t partial = 0;
		for (const auto pixel : image.colors)
			if (pixel[3] > 0 && pixel[3] < 255)
				++partial;
		require((samples == SDL_GPU_SAMPLECOUNT_1) == (partial == 0), "Analytic silhouette was not evaluated per sample");
	}
	Scene noDepth(device, SDL_GPU_SAMPLECOUNT_1, false);
	auto renderer = take(fcg::EllipsoidRenderer::create(device, noDepth.target()));
	auto image = noDepth.render([&] (auto *commands, auto *pass) {
		check(renderer.draw(attributes, state, commands, pass));
	});
	verify(image, shape, state, "target without depth");
}

/// Measure completed frames at 1024 square pixels; enabled explicitly through the test environment.
void benchmark (fcg::Device &device)
{
	// Allocate targets and pipelines before timing any draws
	constexpr Uint32 resolution = 1024;
	auto color = texture(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, SDL_GPU_SAMPLECOUNT_1, resolution);
	auto depth = texture(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
		SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET, SDL_GPU_SAMPLECOUNT_1, resolution);
	auto renderer = take(fcg::EllipsoidRenderer::create(device,
		{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_SAMPLECOUNT_1}));
	fcg::PrimitiveAttributes attributes(device);
	fcg::RenderState state(device);
	fcg::DrawOptions options;
	options.lighting.enabled = true;
	for (int workload = 0; workload < 3; ++workload)
	{
		// Separate setup-heavy small glyphs from overlapping large glyphs and orthographic bounds
		const Uint32 width = workload == 0 ? 100 : 3;
		std::vector<glm::vec4> positions;
		for (Uint32 y = 0; y < width; ++y)
			for (Uint32 x = 0; x < width; ++x)
				positions.emplace_back(4.f * (x + .5f) / width - 2, 4.f * (y + .5f) / width - 2, -4, 1);
		check(attributes.setAttributes([&] (fcg::PrimitiveAttributes::Update &update) {
			update.set<A::Position>(std::span(positions));
			update.set<A::Extent>(glm::vec3(workload == 0 ? .015f : 1.5f));
		}));
		state.loadProjectionMatrix(workload == 2 ? glm::orthoRH_ZO(-2.f,2.f,-2.f,2.f,.1f,100.f)
			: glm::perspectiveRH_ZO(glm::radians(60.f), 1.f, .1f, 100.f));
		Uint64 start = 0;
		for (int frame = 0; frame < 6; ++frame)
		{
			// Two warmup frames exclude deferred pipeline initialization and uploads from the measurement
			if (frame == 2)
				start = SDL_GetPerformanceCounter();
			Commands commands(device);
			SDL_GPUColorTargetInfo colorInfo{};
			colorInfo.texture = color.handle();
			colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
			colorInfo.store_op = SDL_GPU_STOREOP_STORE;
			colorInfo.cycle = true;
			SDL_GPUDepthStencilTargetInfo depthInfo{};
			depthInfo.texture = depth.handle();
			depthInfo.clear_depth = 1;
			depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
			depthInfo.store_op = SDL_GPU_STOREOP_STORE;
			depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
			depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
			depthInfo.cycle = true;
			commands.pass = SDL_BeginGPURenderPass(commands.handle, &colorInfo, 1, &depthInfo);
			require(commands.pass != nullptr, SDL_GetError());
			check(renderer.draw(attributes, state, commands.handle, commands.pass, options));
			SDL_EndGPURenderPass(std::exchange(commands.pass, nullptr));
			auto *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(std::exchange(commands.handle, nullptr));
			require(fence != nullptr, SDL_GetError());
			const bool completed = SDL_WaitForGPUFences(device.handle(), true, &fence, 1);
			SDL_ReleaseGPUFence(device.handle(), fence);
			require(completed, SDL_GetError());
		}
		const double milliseconds = 250.0 * (SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency();
		std::cout << "1024x1024 completion benchmark: " << positions.size() << (workload == 0 ? " small" : " large")
			<< (workload == 2 ? " orthographic" : " perspective") << " ellipsoids, " << milliseconds << " ms/frame\n";
	}
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Exercise analytic ellipsoid rendering against an independent CPU reference.
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
		analytic(*device);
		interoperability(*device);
		textures(*device);
		multisampling(*device);
		if (std::getenv("FCG_ELLIPSOID_BENCHMARK"))
			benchmark(*device);
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
