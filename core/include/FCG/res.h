
#ifndef __FCG_RES_H__
#define __FCG_RES_H__


//////
//
// Module documentation
//

/// \defgroup fcg_resources Resources
/// \ingroup fcg_modules
/// \brief Access to shaders that the build system compiled and embedded into the binary.
///
/// Shaders are written as GLSL, compiled to SPIR-V and embedded into the binary by the build system. This module is
/// the runtime side: look up an embedded shader by name with <tt>\ref fcg::res::shader "fcg::res::shader"</tt>, take
/// the stage data you need, and hand it to <tt>\ref fcg::Device::createShader "Device::createShader"</tt>. A
/// <tt>\ref fcg::res::Shader "Shader"</tt> bundles the stages of one shader; the framework's built-in shaders are
/// reachable without a resource filesystem through the name-only overload.
///
/// \section fcg_resources_usage Getting started
/// Embed your shaders with CMake, link the generated target, include its generated header, and look the shader up by
/// name:
/// \code{.cmake}
/// fcg_add_shader(simple_shape SOURCES simple_shape/simple_shape.vert.glsl simple_shape/simple_shape.frag.glsl)
/// fcg_finalize_shaders(TARGET my-shaders NAMESPACE res::shaders)
/// target_link_libraries(my_app PRIVATE my-shaders)
/// \endcode
/// \snippet resource_examples.cpp resource_usage
///
/// \section fcg_resources_workflows Common tasks
/// \subsection fcg_resources_workflows_stages Working with stages
/// <tt>\ref fcg::res::Shader::stages "Shader::stages"</tt> lists which stages a shader provides, and
/// <tt>\ref fcg::res::Shader::stage "Shader::stage"</tt> returns the compiled SPIR-V bytecode of one stage, ready for
/// <tt>\ref fcg::Device::createShader "Device::createShader"</tt>. The optional
/// <tt>\ref fcg::res::ShaderStageData::source "source"</tt> field carries the GLSL text for diagnostics.
///
/// \subsection fcg_resources_workflows_builtin Built-in shaders
/// The framework's own shaders (e.g. the fullscreen passes') are embedded into Core and can be looked up directly:
/// \snippet resource_examples.cpp builtin_shader
///
/// \section fcg_resources_lifetime Ownership and lifetime
/// Shaders are cheap to copy, and stage data refers to the embedded storage without copying it: the binary that
/// contains the resource target must stay loaded as long as the shader or its stage data is used. The
/// <tt>cpp_embedlib::EmbeddedFS</tt> object itself need not outlive anything.
///
/// \section fcg_resources_errors Handling errors
/// An invalid name or an unknown shader yields \c std::nullopt instead of an error; a shader that lacks a requested
/// stage also yields \c std::nullopt from <tt>\ref fcg::res::Shader::stage "Shader::stage"</tt>.
///
/// \see \ref fcg_devices



//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// FCG Framework
#include <FCG/device.h>



//////
//
// Forward declarations
//

// cpp-embedlib types, as defined by the generated resource headers
namespace cpp_embedlib {
	class EmbeddedFS;
}



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// The library's namespace for resources embedded into binaries, such as compiled shaders.
namespace res {

/// \addtogroup fcg_resources
/// @{



//////
//
// Structs and enums
//

/// The embedded data of one stage of a <tt>\ref fcg::res::Shader "Shader"</tt>, for example to be passed to
/// <tt>\ref fcg::Device::createShader "Device::createShader"</tt>.
struct ShaderStageData
{
	/// The stage this data belongs to.
	ShaderStage stage;

	/// The compiled SPIR-V bytecode.
	std::span<const std::byte> spirv;

	/// The GLSL source text the bytecode was compiled from, for diagnostics. May be empty.
	std::string_view source;
};



//////
//
// Classes
//

/// \brief
/// 	A shader embedded into a binary with the build system's \c fcg_add_shader, consisting of one or more stages.
///
/// Obtain shaders with <tt>\ref fcg::res::shader "shader"</tt>. The stage data refers to the embedded storage
/// without copying it, so the binary that contains the shader must stay loaded as long as the shader or any stage
/// data obtained from it is used. Shaders are cheap to copy.
class FCG_FRAMEWORK_EXPORT Shader
{
	////
	// Friend declarations

	// Repeat the export macro: MSVC requires the friend and namespace declarations to agree on
	// dllexport linkage in Windows shared builds (C2375).
	/// Constructs shaders when looking them up in embedded resources.
	friend FCG_FRAMEWORK_EXPORT auto shader (const cpp_embedlib::EmbeddedFS &resources, std::string_view name)
		-> std::optional<Shader>;


public:

	////
	// Accessors

	/// The shader's name, as given to \c fcg_add_shader.
	[[nodiscard]] inline auto name () const -> const std::string& {
		return m_name;
	}

	/// \brief
	/// 	The stages this shader consists of.
	///
	/// \return The stages in the order vertex, fragment, compute, restricted to those present.
	[[nodiscard]] auto stages () const -> std::vector<ShaderStage>;

	/// \brief
	/// 	Looks up the embedded data of one stage.
	///
	/// \param stage The stage to look up.
	///
	/// \return The stage data, or \c std::nullopt if the shader has no such stage.
	[[nodiscard]] auto stage (ShaderStage stage) const -> std::optional<ShaderStageData>;


private:

	////
	// Object construction/destruction

	/// Private default constructor. Shaders are created by <tt>\ref fcg::res::shader "shader"</tt>.
	Shader() = default;


	////
	// Fields

	/// The shader's name.
	std::string m_name;

	/// Per-stage data, indexed by the \c ShaderStage value. Empty optionals indicate stages the shader does not have.
	std::array<std::optional<ShaderStageData>, 3> stageData;
};



//////
//
// Functions
//

/// \brief
/// 	Looks up a shader in the given embedded resource filesystem.
///
/// \param resources
/// 	The filesystem generated by \c fcg_finalize_shaders. The returned shader refers to its file data, so the binary
/// 	containing that data must stay loaded as long as the shader is used; the filesystem object itself need not.
/// \param name
/// 	The shader name as given to \c fcg_add_shader. Valid names consist of ASCII letters, digits, underscores and
/// 	hyphens.
///
/// \return
/// 	The shader, or \c std::nullopt if \p name is invalid or no stage of that name is embedded.
[[nodiscard]] FCG_FRAMEWORK_EXPORT auto shader (
	const cpp_embedlib::EmbeddedFS &resources, std::string_view name
) -> std::optional<Shader>;

/// \brief
/// 	Looks up one of the framework's built-in shaders.
///
/// \param name The shader name.
///
/// \return The shader, or \c std::nullopt if no built-in shader of that name exists.
[[nodiscard]] FCG_FRAMEWORK_EXPORT auto shader (std::string_view name) -> std::optional<Shader>;



//////
//
// Namespaces close
//

/// @}

// namespace res
}

// namespace fcg
}


#endif // ifndef __FCG_RES_H__
