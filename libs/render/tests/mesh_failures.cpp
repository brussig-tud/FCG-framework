
//////
//
// Includes
//

// C++ STL
#include <iostream>
#include <stdexcept>

// SDL3 library
#include <SDL3/SDL.h>

// Framework
#include <FCG/Render/gpu_mesh.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Resource boundary to fail during this upload.
enum class Failure
{
	/// Forward every operation.
	None,

	/// Fail vertex buffer allocation.
	Vertex,

	/// Fail index buffer allocation after vertices exist.
	Index,

	/// Fail the second staging operation.
	Stage,

	/// Fail upload submission.
	Submit,

	/// Throw during the second staging operation.
	Exception
};

/// Active injected failure.
Failure selected = Failure::None;

/// Number of buffer allocation attempts in the current operation.
unsigned attempts = 0;

/// Number of observed staging operations.
unsigned staged = 0;

/// Number of observed submissions.
unsigned submitted = 0;

/// Buffer allocation boundary forwarding genuine resource ownership.
class ProbedBuffer
{
public:
	////
	// Methods

	/// Allocate normally except at the selected resource boundary.
	static auto create (fcg::Device &device, std::size_t size, SDL_GPUBufferUsageFlags usage)
		-> std::expected<fcg::Buffer, fcg::BufferError>
	{
		++attempts;
		if ((selected == Failure::Vertex && attempts == 1) || (selected == Failure::Index && attempts == 2))
			return std::unexpected(fcg::BufferError{fcg::BufferErrorCode::SDLFailure, "Injected buffer failure"});
		return fcg::Buffer::create(device, size, usage);
	}
};

/// Staging boundary that keeps production resource cleanup active.
class ProbedUpload : public fcg::UploadBatch
{
public:
	////
	// Object construction

	/// Construct genuine staging state.
	using fcg::UploadBatch::UploadBatch;


	////
	// Methods

	/// Stage normally except after the first destination was staged.
	template<class T> auto upload (fcg::Buffer &destination, std::span<T> values)
		-> std::expected<void, fcg::BufferError>
	{
		++staged;
		if (staged == 2 && selected == Failure::Exception) throw std::runtime_error("Injected exception");
		if (staged == 2 && selected == Failure::Stage)
			return std::unexpected(fcg::BufferError{fcg::BufferErrorCode::SDLFailure, "Injected staging failure"});
		return fcg::UploadBatch::upload(destination, values);
	}

	/// Reject submission before publication when requested.
	auto submit () -> std::expected<void, fcg::BufferError> {
		++submitted;
		if (selected == Failure::Submit)
			return std::unexpected(fcg::BufferError{fcg::BufferErrorCode::SDLFailure, "Injected submission failure"});
		return fcg::UploadBatch::submit();
	}
};

// Anonymous namespace end
}



//////
//
// Includes
//

// Exercise the production packing/upload code with substituted resource boundaries
#define Buffer ProbedBuffer
#define UploadBatch ProbedUpload
#include "../src/gpu_mesh.cpp"
#undef UploadBatch
#undef Buffer



//////
//
// Functions
//

/// Exercise failure cleanup, callback exceptions, and immediate polygon rejection through the real factory.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
	try
	{
		auto device = fcg::Device::create();
		if (!device) throw std::runtime_error(SDL_GetError());
		fcg::Mesh mesh;
		auto a = mesh.add_vertex({0,0,0}), b = mesh.add_vertex({1,0,0}), c = mesh.add_vertex({0,1,0});
		mesh.add_face(a,b,c);
		for (auto failure : {Failure::Vertex, Failure::Index, Failure::Stage, Failure::Submit, Failure::Exception})
		{
			selected = failure;
			attempts = staged = submitted = 0;
			bool threw = false;
			try {
				if (fcg::GPUMesh::create(*device, mesh)) throw std::logic_error("Failed upload published geometry");
			} catch (const std::runtime_error&) { threw = true; }
			if (threw != (failure == Failure::Exception)) throw std::logic_error("Wrong exception behavior");
			selected = Failure::None;
			attempts = staged = submitted = 0;
			if (!fcg::GPUMesh::create(*device, mesh)) throw std::logic_error("Retry failed after resource cleanup");
		}
		// The first polygon must stop traversal before a later callback can run
		fcg::Mesh polygons;
		for (int group = 0; group < 2; ++group) {
			std::vector<fcg::Mesh::VertexHandle> v;
			for (const auto &p : std::array<OpenMesh::Vec3f,4>{{{0,0,0},{1,0,0},{1,1,0},{0,1,0}}})
				v.push_back(polygons.add_vertex(p));
			polygons.add_face(v);
		}
		unsigned calls = 0;
		auto policy = fcg::makeMeshPreprocessingPolicy([&] (fcg::MeshFaceView)
			-> std::expected<std::vector<fcg::MeshTriangle>, fcg::MeshError> {
				++calls;
				return std::unexpected(fcg::MeshError{fcg::MeshErrorCode::ProcessingFailure,"Injected callback failure"});
			}, std::nullopt);
		attempts = 0;
		if (fcg::GPUMesh::create(*device, polygons, policy) || calls != 1 || attempts != 0)
			throw std::logic_error("Factory performed a preliminary traversal or retained resources");
		auto throwing = fcg::makeMeshPreprocessingPolicy([] (fcg::MeshFaceView)
			-> std::expected<std::vector<fcg::MeshTriangle>, fcg::MeshError> {
				throw std::runtime_error("Callback exception");
			}, std::nullopt);
		bool caught = false;
		try { (void)fcg::GPUMesh::create(*device, polygons, throwing); }
		catch (const std::runtime_error&) { caught = true; }
		if (!caught) throw std::logic_error("Callback exception swallowed");
	} catch (const std::exception &error) { std::cerr << error.what() << '\n'; SDL_Quit(); return 1; }
	SDL_Quit();
	return 0;
}
