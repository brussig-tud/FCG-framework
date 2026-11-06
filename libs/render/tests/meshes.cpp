
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_set>

// SDL3 library
#include <SDL3/SDL.h>

// GLM library
#include <glm/gtc/matrix_transform.hpp>

// OpenMesh library
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>

// Framework
#include <FCG/texture.h>
#include <FCG/Render/gpu_mesh.h>
#include <FCG/Render/mesh_renderer.h>


// Local includes
#include "lighting_reference.h"


//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Fail independently of assertions.
void require (bool condition, const std::string &message) {
	if (!condition) throw std::runtime_error(message);
}

/// Unwrap results with their diagnostics.
template<class T, class Error> auto take (std::expected<T, Error> result) -> T {
	if (!result) throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Complete a checked operation.
template<class Error> void check (std::expected<void, Error> result) {
	if (!result) throw std::runtime_error(result.error().message);
}

/// Square geometry sharing its diagonal; optional reversed winding.
auto square (bool reverse=false) -> fcg::Mesh
{
	fcg::Mesh mesh;
	std::vector<fcg::Mesh::VertexHandle> vertices;
	for (const auto &p : std::array<OpenMesh::Vec3f, 4>{{{-1,-1,.5}, {1,-1,.5}, {1,1,.5}, {-1,1,.5}}})
		vertices.push_back(mesh.add_vertex(p));
	if (reverse) std::reverse(vertices.begin(), vertices.end());
	mesh.add_face(vertices[0], vertices[1], vertices[2]);
	mesh.add_face(vertices[0], vertices[2], vertices[3]);
	return mesh;
}

/// Render a mesh and download the center pixel, optionally drawing another mesh afterward.
auto draw (fcg::Device &device, const fcg::GPUMesh &mesh, const fcg::MeshDrawOptions &options={},
	SDL_GPUCullMode cull=SDL_GPU_CULLMODE_BACK, bool depth=false, bool blend=false,
	const fcg::GPUMesh *second=nullptr) -> std::array<std::uint8_t, 4>
{
	SDL_GPUTextureCreateInfo info{};
	info.type = SDL_GPU_TEXTURETYPE_2D;
	info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	info.width = info.height = 16;
	info.layer_count_or_depth = info.num_levels = 1;
	const auto color = take(fcg::Texture::create(device, info));
	std::optional<fcg::Texture> z;
	if (depth) {
		info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
		info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
		z.emplace(take(fcg::Texture::create(device, info)));
	}
	const fcg::RenderTargetInfo target{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		depth ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : SDL_GPU_TEXTUREFORMAT_INVALID};
	auto renderer = take(fcg::MeshRenderer::create(device, target, mesh.layout(), {.cullMode=cull, .alphaBlending=blend}));
	fcg::MeshRenderer moved(std::move(renderer));
	renderer = std::move(moved);
	fcg::RenderState state(device);
	state.loadModelviewMatrix(glm::scale(glm::mat4(1), glm::vec3(.9f, .8f, .7f)));
	auto *command = SDL_AcquireGPUCommandBuffer(device.handle());
	require(command != nullptr, SDL_GetError());
	SDL_GPUColorTargetInfo attachment{};
	attachment.texture = color.handle();
	attachment.load_op = SDL_GPU_LOADOP_CLEAR;
	attachment.store_op = SDL_GPU_STOREOP_STORE;
	SDL_GPUDepthStencilTargetInfo depthInfo{};
	depthInfo.texture = z ? z->handle() : nullptr;
	depthInfo.clear_depth = 1;
	depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
	depthInfo.store_op = SDL_GPU_STOREOP_STORE;
	auto *pass = SDL_BeginGPURenderPass(command, &attachment, 1, z ? &depthInfo : nullptr);
	require(pass != nullptr, SDL_GetError());
	renderer.draw(mesh, state, command, pass, options);
	if (second) renderer.draw(*second, state, command, pass, options);
	SDL_EndGPURenderPass(pass);
	require(SDL_SubmitGPUCommandBuffer(command), SDL_GetError());
	auto readback = take(color.readback());
	check(readback.wait());
	auto mapping = take(readback.map());
	return take(mapping.readTexel<std::array<std::uint8_t, 4>>({8,8,0}));
}

/// Test the type-independent packer without involving GPU allocations.
void packing ()
{
	fcg::detail::GPUMeshData data;
	data.vertices = {{{0,0,0}, {}, {}}, {{1,0,0}, {}, {}}, {{0,1,0}, {}, {}}, {{0,0,0}, {}, {}}};
	for (std::size_t v : {0,1,2,0,2,3}) data.corners.push_back({v, {}, {}, {}});
	auto packed = fcg::detail::packMesh(data, {});
	require(packed.vertices.size() == 12 && packed.indices[0] == packed.indices[3], "Source vertex mapping");
	require(packed.indices[0] != packed.indices[5], "Distinct coincident source vertices were welded");
	data.corners[3].uv = {1,1};
	packed = fcg::detail::packMesh(data, {});
	require(packed.indices[0] != packed.indices[3] && packed.layout.texCoords, "UV seam lost");
	packed = fcg::detail::packMesh(data, fcg::GPUMeshLayout{});
	require(packed.indices[0] == packed.indices[3], "Omitted UVs still split GPU vertices");
	const auto incidence = fcg::detail::meshIncidence(data);
	require(incidence.offsets[1] == 2 && incidence.triangles[0].positions[0] == OpenMesh::Vec3d(0,0,0), "Incidence");
}

/// Test public GPU conversion, shader layouts, appearance and fast mesh sharing.
void rendering (fcg::Device &device)
{
	auto source = square();
	std::unordered_set<fcg::GPUMeshLayout> layouts;
	for (unsigned key = 0; key < 8; ++key)
	{
		const fcg::GPUMeshLayout layout{bool(key & 1), bool(key & 2), bool(key & 4)};
		layouts.insert(layout);
		auto mesh = take(fcg::GPUMesh::create(device, source, {.layout=layout}));
		require(mesh.layout() == layout && mesh.vertexCount() == 4 && mesh.indexCount() == 6, "Explicit layout conversion");
		fcg::MeshDrawOptions options;
		options.color = {.8f,.4f,.2f,1};
		auto pixel = draw(device, mesh, options);
		require(std::abs(int(pixel[0]) - 204) <= 1 && pixel[3] == 255, "Unlit layout " + std::to_string(key));
		options.lighting.enabled = true;
		pixel = draw(device, mesh, options, SDL_GPU_CULLMODE_BACK, true);
		const auto litExpected = fcg::test::referencePixel(options.color, {0,0,1}, {0,0,1}, options.lighting);
		require(std::abs(int(pixel[0]) - litExpected[0]) <= 1,
			"Lit layout " + std::to_string(key) + " produced " + std::to_string(pixel[0]));
		if (layout.texCoords)
		{
			SDL_GPUTextureCreateInfo info{};
			info.type = SDL_GPU_TEXTURETYPE_2D;
			info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
			info.width = info.height = info.layer_count_or_depth = info.num_levels = 1;
			auto texture = take(fcg::Texture::create(device, info));
			const std::array<std::uint8_t, 4> texel{128,255,255,255};
			check(texture.upload(std::as_bytes(std::span(texel))));
			auto sampler = take(fcg::Sampler::create(device, {}));
			options.texture = fcg::PrimitiveTexture{texture.handle(), sampler.handle()};
			pixel = draw(device, mesh, options);
			require(std::abs(int(pixel[0]) - fcg::test::referencePixel(
				options.color * glm::vec4(128.f/255.f,1,1,1), {0,0,1}, {0,0,1}, options.lighting)[0]) <= 1, "Textured layout");

			// Colored specular reflection stays independent of the sampled material color
			options.lighting.specularColor = {.2f,.8f,.4f};
			options.lighting.shininess = 4.f;
			pixel = draw(device, mesh, options);
			const auto expected = fcg::test::referencePixel(options.color * glm::vec4(128.f/255.f,1,1,1),
				{0,0,1}, {0,0,1}, options.lighting);
			for (unsigned channel = 0; channel < 4; ++channel)
				require(std::abs(int(pixel[channel]) - expected[channel]) <= 1, "Textured specular material");
		}

		// Every normal/color/UV layout must transport explicit material properties and preserve alpha
		options.texture.reset();
		options.color = {1.f/32.f,.25f,2.f/3.f,.5f};
		for (const float exponent : {4.f, 48.f, 128.f})
		{
			options.lighting.shininess = exponent;
			options.lighting.specularColor = exponent == 48.f ? glm::vec3(0) : glm::vec3(.2f,.8f,.4f);
			pixel = draw(device, mesh, options);
			const auto expected = fcg::test::referencePixel(options.color, {0,0,1}, {0,0,1}, options.lighting);
			for (unsigned channel = 0; channel < 4; ++channel)
				require(std::abs(int(pixel[channel]) - expected[channel]) <= 1, "Mesh specular material");
		}
	}
	require(layouts.size() == 8, "Layout hashing");
	auto mesh = take(fcg::GPUMesh::create(device, source));
	require(!mesh.hasNormals() && !mesh.hasColors() && !mesh.hasTexCoords(), "Source property absence");
	const auto reversed = square(true);
	auto back = take(fcg::GPUMesh::create(device, reversed));
	require(draw(device, back)[3] == 0, "Back-face culling");
	fcg::MeshDrawOptions lit;
	lit.lighting.enabled = true;
	require(std::abs(int(draw(device, back, lit, SDL_GPU_CULLMODE_NONE)[0])
		- fcg::test::referencePixel(lit.color, {0,0,1}, {0,0,1}, lit.lighting)[0]) <= 1, "Two-sided flat lighting");
	lit.color = {1,0,0,.5f};
	const auto blended = draw(device, mesh, lit, SDL_GPU_CULLMODE_BACK, false, true);
	const auto blendedExpected = fcg::test::referencePixel(lit.color, {0,0,1}, {0,0,1}, lit.lighting);
	require(std::abs(int(blended[0]) - .5 * blendedExpected[0]) <= 1
		&& std::abs(int(blended[3]) - 128) <= 1, "Alpha blending");
	const auto shared = draw(device, mesh, {}, SDL_GPU_CULLMODE_BACK, true, false, &mesh);
	require(shared[0] == 255, "Shared renderer with depth");

	// A farther second mesh must be rejected by depth testing but overwrite without depth
	auto nearSource = square(), farSource = square();
	nearSource.request_vertex_colors();
	farSource.request_vertex_colors();
	for (auto vertex : nearSource.vertices()) {
		nearSource.set_color(vertex, {1,0,0,1});
		nearSource.data(vertex).hasColor = true;
	}
	for (auto vertex : farSource.vertices()) {
		farSource.set_color(vertex, {0,1,0,1});
		farSource.data(vertex).hasColor = true;
		auto point = farSource.point(vertex);
		point[2] = .8f;
		farSource.set_point(vertex, point);
	}
	auto nearMesh = take(fcg::GPUMesh::create(device, nearSource));
	auto farMesh = take(fcg::GPUMesh::create(device, farSource));
	const auto occluded = draw(device, nearMesh, {}, SDL_GPU_CULLMODE_BACK, true, false, &farMesh);
	const auto overdraw = draw(device, nearMesh, {}, SDL_GPU_CULLMODE_BACK, false, false, &farMesh);
	require(occluded[0] == 255 && occluded[1] == 0 && overdraw[0] == 0 && overdraw[1] == 255,
		"Mesh depth test/write behavior");
	auto smoothBack = take(fcg::GPUMesh::create(device, reversed, {.layout=fcg::GPUMeshLayout{.normals=true}}));
	require(std::abs(int(draw(device, smoothBack, {.lighting={.enabled=true}}, SDL_GPU_CULLMODE_NONE)[0])
		- fcg::test::referencePixel(glm::vec4(1), {0,0,1}, {0,0,1}, fcg::DirectionalLight{})[0]) <= 1,
		"Two-sided supplied-normal lighting");

	// Factory callbacks own move-only state and preserve authored face normals
	source.request_face_normals();
	for (auto f : source.faces()) { source.set_normal(f, {0,0,-1}); source.data(f).hasNormal = true; }
	unsigned calls = 0;
	auto policy = fcg::makeMeshPreprocessingPolicy(std::nullopt,
		[capture=std::make_unique<int>(0), &calls] (std::span<const fcg::MeshIncidentTriangle> triangles,
			std::span<double> weights) mutable {
			++*capture;
			++calls;
			fcg::vertexNormalAngleWeights(triangles, weights);
		});
	auto authored = take(fcg::GPUMesh::create(device, source, policy));
	require(calls == 4 && authored.hasNormals() && !source.has_vertex_normals(), "Missing normal callback or input mutation");
	const auto shaded = draw(device, authored, {.lighting={.enabled=true}});
	require(std::abs(int(shaded[0]) - 26) <= 1, "Authored face normals overwritten");

	// Authored corner normals take precedence over vertex and face normals
	source.request_vertex_normals();
	source.request_halfedge_normals();
	for (auto vertex : source.vertices()) {
		source.set_normal(vertex, {0,0,-1});
		source.data(vertex).hasNormal = true;
	}
	for (auto face : source.faces())
		for (auto halfedge : source.fh_range(face)) {
			source.set_normal(halfedge, {0,0,1});
			source.data(halfedge).hasNormal = true;
		}
	auto cornerNormals = take(fcg::GPUMesh::create(device, source));
	require(std::abs(int(draw(device, cornerNormals, {.lighting={.enabled=true}})[0])
		- fcg::test::referencePixel(glm::vec4(1), {0,0,1}, {0,0,1}, fcg::DirectionalLight{})[0]) <= 1, "Corner normal precedence");

	// Polygon failure is encountered during conversion; enabled preprocessing creates complete geometry
	fcg::Mesh polygon;
	std::vector<fcg::Mesh::VertexHandle> v;
	for (auto vertex : source.vertices()) v.push_back(polygon.add_vertex(source.point(vertex)));
	polygon.add_face(v);
	require(!fcg::GPUMesh::create(device, polygon), "Untriangulated input accepted without a policy");
	auto triangulate = fcg::makeMeshPreprocessingPolicy(fcg::triangulateEarCut, std::nullopt);
	auto triangulated = take(fcg::GPUMesh::create(device, polygon, triangulate));
	require(triangulated.triangleCount() == 2 && polygon.n_faces() == 1, "Polygon preprocessing changed input");
	fcg::Mesh empty;
	auto emptyGPU = take(fcg::GPUMesh::create(device, empty));
	require(emptyGPU.vertexCount() == 0 && draw(device, emptyGPU)[3] == 0, "Empty mesh");

	// Deleted polygon faces and sparse source handle indices are skipped
	auto deleted = square();
	std::vector<fcg::Mesh::VertexHandle> removed;
	for (const auto &point : std::array<OpenMesh::Vec3f,4>{{{0,0,0},{1,0,0},{1,1,0},{0,1,0}}})
		removed.push_back(deleted.add_vertex(point));
	const auto removedFace = deleted.add_face(removed);
	deleted.delete_face(removedFace, true);
	auto surviving = take(fcg::GPUMesh::create(device, deleted));
	require(surviving.triangleCount() == 2 && surviving.vertexCount() == 4, "Deleted source elements uploaded");

	// Generic OpenMesh RGB integer colors are normalized and alpha defaults to one
	OpenMesh::TriMesh_ArrayKernelT<> generic;
	auto a = generic.add_vertex({-1,-1,.5}), b = generic.add_vertex({1,-1,.5}), c = generic.add_vertex({0,1,.5});
	generic.add_face(a,b,c);
	generic.request_vertex_colors();
	for (auto vertex : generic.vertices()) generic.set_color(vertex, {128,64,255});
	auto colored = take(fcg::GPUMesh::create(device, generic));
	const auto rgb = draw(device, colored);
	require(rgb[0] == 128 && rgb[1] == 64 && rgb[2] == 255 && rgb[3] == 255, "Generic RGB normalization");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Run CPU packing and offscreen GPU mesh regressions.
auto main () -> int {
	if (!SDL_Init(SDL_INIT_VIDEO)) { std::cerr << SDL_GetError(); return 1; }
	try {
		packing();
		auto device = fcg::Device::create();
		require(device.has_value(), SDL_GetError());
		rendering(*device);
	} catch (const std::exception &error) { std::cerr << error.what() << '\n'; SDL_Quit(); return 1; }
	SDL_Quit();
	return 0;
}
