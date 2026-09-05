
//////
//
// Includes
//

// C++ STL
#include <format>
#include <string>
#include <string_view>

// cmrc (embedded resource filesystem)
#include <cmrc/cmrc.hpp>

// FCG Framework
#include <FCG/res.h>



//////
//
// Non-include prototypes
//

// cmrc library
namespace cmrc::res {
	// the embedded resource filesystem — defined by the generated fcg-resources library (see cmake/Shaders.cmake).
	auto get_filesystem () -> cmrc::embedded_filesystem;
}



//////
//
// Module namespace open
//

/// Our module namespace.
namespace fcg::res {



//////
//
// Local details
//

namespace {

/// The file suffix of each shader stage.
constexpr const char* stageSuffix (ShaderStage stage) {
	switch (stage) {
		case ShaderStage::VERTEX:   return "vert";
		case ShaderStage::FRAGMENT: return "frag";
		case ShaderStage::COMPUTE:  return "comp";
	}
	return "";
}

/// Try to open a resource file; nullopt instead of cmrc's file-not-found exception.
auto tryOpen (const cmrc::embedded_filesystem &fs, const std::string &path)
	-> std::optional<cmrc::file>
{
	if (!fs.is_file(path))
		return std::nullopt;
	return fs.open(path);
}

/// Map an embedded resource file to a byte span.
auto asBytes (const cmrc::file &file) -> std::span<const std::byte> {
	return {
		reinterpret_cast<const std::byte*>(file.begin()),
		file.size()
	};
}

/// Map an embedded resource file to a string view.
auto asStringView (const cmrc::file &file) -> std::string_view {
	return {file.begin(), file.size()};
}

} // namespace ::(local)



//////
//
// Class implementations
//

////
// res::Shader

auto Shader::stages () const -> std::vector<ShaderStage> {
	std::vector<ShaderStage> result;
	for (std::size_t i = 0; i < stageData.size(); i ++)
		if (stageData[i])
			result.push_back(static_cast<ShaderStage>(i));
	return result;
}

auto Shader::stage (ShaderStage stage) const -> std::optional<ShaderStageData> {
	const auto &entry = stageData[static_cast<std::size_t>(stage)];
	if (!entry)
		return std::nullopt;
	return *entry;
}



////
// Functions

auto shader (std::string_view name) -> std::optional<Shader>
{
	// Guard the lookup paths we build from the name
	if (name.empty() || name.find('/') != std::string_view::npos)
		return std::nullopt;

	const cmrc::embedded_filesystem fs = cmrc::res::get_filesystem();

	Shader result;
	result.m_name = name;

	// Probe each stage's compiled blob; the raw source sits right next to it
	for (auto stage : {ShaderStage::VERTEX, ShaderStage::FRAGMENT, ShaderStage::COMPUTE}) {
		const std::string base = std::format("{}/{}.{}", name, name, stageSuffix(stage));
		auto spirv = tryOpen(fs, base + ".spv");
		if (!spirv)
			continue;
		auto source = tryOpen(fs, base + ".glsl");
		result.stageData[static_cast<std::size_t>(stage)] = ShaderStageData {
			.stage = stage,
			.spirv = asBytes(*spirv),
			.source = source ? asStringView(*source) : std::string_view(),
		};
	}

	if (result.stages().empty())
		return std::nullopt;
	return result;
}



//////
//
// Namespaces close
//

// Our module namespace
} // namespace fcg::res
