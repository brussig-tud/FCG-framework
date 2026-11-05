
#ifndef __FCG_RENDER_GPU_MESH_H__
#define __FCG_RENDER_GPU_MESH_H__


//////
//
// Module documentation
//

/// \defgroup fcg_gpu_meshes GPU meshes
/// \ingroup fcg_modules
/// \brief Immutable GPU triangle geometry with explicit vertex layouts.
///
/// Upload an OpenMesh object with <tt>\ref fcg::GPUMesh "GPUMesh"</tt> and draw it with
/// <tt>\ref fcg::MeshRenderer "MeshRenderer"</tt>. Source coordinates and winding are retained.
///
/// \section fcg_gpu_meshes_usage Getting started
/// Choose preprocessing for polygon inputs and missing smooth normals:
/// \snippet mesh_render_examples.cpp upload
///
/// \section fcg_gpu_meshes_workflows Common tasks
/// An explicit <tt>\ref fcg::GPUMeshLayout "GPUMeshLayout"</tt> allows creating a renderer before loading geometry.
/// Missing requested attributes receive geometric triangle normals, opaque white colors and zero UV coordinates.
/// Authored corner normals override vertex normals, then face normals; generated normals fill missing values.
/// Corner colors override vertex colors, then face colors. Corner UVs override vertex UVs.
/// The device must outlive GPU meshes; the source mesh is borrowed only during upload and remains unchanged.



//////
//
// Includes
//

// C++ STL
#include <limits>
#include <memory>
#include <numeric>

// Framework
#include <FCG/buffer.h>
#include <FCG/Render/error.h>
#include <FCG/Render/detail/mesh_data.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_gpu_meshes
/// @{



//////
//
// Structs and enums
//

/// Options independent of the concrete preprocessing callable types.
struct GPUMeshOptions {
	/// Requested GPU channels; absent channels receive flat normals, white color, or zero UVs.
	std::optional<GPUMeshLayout> layout;
};



//////
//
// Classes
//

/// Owns immutable indexed GPU geometry; its creating device must outlive it.
class FCG_RENDER_EXPORT GPUMesh
{
	////
	// Friend declarations

	/// The renderer binds the mesh's owned vertex and index buffers.
	friend class MeshRenderer;


public:

	////
	// Object construction/destruction

	/// Release buffers without waiting for submitted GPU work.
	~GPUMesh() = default;

	/// Not copyable.
	GPUMesh(const GPUMesh&) = delete;

	/// Not copy-assignable.
	auto operator= (const GPUMesh&) -> GPUMesh& = delete;

	/// Transfer buffer ownership; the source subsequently permits assignment and destruction only.
	///
	/// \param other Mesh to move.
	GPUMesh(GPUMesh &&other) noexcept = default;

	/// Replace buffers with transferred ownership.
	///
	/// \param other Mesh to move.
	///
	/// \return This mesh.
	auto operator= (GPUMesh &&other) noexcept -> GPUMesh& = default;

	/// Upload triangular geometry while leaving the borrowed source unchanged.
	///
	/// \pre The device is live and outlives the resulting mesh.
	///
	/// \tparam MeshT Compatible OpenMesh mesh type.
	/// \param device Device owning the uploaded buffers.
	/// \param mesh Borrowed source, retaining its coordinate and winding conventions.
	/// \param options Optional explicit layout; otherwise supplied channels determine the layout.
	///
	/// \return Complete geometry, or an argument, processing, or GPU operational error.
	///
	/// \throws ... Allocation exceptions propagate.
	template<class MeshT> [[nodiscard]] static auto create (
		Device &device, const MeshT &mesh, const GPUMeshOptions &options={}
	) -> std::expected<GPUMesh, RenderError> {
		return create(device, mesh, MeshPreprocessingPolicy<>{}, options);
	}

