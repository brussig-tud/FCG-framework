//////
//
// Includes
//

#ifndef __FCG_RES_H__
#define __FCG_RES_H__

// C++ STL
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// FCG Framework
#include <FCG/render_state.h>



namespace fcg {

namespace res {



//////
//
// Structs & enums
//

/// The embedded artifacts of one shader stage: the compiled SPIR-V blob and the raw GLSL source it was
/// built from (the latter kept for readable error messages from runtime shader translation).
struct ShaderStageData
{
	/// The stage this data set belongs to.
	ShaderStage stage;

	/// The compiled SPIR-V bytecode.
	std::span<const std::byte> spirv;

	/// The raw GLSL source code the SPIR-V was compiled from.
	std::string_view source;
};



//////
//
// Classes
//

/// A logical shader registered via the build system's \c fcg_add_shader, composed of one or more shader
/// stages. The embedded resources are process-static, so \c Shader instances can outlive anything.
class FCG_FRAMEWORK_EXPORT Shader
{
public:

	////
	// Accessors

	/// The shader's name, as given to \c fcg_add_shader.
	[[nodiscard]] inline auto name () const -> const std::string& {
		return m_name;
	}

	/// The stages this shader is composed of.
	[[nodiscard]] auto stages () const -> std::vector<ShaderStage>;

	/// Lookup of one stage's embedded data; \c std::nullopt if the shader has no such stage.
	[[nodiscard]] auto stage (ShaderStage stage) const -> std::optional<ShaderStageData>;


private:

	/// Only the lookup function constructs shaders.
	friend auto shader (std::string_view name) -> std::optional<Shader>;

	Shader () = default;

	/// The shader's name.
	std::string m_name;

	/// Per-stage data, indexed as the stage enum. Empty optionals indicate stages the shader does not have.
	std::array<std::optional<ShaderStageData>, 3> m_stageData;
};



////
// Functions

/// Look up an embedded logical shader by name.
///
/// \param name The shader name as given to \c fcg_add_shader.
///
/// \returns The shader, or \c std::nullopt if no shader of that name is embedded.
[[nodiscard]] FCG_FRAMEWORK_EXPORT auto shader (std::string_view name) -> std::optional<Shader>;



} // namespace res

} // namespace fcg



#endif // ifndef __FCG_RES_H__
