//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// FCG Framework
#include <FCG/res.h>

// Embedded test resources
#include <test-resources.h>
#include <test-bytecode-only.h>
#ifdef FCG_TEST_HELLO
#include <hello-resources.h>
#endif



//////
//
// Local details
//

namespace {

/// Fail the smoke test in every build configuration, including builds with assertions disabled.
auto require (bool condition, std::string_view message) -> void {
	if (!condition)
		throw std::runtime_error(std::string(message));
}

/// Read a build artifact for an optional byte-for-byte comparison with embedded storage.
auto readFile (const std::filesystem::path &path) -> std::string {
	std::ifstream input(path, std::ios::binary);
	require(input.is_open(), "Cannot open comparison artifact: " + path.string());
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

/// Check the stage contract and, when given an artifact root, the exact compiled and source bytes.
auto checkStage (
	const fcg::res::Shader &shader, fcg::ShaderStage stage, std::string_view suffix,
	const std::filesystem::path &root = {}
) -> void {
	const auto data = shader.stage(stage);
	require(data.has_value(), "Expected shader stage is missing");
	require(data->stage == stage, "Wrong shader stage");
	require(data->spirv.size() >= 20 && data->spirv.size() % 4 == 0, "Invalid SPIR-V size");
	const auto bytes = data->spirv;
	require(bytes[0] == std::byte{0x03} && bytes[1] == std::byte{0x02}
		&& bytes[2] == std::byte{0x23} && bytes[3] == std::byte{0x07}, "Invalid SPIR-V magic");
	require(data->source.starts_with("#version 450"), "GLSL source is missing");
	if (!root.empty()) {
		const auto base = root / shader.name() / (shader.name() + "." + std::string(suffix));
		const auto compiled = readFile(base.string() + ".spv");
		const auto source = readFile(base.string() + ".glsl");
		require(std::ranges::equal(bytes, std::as_bytes(std::span(compiled))), "Embedded SPIR-V differs from output");
		require(data->source == source, "Embedded GLSL differs from output");
	}
}

} // namespace ::(local)



//////
//
// Entry point
//

/// Exercise framework and caller-provided collections without initializing SDL or a GPU.
auto main (int argc, char **argv) -> int {
	try {
		const auto framework = fcg::res::shader("triangle");
		require(framework.has_value(), "Framework shader is missing");
		require(framework->name() == "triangle", "Wrong logical name");
		require(framework->stages() == std::vector{fcg::ShaderStage::VERTEX, fcg::ShaderStage::FRAGMENT},
			"Wrong framework stages or order");
		const std::filesystem::path frameworkRoot = argc > 1 ? argv[1] : "";
		checkStage(*framework, fcg::ShaderStage::VERTEX, "vert", frameworkRoot);
		checkStage(*framework, fcg::ShaderStage::FRAGMENT, "frag", frameworkRoot);
		require(!framework->stage(fcg::ShaderStage::COMPUTE), "Unexpected framework compute stage");

		// The copied filesystem dies on return; the returned shader must retain only views of static file data.
		const auto custom = [] {
			const auto resources = probe::embedded::FS;
			return fcg::res::shader(resources, "triangle");
		}();
		require(custom.has_value(), "Custom shader is missing");
		require(custom->stages() == std::vector{fcg::ShaderStage::COMPUTE}, "Resource namespaces are not isolated");
		checkStage(*custom, fcg::ShaderStage::COMPUTE, "comp", argc > 2 ? argv[2] : "");
		require(!custom->stage(fcg::ShaderStage::VERTEX), "Unexpected custom vertex stage");

		const auto bytecode = fcg::res::shader(probe::bytecode::FS, "triangle");
		require(bytecode.has_value(), "Shader without GLSL should still load");
		const auto compute = bytecode->stage(fcg::ShaderStage::COMPUTE);
		require(compute.has_value() && compute->source.empty(), "Missing GLSL should produce an empty view");
		require(std::ranges::equal(compute->spirv, custom->stage(fcg::ShaderStage::COMPUTE)->spirv),
			"Bytecode-only shader differs");

		for (const auto name : {"", "missing", ".", "..", "../triangle", "triangle/", "triangle\\", "./triangle"}) {
			require(!fcg::res::shader(name), "Invalid or unknown framework name resolved");
			require(!fcg::res::shader(probe::embedded::FS, name), "Invalid or unknown custom name resolved");
		}
		require(!fcg::res::shader("simple_shapes"), "Application resources leaked into framework lookup");

#ifdef FCG_TEST_HELLO
		const auto app = fcg::res::shader(hello::embedded::FS, "simple_shapes");
		require(app.has_value(), "Hello shader is missing");
		const std::filesystem::path appRoot = argc > 3 ? argv[3] : "";
		checkStage(*app, fcg::ShaderStage::VERTEX, "vert", appRoot);
		checkStage(*app, fcg::ShaderStage::FRAGMENT, "frag", appRoot);
		require(!fcg::res::shader(hello::embedded::FS, "triangle"), "Framework resources leaked into app lookup");
#endif
		return EXIT_SUCCESS;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return EXIT_FAILURE;
	}
}
