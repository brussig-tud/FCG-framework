
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

// tinyply library
#include <tinyply.h>

// Framework
#include <FCG/Mesh/ply.h>
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
// PlyMeshFormatHandler

auto PlyMeshFormatHandler::fileFormats () const -> std::vector<MeshFileFormat> {
	return {{"Polygon File Format", {"ply"}}};
}

auto PlyMeshFormatHandler::accepts (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<bool, MeshError>
{
	const auto text = detail::meshText(bytes);
	return detail::meshHint(hint) == "ply" || text.starts_with("ply\n") || text.starts_with("ply\r\n");
}

auto PlyMeshFormatHandler::decode (
	std::span<const std::byte> bytes, std::string_view, MeshLoaderOptions options
) const -> std::expected<Mesh, MeshError>
{
	try
	{
		// Parse declared properties independently so reordered and mixed scalar types retain their meaning
		const auto text = detail::meshText(bytes);
		if (!text.starts_with("ply\n") && !text.starts_with("ply\r\n"))
			throw std::runtime_error("Missing PLY signature");
		std::istringstream input{std::string(text)};
		input.imbue(std::locale::classic());
		std::string headerLine;
		bool formatFound = false;
		while (std::getline(input, headerLine))
		{
			std::istringstream record(headerLine);
			std::string key, encoding, version;
			record >> key;
			if (key == "end_header")
				break;
			if (key != "format")
				continue;
			if (formatFound || !(record >> encoding >> version) || version != "1.0"
				|| (encoding != "ascii" && encoding != "binary_little_endian" && encoding != "binary_big_endian"))
				throw std::runtime_error("Invalid or unsupported PLY format declaration");
			formatFound = true;
			if (encoding != "ascii")
				input.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);
		}
		if (!formatFound)
			throw std::runtime_error("Missing PLY format declaration");
		input.clear();
		input.seekg(0);
		tinyply::PlyFile file;
		if (!file.parse_header(input))
			throw std::runtime_error("Invalid PLY header");
		using Data = std::shared_ptr<tinyply::PlyData>;
		std::map<std::string, Data> vertices, faces;
		std::size_t vertexCount = 0, faceCount = 0;
		std::unordered_set<std::string> elements;
		for (const auto &element : file.get_elements())
		{
			if (element.size > bytes.size() || !elements.insert(element.name).second)
				throw std::runtime_error("Invalid PLY element count or duplicate element");
			const bool vertex = element.name == "vertex", face = element.name == "face";
			if (vertex) vertexCount = element.size;
			if (face) faceCount = element.size;
			std::unordered_set<std::string> names;
			for (const auto &property : element.properties)
			{
				if (!names.insert(property.name).second)
					throw std::runtime_error("Duplicate PLY property");
				const auto &name = property.name;
				const bool indices = face && (name == "vertex_indices" || name == "vertex_index");
				const bool cornerUV = face && name == "texcoord";
				const bool common = name == "nx" || name == "ny" || name == "nz" || name == "red"
					|| name == "green" || name == "blue" || name == "alpha";
				const bool positionUV = vertex && (name == "x" || name == "y" || name == "z" || name == "u" || name == "v"
					|| name == "s" || name == "t" || name == "texture_u" || name == "texture_v");
				if ((vertex || face) && (indices || cornerUV || common || positionUV)) {
					if (property.isList != (indices || cornerUV))
						throw std::runtime_error("Invalid PLY attribute property shape");
					if (indices && (property.propertyType == tinyply::Type::FLOAT32 || property.propertyType == tinyply::Type::FLOAT64))
						throw std::runtime_error("PLY vertex indices must be integers");
					(vertex ? vertices : faces)[name] = file.request_properties_from_element(element.name, {name});
				}
			}
		}
		if (!vertices.contains("x") || !vertices.contains("y") || !vertices.contains("z"))
			throw std::runtime_error("PLY requires vertex x, y and z properties");
		if (faceCount && !faces.contains("vertex_indices") && !faces.contains("vertex_index"))
			throw std::runtime_error("PLY faces require vertex indices");
		file.read(input);

		// Read native scalar values through memcpy to avoid alignment and aliasing assumptions
		const auto scalar = [] (const Data &data, std::size_t index, bool color=false) -> double {
			const auto read = [&]<class T> () -> double {
				if (index >= data->buffer.size_bytes() / sizeof(T))
					throw std::runtime_error("PLY property buffer is incomplete");
				T value;
				std::memcpy(&value, data->buffer.get_const() + index * sizeof(T), sizeof(T));
				if (color && std::is_integral_v<T>)
					return static_cast<double>(value) / std::numeric_limits<T>::max();
				return static_cast<double>(value);
			};
			switch (data->t) {
			case tinyply::Type::INT8: return read.template operator()<std::int8_t>();
			case tinyply::Type::UINT8: return read.template operator()<std::uint8_t>();
			case tinyply::Type::INT16: return read.template operator()<std::int16_t>();
			case tinyply::Type::UINT16: return read.template operator()<std::uint16_t>();
			case tinyply::Type::INT32: return read.template operator()<std::int32_t>();
			case tinyply::Type::UINT32: return read.template operator()<std::uint32_t>();
			case tinyply::Type::FLOAT32: return read.template operator()<float>();
			case tinyply::Type::FLOAT64: return read.template operator()<double>();
			default: throw std::runtime_error("Invalid PLY scalar type");
			}
		};
		// Reject finite doubles outside the destination float range before narrowing
		const auto floating = [&] (const Data &data, std::size_t index, bool color=false) -> float {
			const auto value = scalar(data, index, color);
			if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
				throw std::runtime_error("PLY value is not representable as a finite float");
			return static_cast<float>(value);
		};
		const auto count = [] (const Data &data, std::size_t index) -> std::size_t {
			if (!data->list_sizes.empty())
				return data->list_sizes.at(index);
			const auto stride = tinyply::PropertyTable.at(data->t).stride;
			return data->count ? data->buffer.size_bytes() / static_cast<std::size_t>(stride) / data->count : 0;
		};
		const auto normal = [&] (const auto &properties, std::size_t index) -> std::optional<Mesh::Normal> {
			const auto n = properties.contains("nx") + properties.contains("ny") + properties.contains("nz");
			if (n && n != 3)
				throw std::runtime_error("Incomplete PLY normal");
			if (!n) return {};
			return Mesh::Normal(floating(properties.at("nx"), index), floating(properties.at("ny"), index), floating(properties.at("nz"), index));
		};
		const auto color = [&] (const auto &properties, std::size_t index) -> std::optional<Mesh::Color> {
			const auto n = properties.contains("red") + properties.contains("green") + properties.contains("blue");
			if ((n && n != 3) || (!n && properties.contains("alpha")))
				throw std::runtime_error("Incomplete PLY color");
			if (!n) return {};
			return Mesh::Color(floating(properties.at("red"), index, true), floating(properties.at("green"), index, true),
				floating(properties.at("blue"), index, true), properties.contains("alpha") ? floating(properties.at("alpha"), index, true) : 1);
		};

		// Preserve vertex attributes independently of per-corner texture coordinates
		detail::MeshSource source;
		for (std::size_t i = 0; i < vertexCount; ++i)
		{
			detail::MeshVertex vertex{Mesh::Point(floating(vertices.at("x"), i), floating(vertices.at("y"), i), floating(vertices.at("z"), i)), normal(vertices, i), {}, color(vertices, i)};
			for (const auto &[u, v] : {std::pair{"u", "v"}, {"s", "t"}, {"texture_u", "texture_v"}}) {
				if (vertices.contains(u) != vertices.contains(v))
					throw std::runtime_error("Incomplete PLY texture coordinate");
				if (!vertex.uv && vertices.contains(u))
					vertex.uv = Mesh::TexCoord2D(floating(vertices.at(u), i), floating(vertices.at(v), i));
			}
			source.vertices.push_back(vertex);
		}
		std::size_t indexOffset = 0, uvOffset = 0;
		for (std::size_t i = 0; i < faceCount; ++i)
		{
			const auto indices = faces.at(faces.contains("vertex_indices") ? "vertex_indices" : "vertex_index");
			const auto size = count(indices, i);
			if (size > bytes.size())
				throw std::runtime_error("Invalid PLY face length");
			if (faces.contains("texcoord") && count(faces.at("texcoord"), i) != 2 * size)
				throw std::runtime_error("PLY corner texture coordinate count differs from face size");
			detail::MeshPolygon face{{}, normal(faces, i), color(faces, i)};
			for (std::size_t j = 0; j < size; ++j) {
				const auto value = scalar(indices, indexOffset++);
				if (value < 0 || value >= vertexCount)
					throw std::runtime_error("PLY vertex index is outside its array");
				detail::MeshCorner corner{static_cast<std::size_t>(value), {}, {}};
				if (faces.contains("texcoord")) {
					corner.uv = Mesh::TexCoord2D(floating(faces.at("texcoord"), uvOffset), floating(faces.at("texcoord"), uvOffset + 1));
					uvOffset += 2;
				}
				face.corners.push_back(corner);
			}
			source.polygons.push_back(std::move(face));
		}
		return detail::constructMesh(source, options);
	} catch (const std::runtime_error &error) {
		return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, error.what()});
	} catch (const std::invalid_argument &error) {
		return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, error.what()});
	}
}



//////
//
// Module namespace close
//

// namespace fcg
}
