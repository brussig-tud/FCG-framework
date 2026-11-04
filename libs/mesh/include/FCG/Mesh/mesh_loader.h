
#ifndef __FCG_MESH_MESH_LOADER_H__
#define __FCG_MESH_MESH_LOADER_H__


//////
//
// Module documentation
//

/// \defgroup fcg_mesh_loading Mesh loading
/// \ingroup fcg_modules
/// \brief Synchronous loading of polygon geometry from files and encoded memory.
///
/// Use \ref fcg::MeshLoader "<tt>MeshLoader</tt>" to load OBJ, PLY and STL files into an independently owned
/// \ref fcg::Mesh "<tt>Mesh</tt>". Supplied normals, colors and UVs are retained, including corner seams.
///
/// \section fcg_mesh_loading_usage Getting started
/// The global loader contains the built-in format handlers:
/// \snippet mesh_examples.cpp load
///
/// \section fcg_mesh_loading_workflows Common tasks
/// To load encoded memory, pass a byte span and a file-extension hint. Hints ignore a leading dot and ASCII case.
/// PLY and STL signatures can be recognized without a hint; OBJ requires an OBJ hint.
///
/// Select a topology policy through \ref fcg::MeshLoaderOptions "<tt>MeshLoaderOptions</tt>":
/// \snippet mesh_examples.cpp repair
/// Supported bow-tie vertices are accepted under both policies. Construction proceeds in file order, which can
/// affect whether a face can be inserted. Repair may change connectivity and may fail. Inspect the OpenMesh
/// \c fixed_nonmanifold status on vertices, edges and faces to identify elements affected by repair.
///
/// Construct a local loader for an explicit handler set, or implement a handler for another encoding:
/// \snippet mesh_examples.cpp custom
/// Handlers return owned meshes and receive input only for the duration of a call.



//////
//
// Includes
//

// C++ STL
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Framework
#include <FCG/Mesh/export.h>
#include <FCG/Mesh/error.h>
#include <FCG/Mesh/mesh.h>



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_mesh_loading
/// @{



//////
//
// Structs and enums
//

/// Policy for connectivity that prevents construction through OpenMesh's face insertion API.
enum class MeshTopologyPolicy {
	/// Fail when a source face cannot be inserted with its requested connectivity.
	Reject,

	/// Ask the handler to attempt implementation-defined repair; the attempt may fail.
	Repair
};

/// Options controlling mesh loading; source attributes are preserved and missing normals remain absent.
struct MeshLoaderOptions {
	/// Construction policy, defaulting to rejection of connectivity OpenMesh cannot insert.
	MeshTopologyPolicy topology = MeshTopologyPolicy::Reject;
};

/// A format advertised by a mesh handler for file-dialog filters.
struct MeshFileFormat {
	/// Human-readable format name.
	std::string name;

	/// Lowercase extensions without a leading dot.
	std::vector<std::string> extensions;
};



//////
//
// Classes
//

/// \brief Interface for synchronous decoders producing independently owned OpenMesh meshes.
///
/// Override \c accepts and <tt>decode</tt>; optionally advertise formats through <tt>fileFormats</tt>. Input is borrowed
/// for the call and must not be retained. Expected decoding failures use <tt>MeshError</tt>; exceptions propagate.
/// A handler must not modify the registry invoking it. Shared state across loaders requires synchronization.
class FCG_MESH_EXPORT MeshFormatHandler
{
public:

	////
	// Object construction/destruction

	/// Destroys the handler through its owning base pointer.
	virtual ~MeshFormatHandler() = default;


	////
	// Methods

	/// Returns advertised formats, or an empty list when the handler provides no filter information.
	[[nodiscard]] virtual auto fileFormats () const -> std::vector<MeshFileFormat> { return {}; }

	/// Checks whether decoding should be attempted; acceptance does not guarantee success.
	///
	/// \param bytes Encoded contents borrowed for the call.
	/// \param hint Normalized lowercase extension without a dot when called by the loader.
	/// \return Whether to attempt decoding, or a probe error; the registry continues after errors.
	[[nodiscard]] virtual auto accepts (std::span<const std::byte> bytes, std::string_view hint={}) const
		-> std::expected<bool, MeshError> = 0;

	/// Reads a file completely and decodes it using this handler, without probing first.
	///
	/// \param path Nonempty file path without NUL characters.
	/// \param options Construction policy.
	/// \return An owned mesh, or an argument, file I/O or decoding error.
	[[nodiscard]] auto load (const std::filesystem::path &path, MeshLoaderOptions options={}) const
		-> std::expected<Mesh, MeshError>;

	/// Decodes memory with a normalized hint and validates the returned mesh is nonempty.
	///
	/// \param bytes Nonempty encoded contents borrowed for the call.
	/// \param hint Extension hint; ASCII case and a leading dot are ignored.
	/// \param options Construction policy.
	/// \return An owned mesh, or an argument or decoding error; handler exceptions propagate.
	[[nodiscard]] auto load (
		std::span<const std::byte> bytes, std::string_view hint={}, MeshLoaderOptions options={}
	) const -> std::expected<Mesh, MeshError>;


protected:

	////
	// Methods

