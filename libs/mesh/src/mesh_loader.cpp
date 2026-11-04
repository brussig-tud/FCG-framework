
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <fstream>
#include <limits>
#include <stdexcept>

// Framework
#include <FCG/Mesh/obj.h>
#include <FCG/Mesh/ply.h>
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
// MeshFormatHandler

auto MeshFormatHandler::load (const std::filesystem::path &path, MeshLoaderOptions options) const
	-> std::expected<Mesh, MeshError>
{
	// Read a complete owned byte buffer using filesystem-aware stream construction
	if (path.empty() || path.native().find(std::filesystem::path::value_type{}) != path.native().npos)
		return std::unexpected(MeshError{MeshErrorCode::InvalidArgument, "Mesh path is empty or contains a NUL"});
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if (!input)
		return std::unexpected(MeshError{MeshErrorCode::IOFailure, "Opening mesh file failed"});
	const auto size = input.tellg();
	if (size < 0 || static_cast<std::uintmax_t>(size) > std::numeric_limits<std::size_t>::max()
		|| size > std::numeric_limits<std::streamsize>::max())
		return std::unexpected(MeshError{MeshErrorCode::IOFailure, "Mesh file size cannot be read"});
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	input.seekg(0);
	if (size && !input.read(reinterpret_cast<char *>(bytes.data()), size))
		return std::unexpected(MeshError{MeshErrorCode::IOFailure, "Reading mesh file failed"});
	const auto extension = path.extension().u8string();
	return load(bytes, {reinterpret_cast<const char *>(extension.data()), extension.size()}, options);
}

auto MeshFormatHandler::load (
	std::span<const std::byte> bytes, std::string_view hint, MeshLoaderOptions options
) const -> std::expected<Mesh, MeshError>
{
	// Validate shared arguments before invoking a concrete decoder
	if (bytes.empty() || hint.find('\0') != hint.npos
		|| (options.topology != MeshTopologyPolicy::Reject && options.topology != MeshTopologyPolicy::Repair))
		return std::unexpected(MeshError{MeshErrorCode::InvalidArgument, "Invalid mesh input, hint or topology policy"});
	auto result = decode(bytes, detail::meshHint(hint), options);
	if (result && !result->n_vertices())
		return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, "Handler returned an empty mesh"});
	return result;
}


////
// MeshLoader

auto MeshLoader::global () -> MeshLoader&
{
	// Construct all built-ins through ordinary symbol references, retaining them in static archives
	static MeshLoader loader;
	static const bool initialized = [&] {
		const auto add = [&] (std::string id, std::unique_ptr<MeshFormatHandler> handler) {
			if (auto result = loader.registerHandler(std::move(id), std::move(handler), -100); !result)
				throw std::invalid_argument(result.error().message);
		};
		add("obj", std::make_unique<ObjMeshFormatHandler>());
		add("ply", std::make_unique<PlyMeshFormatHandler>());
		add("stl", std::make_unique<StlMeshFormatHandler>());
		return true;
	}();
	(void)initialized;
	return loader;
}

auto MeshLoader::registerHandler (std::string id, std::unique_ptr<MeshFormatHandler> handler, int priority)
	-> std::expected<void, MeshError>
{
	if (id.empty() || id.find('\0') != id.npos || !handler)
		return std::unexpected(MeshError{MeshErrorCode::InvalidArgument, "Registration requires an ID and handler"});
	if (std::ranges::any_of(m_handlers, [&] (const Entry &entry) { return entry.id == id; }))
		return std::unexpected(MeshError{MeshErrorCode::DuplicateHandler, "Duplicate mesh handler ID: " + id});
	Entry entry{std::move(id), std::move(handler), priority};
	const auto position = std::ranges::find_if(m_handlers, [&] (const Entry &other) {
		return entry.priority > other.priority || (entry.priority == other.priority && entry.id < other.id);
	});
	m_handlers.insert(position, std::move(entry));
	return {};
}

auto MeshLoader::removeHandler (std::string_view id) -> bool {
	return std::erase_if(m_handlers, [&] (const Entry &entry) { return entry.id == id; }) != 0;
}

auto MeshLoader::fileFormats () const -> std::vector<MeshFileFormat>
{
	std::vector<MeshFileFormat> result;
	for (const auto &entry : m_handlers) {
		auto formats = entry.handler->fileFormats();
		result.insert(result.end(), std::make_move_iterator(formats.begin()), std::make_move_iterator(formats.end()));
	}
	return result;
}

auto MeshLoader::load (const std::filesystem::path &path, MeshLoaderOptions options) const
	-> std::expected<Mesh, MeshError>
{
	// Reuse the file reader and forward its bytes to this registry through a short-lived handler
	class Dispatch final : public MeshFormatHandler
	{
	public:
		explicit Dispatch(const MeshLoader &loader) : m_loader(loader) {}
		auto accepts (std::span<const std::byte>, std::string_view) const -> std::expected<bool, MeshError> override {
			return true;
		}
		auto decode (std::span<const std::byte> bytes, std::string_view hint, MeshLoaderOptions opts) const
			-> std::expected<Mesh, MeshError> override { return m_loader.load(bytes, hint, opts); }
	private:
		const MeshLoader &m_loader;
	};
	return Dispatch(*this).load(path, options);
}

auto MeshLoader::load (
	std::span<const std::byte> bytes, std::string_view hint, MeshLoaderOptions options
) const -> std::expected<Mesh, MeshError>
{
	if (bytes.empty() || hint.find('\0') != hint.npos
		|| (options.topology != MeshTopologyPolicy::Reject && options.topology != MeshTopologyPolicy::Repair))
		return std::unexpected(MeshError{MeshErrorCode::InvalidArgument, "Invalid mesh input, hint or topology policy"});
	const auto normalized = detail::meshHint(hint);
	std::string failures;
	for (const auto &entry : m_handlers)
	{
		auto accepted = entry.handler->accepts(bytes, normalized);
		if (accepted && !*accepted)
			continue;
		if (!failures.empty())
			failures += '\n';
		if (!accepted) {
			failures += entry.id + " (probe): " + accepted.error().message;
			continue;
		}
		auto mesh = entry.handler->load(bytes, normalized, options);
		if (mesh)
			return mesh;
		failures += entry.id + ": " + mesh.error().message;
	}
	if (failures.empty())
		return std::unexpected(MeshError{MeshErrorCode::UnsupportedFormat, "No mesh handler accepted the input"});
	return std::unexpected(MeshError{MeshErrorCode::DecodeFailure, std::move(failures)});
}


////
// MeshFormatHandlerRegistration

MeshFormatHandlerRegistration::MeshFormatHandlerRegistration(
	std::string id, std::unique_ptr<MeshFormatHandler> handler, int priority
){
	if (auto result = MeshLoader::global().registerHandler(std::move(id), std::move(handler), priority); !result)
		throw std::invalid_argument(result.error().message);
}



//////
//
// Module namespace close
//

// namespace fcg
}
