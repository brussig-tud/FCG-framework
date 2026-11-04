
//////
//
// Includes
//

// C++ STL
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <locale>
#include <sstream>
#include <stdexcept>

// Framework
#include <FCG/Mesh/stl.h>
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
// StlMeshFormatHandler

auto StlMeshFormatHandler::fileFormats () const -> std::vector<MeshFileFormat> {
	return {{"Stereolithography", {"stl"}}};
}

auto StlMeshFormatHandler::accepts (std::span<const std::byte> bytes, std::string_view hint) const
	-> std::expected<bool, MeshError>
{
	if (detail::meshHint(hint) == "stl")
		return true;
	const auto text = detail::meshText(bytes);
	if (text.starts_with("solid") && text.find("facet") != text.npos)
		return true;
	if (bytes.size() < 84)
		return false;
	std::uint32_t count = 0;
	for (unsigned i = 0; i < 4; ++i)
		count |= std::to_integer<std::uint32_t>(bytes[80 + i]) << (8 * i);
	return count == (bytes.size() - 84) / 50 && (bytes.size() - 84) % 50 == 0;
}

auto StlMeshFormatHandler::decode (
	std::span<const std::byte> bytes, std::string_view, MeshLoaderOptions options
) const -> std::expected<Mesh, MeshError>
{
	try
	{
		// Read little-endian binary words without alignment or host-endian assumptions
		const auto word = [&] (std::size_t offset) -> std::uint32_t {
			if (offset > bytes.size() || bytes.size() - offset < 4)
				throw std::runtime_error("Truncated binary STL record");
			std::uint32_t value = 0;
			for (unsigned i = 0; i < 4; ++i)
				value |= std::to_integer<std::uint32_t>(bytes[offset + i]) << (8 * i);
			return value;
		};
		const bool binary = bytes.size() >= 84 && (bytes.size() - 84) % 50 == 0
			&& word(80) == (bytes.size() - 84) / 50;
		detail::MeshSource source;
		std::map<std::array<float, 3>, std::size_t> vertices;
		const auto append = [&] (const std::array<Mesh::Point, 3> &points, const std::optional<Mesh::Normal> &normal) {
			detail::MeshPolygon face{{}, normal, {}};
			for (const auto &point : points) {
				if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2]))
					throw std::runtime_error("Non-finite STL position");
				auto [it, inserted] = vertices.emplace(std::array{point[0], point[1], point[2]}, source.vertices.size());
				if (inserted)
					source.vertices.push_back({point, {}, {}, {}});
				face.corners.push_back({it->second, {}, {}});
			}
			source.polygons.push_back(std::move(face));
		};
		if (binary)
		{
			for (std::size_t offset = 84; offset < bytes.size(); offset += 50) {
				const auto vector = [&] (std::size_t at) {
					return Mesh::Point(std::bit_cast<float>(word(at)), std::bit_cast<float>(word(at + 4)), std::bit_cast<float>(word(at + 8)));
				};
				append({vector(offset + 12), vector(offset + 24), vector(offset + 36)}, vector(offset));
			}
		} else
		{
			// Enforce complete ASCII facet structure, allowing absent facet normals without generating them
			std::istringstream input{std::string(detail::meshText(bytes))};
			input.imbue(std::locale::classic());
			std::string token, rest;
			if (!(input >> token) || token != "solid")
				throw std::runtime_error("Invalid STL header or binary record length");
			std::getline(input, rest);
			bool ended = false;
			const auto expect = [&] (const char *expected) {
				if (!(input >> token) || token != expected)
					throw std::runtime_error(std::string("Expected STL ") + expected);
			};
			while (input >> token)
			{
				if (token == "endsolid") {
					std::getline(input, rest);
					ended = true;
					input >> std::ws;
					if (!input.eof())
						throw std::runtime_error("Unexpected data after STL endsolid");
					break;
				}
				if (token != "facet")
					throw std::runtime_error("Expected STL facet");
				std::optional<Mesh::Normal> normal;
				if (!(input >> token))
					throw std::runtime_error("Truncated STL facet");
				if (token == "normal") {
					Mesh::Normal value;
					if (!(input >> value[0] >> value[1] >> value[2]))
						throw std::runtime_error("Invalid STL facet normal");
					normal = value;
					expect("outer");
				} else if (token != "outer")
					throw std::runtime_error("Expected STL normal or outer loop");
				expect("loop");
				std::array<Mesh::Point, 3> points;
				for (auto &point : points) {
					expect("vertex");
					if (!(input >> point[0] >> point[1] >> point[2]))
						throw std::runtime_error("Invalid STL vertex");
				}
				expect("endloop");
				expect("endfacet");
				append(points, normal);
			}
			if (!ended)
				throw std::runtime_error("Missing STL endsolid");
		}
		return detail::constructMesh(source, options);
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