	/// Decodes normalized input and applies the requested construction policy.
	///
	/// \param bytes Nonempty encoded contents borrowed for the call.
	/// \param hint Lowercase extension without a leading dot.
	/// \param options Construction policy; repair can fail and is implementation-defined.
	/// \return An independently owned mesh containing at least one vertex, or an error.
	[[nodiscard]] virtual auto decode (
		std::span<const std::byte> bytes, std::string_view hint, MeshLoaderOptions options
	) const -> std::expected<Mesh, MeshError> = 0;
};

/// \brief A registry trying handlers in descending priority and ascending ID order until one succeeds.
///
/// A new loader is empty. All operations on one loader require external serialization. Handlers must not modify
/// their invoking loader. Probe and decoding failures are aggregated in dispatch order; exceptions propagate.
class FCG_MESH_EXPORT MeshLoader
{
public:

	////
	// Object construction/destruction

	/// Creates an empty handler registry.
	MeshLoader() = default;

	/// Destroys the registry and its handlers; no operation may be in progress.
	~MeshLoader() = default;

	/// Not copyable.
	MeshLoader(const MeshLoader&) = delete;

	/// Not copy-assignable.
	auto operator= (const MeshLoader&) -> MeshLoader& = delete;

	/// Not movable.
	MeshLoader(MeshLoader&&) = delete;

	/// Not move-assignable.
	auto operator= (MeshLoader&&) -> MeshLoader& = delete;


	////
	// Registry

	/// Returns the process-wide registry, initially containing OBJ, PLY and STL handlers at priority -100.
	[[nodiscard]] static auto global () -> MeshLoader&;

	/// Transfers ownership of a handler to the registry, consuming it even when registration fails.
	///
	/// \param id Unique nonempty ID without NUL characters.
	/// \param handler Non-null handler owned by the registry after registration.
	/// \param priority Higher values are tried first.
	/// \return Nothing, or an invalid-argument or duplicate-ID error; allocation exceptions leave the registry unchanged.
	[[nodiscard]] auto registerHandler (std::string id, std::unique_ptr<MeshFormatHandler> handler, int priority=0)
		-> std::expected<void, MeshError>;

	/// Removes and destroys the handler with the requested ID.
	///
	/// \param id Registration ID.
	/// \return Whether a handler was removed.
	auto removeHandler (std::string_view id) -> bool;

	/// Returns the number of registered handlers.
	[[nodiscard]] auto handlerCount () const -> std::size_t { return m_handlers.size(); }

	/// Returns the concatenated advertised formats in handler order, retaining duplicates.
	[[nodiscard]] auto fileFormats () const -> std::vector<MeshFileFormat>;


	////
	// Loading

	/// Reads a file completely and dispatches it using its extension as the hint.
	///
	/// \param path Nonempty file path without NUL characters.
	/// \param options Construction policy.
	/// \return An owned mesh, or an argument, file I/O or dispatch error.
	[[nodiscard]] auto load (const std::filesystem::path &path, MeshLoaderOptions options={}) const
		-> std::expected<Mesh, MeshError>;

	/// Dispatches encoded memory; no input references are retained and no results are cached.
	///
	/// \param bytes Nonempty encoded contents borrowed for the call.
	/// \param hint Extension; a leading dot and ASCII case are ignored. OBJ requires an OBJ hint.
	/// \param options Construction policy.
	/// \return An owned mesh, invalid argument, unsupported format if none accepts, or decode failure with handler diagnostics.
	[[nodiscard]] auto load (
		std::span<const std::byte> bytes, std::string_view hint={}, MeshLoaderOptions options={}
	) const -> std::expected<Mesh, MeshError>;


private:

	////
	// Types

	/// One owned decoder and its dispatch key.
	struct Entry
	{
		/// Unique diagnostic and ordering ID.
		std::string id;

		/// Owned decoder.
		std::unique_ptr<MeshFormatHandler> handler;

		/// Dispatch priority.
		int priority;
	};


	////
	// Fields

	/// Entries sorted by descending priority and ascending ID.
	std::vector<Entry> m_handlers;
};

/// \brief Permanently registers a handler with the global loader, including from static initialization.
///
/// A registrar in an otherwise unreferenced static-library translation unit needs an explicit linker anchor,
/// object-library linkage or whole-archive linkage to ensure registration takes place.
class FCG_MESH_EXPORT MeshFormatHandlerRegistration
{
public:

	////
	// Object construction/destruction

	/// Registers a handler with the global loader.
	///
	/// \param id Unique nonempty ID without NUL characters.
	/// \param handler Non-null handler whose ownership is transferred.
	/// \param priority Higher values are tried first.
	/// \throws std::invalid_argument If the ID or handler is invalid, or the ID already exists.
	MeshFormatHandlerRegistration(std::string id, std::unique_ptr<MeshFormatHandler> handler, int priority=0);

	/// Destroys the registrar; its handler remains registered.
	~MeshFormatHandlerRegistration() = default;

	/// Not copyable.
	MeshFormatHandlerRegistration(const MeshFormatHandlerRegistration&) = delete;

	/// Not copy-assignable.
	auto operator= (const MeshFormatHandlerRegistration&) -> MeshFormatHandlerRegistration& = delete;
};



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif // __FCG_MESH_MESH_LOADER_H__