	/// Upload geometry with directly invoked preprocessing callbacks, leaving the source unchanged.
	///
	/// \pre Callback contracts of \c MeshPreprocessingPolicy hold; captured spans must not be retained.
	/// \pre The device is live and outlives the resulting mesh.
	///
	/// \tparam MeshT Compatible OpenMesh mesh type.
	/// \tparam Policy Concrete \c MeshPreprocessingPolicy type, borrowed without copying.
	///
	/// \param device Device owning the uploaded buffers.
	/// \param mesh Borrowed source geometry.
	/// \param policy Optional triangulation and missing-vertex-normal callbacks.
	/// \param options Optional explicit GPU channel selection.
	///
	/// \return Complete geometry or a face-specific processing, size-limit, or GPU operational error.
	///
	/// \throws ... Allocation and callback exceptions propagate.
	template<class MeshT, class Policy> requires requires(Policy &p) { p.triangulate; p.vertexNormals; }
	[[nodiscard]] static auto create (
		Device &device, const MeshT &mesh, Policy &&policy, const GPUMeshOptions &options={}
	) -> std::expected<GPUMesh, RenderError>
	{
		// Copy only geometry and supported attributes, retaining original vertex identities
		using Traits = MeshAttributeTraits<MeshT>;
		detail::GPUMeshData data;
		std::vector<std::size_t> vertexMap(mesh.n_vertices());
		for (auto vertex : mesh.vertices()) {
			vertexMap[vertex.idx()] = data.vertices.size();
			data.vertices.push_back({detail::meshPoint(mesh.point(vertex)), {}, {}});
			if (Traits::hasNormal(mesh, vertex)) data.vertices.back().normal = detail::meshPoint(mesh.normal(vertex));
		}
		const bool canTriangulate = policy.triangulate.has_value();
		std::vector<OpenMesh::Vec3d> positions;
		std::vector<detail::GPUCornerData> corners;
		for (auto face : mesh.faces())
		{
			positions.clear();
			corners.clear();
			for (auto halfedge : mesh.fh_range(face))
			{
				if (!canTriangulate && corners.size() == 3)
					return std::unexpected(RenderError{RenderErrorCode::InvalidArgument,
						"Non-triangle face " + std::to_string(face.idx())});
				const auto vertex = mesh.to_vertex_handle(halfedge);
				detail::GPUCornerData corner{vertexMap[vertex.idx()], {}, {}, {}};
				if (Traits::hasNormal(mesh, halfedge)) corner.normal = detail::meshPoint(mesh.normal(halfedge));
				else if (Traits::hasNormal(mesh, vertex)) corner.normal = detail::meshPoint(mesh.normal(vertex));
				else if (Traits::hasNormal(mesh, face)) corner.normal = detail::meshPoint(mesh.normal(face));
				if (Traits::hasColor(mesh, halfedge)) corner.color = colorValue(mesh.color(halfedge));
				else if (Traits::hasColor(mesh, vertex)) corner.color = colorValue(mesh.color(vertex));
				else if (Traits::hasColor(mesh, face)) corner.color = colorValue(mesh.color(face));
				if (Traits::hasTexCoord(mesh, halfedge)) {
					const auto &uv = mesh.texcoord2D(halfedge);
					corner.uv = {float(uv[0]), float(uv[1])};
				} else if (Traits::hasTexCoord(mesh, vertex)) {
					const auto &uv = mesh.texcoord2D(vertex);
					corner.uv = {float(uv[0]), float(uv[1])};
				}
				positions.push_back(data.vertices[corner.vertex].position);
				corners.push_back(corner);
			}
			if (corners.size() == 3) data.corners.insert(data.corners.end(), corners.begin(), corners.end());
			else
			{
				if (!canTriangulate) return std::unexpected(RenderError{RenderErrorCode::InvalidArgument,
					"Non-triangle face " + std::to_string(face.idx())});
				auto triangles = std::invoke(*policy.triangulate, MeshFaceView{positions});
				if (!triangles) return std::unexpected(RenderError{RenderErrorCode::InvalidArgument,
					"Face " + std::to_string(face.idx()) + ": " + triangles.error().message});
				for (const auto &triangle : *triangles)
					for (const auto corner : triangle) data.corners.push_back(corners[corner]);
			}
		}

		// Invoke the concrete weighting type directly, with one reusable output buffer
		if (policy.vertexNormals)
		{
			const auto incidence = detail::meshIncidence(data);
			std::vector<double> weights;
			auto &weighter = *policy.vertexNormals;
			for (std::size_t i = 0; i < data.vertices.size(); ++i)
				if (!data.vertices[i].normal) {
					const auto triangles = std::span(incidence.triangles).subspan(incidence.offsets[i],
						incidence.offsets[i + 1] - incidence.offsets[i]);
					weights.resize(triangles.size());
					std::invoke(weighter, triangles, std::span<double>(weights));
					data.vertices[i].generated = detail::weightedMeshNormal(triangles, weights);
				}
		}
		return upload(device, data, options);
	}


