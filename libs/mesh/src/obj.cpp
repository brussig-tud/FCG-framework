
//////
//
// Includes
//

// C++ STL
#include <charconv>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>

// tinyobjloader library
#include <tiny_obj_loader.h>

// Framework
#include <FCG/Mesh/obj.h>
#include "mesh_internal.h"



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// ObjMeshFormatHandler

auto ObjMeshFormatHandler::fileFormats () const -> std::vector<MeshFileFormat> {
	return {{"Wavefront OBJ", {"obj"}}};
}

auto ObjMeshFormatHandler::accepts (std::span<const std::byte>, std::string_view hint) const
	-> std::expected<bool, MeshError> { return detail::meshHint(hint) == "obj"; }

auto ObjMeshFormatHandler::decode (
	std::span<const std::byte> bytes, std::string_view hint, MeshLoaderOptions options
) const -> std::expected<Mesh, MeshError>
{
	if (hint != "obj")
		return std::unexpected(MeshError{MeshErrorCode::UnsupportedFormat, "OBJ requires an obj hint"});
	try
	{
		// Validate numeric records and remove inline comments before the callback parser sees them
		std::istringstream lines{std::string(detail::meshText(bytes))};
		std::string line, text;
		while (std::getline(lines, line))
		{
			line.resize(line.find('#') == line.npos ? line.size() : line.find('#'));
			std::istringstream record(line);
			std::string key, token;
			record >> key;
			std::size_t count = 0;
			while (record >> token)
			{
				++count;
				if (key == "v" || key == "vn" || key == "vt") {
					float value;
					const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
					if (error != std::errc{} || end != token.data() + token.size() || !std::isfinite(value))
						throw std::runtime_error("Invalid OBJ coordinate or attribute");
				} else if (key == "f") {
					std::size_t start = 0, component = 0;
					do
					{
						const auto end = token.find('/', start);
						const auto field = std::string_view(token).substr(start, end == token.npos ? token.size() - start : end - start);
						if (field.empty() && component == 0)
							throw std::runtime_error("Missing OBJ vertex index");
						if (!field.empty()) {
							int index;
							const auto [last, error] = std::from_chars(field.data(), field.data() + field.size(), index);
							if (error != std::errc{} || last != field.data() + field.size() || index == 0)
								throw std::runtime_error("Invalid OBJ index");
						}
						if (++component > 3)
							throw std::runtime_error("Too many OBJ corner indices");
						if (end == token.npos)
							break;
						start = end + 1;
					} while (true);
				}
			}
			if ((key == "v" && count != 3 && count != 4 && count != 6)
				|| (key == "vn" && count != 3) || (key == "vt" && (count < 1 || count > 3))
				|| (key == "f" && count < 3))
				throw std::runtime_error("Invalid OBJ record length");
			text += line + '\n';
		}

		// Capture raw index triples and attribute arrays without requesting external material files
		struct Pending {
			std::vector<tinyobj::index_t> indices;
			std::size_t vertices, normals, uvs;
		};
		struct Context {
			detail::MeshSource source;
			std::vector<Mesh::Normal> normals;
			std::vector<Mesh::TexCoord2D> uvs;
			std::vector<Pending> faces;
		} context;
		tinyobj::callback_t callbacks;
		callbacks.vertex_color_cb = [] (void *ptr, auto x, auto y, auto z, auto r, auto g, auto b, bool colored) {
			auto &state = *static_cast<Context *>(ptr);
			detail::MeshVertex vertex{Mesh::Point(x, y, z), {}, {}, {}};
			if (colored)
				vertex.color = Mesh::Color(r, g, b, 1);
			state.source.vertices.push_back(vertex);
		};
		callbacks.normal_cb = [] (void *ptr, auto x, auto y, auto z) {
			static_cast<Context *>(ptr)->normals.emplace_back(x, y, z);
		};
		callbacks.texcoord_cb = [] (void *ptr, auto x, auto y, auto) {
			static_cast<Context *>(ptr)->uvs.emplace_back(x, y);
		};
		callbacks.index_cb = [] (void *ptr, tinyobj::index_t *indices, int count) {
			auto &state = *static_cast<Context *>(ptr);
			state.faces.push_back({{indices, indices + count}, state.source.vertices.size(), state.normals.size(), state.uvs.size()});
		};
		std::istringstream input(text);
		std::string warning, error;
		if (!tinyobj::LoadObjWithCallback(input, callbacks, &context, nullptr, &warning, &error))
			throw std::runtime_error(error.empty() ? "OBJ parsing failed" : error);

		// Resolve negative references at the source face and positive references against the complete arrays
		const auto resolve = [] (int index, std::size_t current, std::size_t total) -> std::size_t {
			const auto value = index < 0 ? static_cast<std::int64_t>(current) + index : static_cast<std::int64_t>(index) - 1;
			if (value < 0 || static_cast<std::uint64_t>(value) >= total)
				throw std::runtime_error("OBJ index is outside its source array");
			return static_cast<std::size_t>(value);
		};
		for (const auto &pending : context.faces)
		{
			detail::MeshPolygon face;
			for (const auto &index : pending.indices) {
				detail::MeshCorner corner{resolve(index.vertex_index, pending.vertices, context.source.vertices.size()), {}, {}};
				if (index.normal_index)
					corner.normal = context.normals[resolve(index.normal_index, pending.normals, context.normals.size())];
				if (index.texcoord_index)
					corner.uv = context.uvs[resolve(index.texcoord_index, pending.uvs, context.uvs.size())];
				face.corners.push_back(corner);
			}
			context.source.polygons.push_back(std::move(face));
		}
		return detail::constructMesh(context.source, options);
	} catch (const std::runtime_error &error) {
		return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, error.what()});
	}
}



//////
//
// Module namespace close
//

// namespace fcg
}