	////
	// Accessors

	/// Return the exact vertex format required of a compatible renderer.
	[[nodiscard]] auto layout () const -> GPUMeshLayout { return m_layout; }

	/// Return whether GPU vertices contain normals, including explicitly requested fallback normals.
	[[nodiscard]] auto hasNormals () const -> bool { return m_layout.normals; }

	/// Return whether GPU vertices contain RGBA colors.
	[[nodiscard]] auto hasColors () const -> bool { return m_layout.colors; }

	/// Return whether GPU vertices contain 2D texture coordinates.
	[[nodiscard]] auto hasTexCoords () const -> bool { return m_layout.texCoords; }

	/// Return the number of GPU vertices, including splits at attribute seams.
	[[nodiscard]] auto vertexCount () const -> std::size_t { return m_vertexCount; }

	/// Return the number of triangle-list indices.
	[[nodiscard]] auto indexCount () const -> std::size_t { return m_indexCount; }

	/// Return the number of drawable triangles.
	[[nodiscard]] auto triangleCount () const -> std::size_t { return m_indexCount / 3; }

	/// Return the borrowed creating device.
	[[nodiscard]] auto device () const -> Device& { return *m_device; }


private:

	////
	// Object construction

	/// Create an empty owner during an upload transaction.
	///
	/// \param device Borrowed creating device.
	explicit GPUMesh(Device &device) : m_device(&device) {}


	////
	// Methods

	/// Convert an OpenMesh RGB or RGBA color to the canonical GPU representation.
	///
	/// \tparam Color Source fixed-size vector type.
	/// \param value Source color, with integer components normalized by their maximum.
	///
	/// \return Floating-point RGBA values with alpha one for RGB inputs.
	template<class Color> static auto colorValue (const Color &value) -> std::array<float, 4> {
		std::array<float, 4> result{1, 1, 1, 1};
		using Scalar = std::remove_cvref_t<decltype(value[0])>;
		for (std::size_t i = 0; i < OpenMesh::vector_traits<Color>::size_; ++i)
			result[i] = float(value[i]) / (std::is_integral_v<Scalar> ? float(std::numeric_limits<Scalar>::max()) : 1.f);
		return result;
	}

	/// Pack and upload prepared geometry, cleaning up every failed transaction.
	///
	/// \param device Borrowed creating device.
	/// \param data Borrowed CPU preparation data.
	/// \param options Requested channel selection.
	///
	/// \return Complete GPU geometry or an operational error.
	static auto upload (Device &device, const detail::GPUMeshData &data, const GPUMeshOptions &options)
		-> std::expected<GPUMesh, RenderError>;


	////
	// Fields

	/// Borrowed creating device.
	Device *m_device;

	/// Exact canonical GPU vertex format.
	GPUMeshLayout m_layout;

	/// Owned interleaved vertex buffer; absent for empty geometry.
	std::optional<Buffer> m_vertices;

	/// Owned 32-bit index buffer; absent for empty geometry.
	std::optional<Buffer> m_indices;

	/// GPU vertex count after attribute splitting.
	std::size_t m_vertexCount = 0;

	/// Triangle-list index count.
	std::size_t m_indexCount = 0;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // __FCG_RENDER_GPU_MESH_H__
