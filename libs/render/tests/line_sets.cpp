
//////
//
// Includes
//

// C++ STL
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// FCG Framework
#include <FCG/Render/line_set_builder.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Attribute shorthand.
using A = fcg::Attribute;

static_assert(!std::is_copy_constructible_v<fcg::LineSet>);
static_assert(!std::is_copy_assignable_v<fcg::LineSet>);
static_assert(std::is_nothrow_move_constructible_v<fcg::LineSet>);
static_assert(std::is_nothrow_move_assignable_v<fcg::LineSet>);

/// Fail independently of release assertions.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Unwrap an operation while preserving its diagnostic.
template<class T, class E> auto take (std::expected<T, E> result) -> T {
	if (!result)
		throw std::runtime_error(result.error().message);
	return std::move(*result);
}

/// Unwrap a void operation while preserving its diagnostic.
template<class E> void check (std::expected<void, E> result) {
	if (!result)
		throw std::runtime_error(result.error().message);
}

/// Download contiguous typed GPU data without depending on mapped alignment.
template<class T> auto read (const fcg::Buffer &buffer, std::size_t offset, std::size_t count) -> std::vector<T>
{
	auto ticket = take(buffer.readback(offset, count * sizeof(T)));
	check(ticket.wait());
	auto mapping = take(ticket.map());
	std::vector<T> values(count);
	std::memcpy(values.data(), mapping.data().data(), count * sizeof(T));
	return values;
}

/// Download an attribute whose builder-generated source is a contiguous array.
template<A Attribute> auto read (const fcg::PrimitiveAttributes &attributes)
	-> std::vector<typename fcg::AttributeTraits<Attribute>::Value> {
	const auto source = std::get<fcg::AttributeBufferView<Attribute>>(attributes.source<Attribute>());
	return read<typename fcg::AttributeTraits<Attribute>::Value>(*source.buffer, source.offset, source.count);
}

/// Verify exact indices, node data, callback rollback, and source stability after moves.
void dataAndMoves (fcg::Device &device)
{
	auto builder = fcg::LineSetBuilder::create();
	builder.recordPolyline([] (auto &line) {
		line.addNode({0, 0, 0}, {1, 0, 0, 1}, 2.f);
		line.addNode({1, 0, 0}, {0, 1, 0, 1}, 4.f);
		line.addNode({1, 1, 0}, {0, 0, 1, 1}, 6.f);
	});
	try {
		builder.recordPolyline([] (auto &line) {
			line.addNode({99, 99, 99});
			throw std::runtime_error("draft");
		});
		require(false, "Callback did not propagate its exception");
	} catch (const std::runtime_error &error) {
		require(std::string(error.what()) == "draft", "Unexpected callback exception");
	}
	builder.addPolyline({{{2, 0, 0}, {1, 1, 0, 1}, 8.f}, {{2, 1, 0}, {1, 0, 1, 1}, 10.f}});
	const std::array<fcg::LineNode, 3> triangle{{
		{{3, 0, 0}, {0, 1, 1, 1}, 12.f},
		{{3, 1, 0}, {1, 1, 1, 1}, 14.f},
		{{4, 1, 0}, {0, 0, 0, 1}, 16.f},
	}};
	builder.addPolyline(std::span(triangle), true);
	builder.addPolyline({});
	builder.recordPolyline([] (auto&) {});
	auto original = take(builder.upload(device));
	const auto positionsView = std::get<fcg::AttributeBufferView<A::Position>>(original.attributes.source<A::Position>());
	const auto indexView = original.connectivity.source();

	// Both construction and assignment transfer source ownership without moving buffers
	fcg::LineSet moved(std::move(original));
	fcg::LineSet assigned{fcg::PrimitiveAttributes(device), fcg::PrimitiveConnectivity(device)};
	assigned = std::move(moved);
	require(positionsView.buffer == std::get<fcg::AttributeBufferView<A::Position>>(
		assigned.attributes.source<A::Position>()).buffer, "Moving changed an attribute buffer address");
	require(indexView.buffer == assigned.connectivity.source().buffer, "Moving changed connectivity storage");
	const std::vector<std::uint32_t> expectedIndices{0, 1, 1, 2, 3, 4, 5, 6, 6, 7, 7, 5};
	require(read<std::uint32_t>(*indexView.buffer, indexView.offset, indexView.count) == expectedIndices,
		"Wrong independent or closed polyline connectivity");
	const std::vector<glm::vec4> expectedPositions{{0, 0, 0, 1}, {1, 0, 0, 1}, {1, 1, 0, 1},
		{2, 0, 0, 1}, {2, 1, 0, 1}, {3, 0, 0, 1}, {3, 1, 0, 1}, {4, 1, 0, 1}};
	require(read<A::Position>(assigned.attributes) == expectedPositions, "Wrong homogeneous positions");
	require(read<A::Radius>(assigned.attributes) == std::vector<float>({1, 2, 3, 4, 5, 6, 7, 8}),
		"Widths were not converted to half-widths");
	const std::vector<glm::vec4> expectedColors{{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1},
		{1, 1, 0, 1}, {1, 0, 1, 1}, {0, 1, 1, 1}, {1, 1, 1, 1}, {0, 0, 0, 1}};
	require(read<A::Color>(assigned.attributes) == expectedColors, "Wrong node colors");

	// A second upload uses the retained CPU geometry and a separately owned GPU set
	auto repeated = take(builder.upload(device));
	require(read<A::Position>(repeated.attributes) == expectedPositions, "Upload consumed CPU geometry");
	original = std::move(repeated);
	moved = std::move(assigned);
	original = std::move(moved);
	require(original.connectivity.indexCount() == expectedIndices.size(), "Move assignment lost connectivity");
}

/// Verify source transitions, retained settings, GPU reuse, and empty-set semantics.
void replacements (fcg::Device &device)
{
	fcg::LineSetBuilder builder;
	builder.setConstantWidth(0.f);
	builder.setConstantColor({.25f, .5f, 1.f, .75f});
	builder.addPolyline({{{0, 0, 0}}, {{1, 0, 0}}, {{1, 1, 0}}});
	auto set = take(builder.upload(device));
	require(std::get<float>(set.attributes.source<A::Radius>()) == 0.f, "Zero width was lost");
	require(std::get<glm::vec4>(set.attributes.source<A::Color>()) == glm::vec4(.25f, .5f, 1.f, .75f),
		"Constant color was lost");
	const auto positionBuffer = std::get<fcg::AttributeBufferView<A::Position>>(set.attributes.source<A::Position>()).buffer;
	const auto indexBuffer = set.connectivity.source().buffer;
	builder.clear();
	builder.addPolyline({{{0, 0, 0}}, {{0, 1, 0}}});
	builder.setConstantWidth(4.f);
	check(builder.upload(set));
	require(std::get<float>(set.attributes.source<A::Radius>()) == 2.f, "Constant width did not apply retroactively");
	require(std::get<glm::vec4>(set.attributes.source<A::Color>()) == glm::vec4(.25f, .5f, 1.f, .75f),
		"Clear discarded constant color");
	require(std::get<fcg::AttributeBufferView<A::Position>>(set.attributes.source<A::Position>()).buffer == positionBuffer
		&& set.connectivity.source().buffer == indexBuffer, "Smaller replacement did not reuse buffers");

	// Absent sources replace constants and remove unrelated externally assigned attributes
	builder.clearConstantColor();
	builder.clearConstantWidth();
	check(set.attributes.setAttributes([] (auto &update) { update.template set<A::Extent>(glm::vec3(2)); }));
	check(builder.upload(set));
	require(std::holds_alternative<std::monostate>(set.attributes.source<A::Radius>())
		&& std::holds_alternative<std::monostate>(set.attributes.source<A::Color>())
		&& std::holds_alternative<std::monostate>(set.attributes.source<A::Extent>()), "Replacement retained stale sources");
	builder.clear();
	builder.recordPolyline([] (auto &line) {
		line.addNode(fcg::LineNode({0, 0, 0}, {1, 0, 0, 1}, 2.f));
		line.addNode({1, 0, 0}, {0, 1, 0, 1}, 6.f);
	});
	check(builder.upload(set));
	require(read<A::Radius>(set.attributes) == std::vector<float>({1, 3}), "Array replacement failed");
	builder.clear();
	builder.setConstantWidth(2.f);
	builder.setConstantColor({1, 1, 1, 1});
	builder.addPolyline({{{0, 0, 0}}, {{1, 0, 0}}});
	check(builder.upload(set));
	require(std::get<float>(set.attributes.source<A::Radius>()) == 1.f, "Array-to-constant transition failed");
	builder.clear();
	check(builder.upload(set));
	require(set.attributes.instanceCount() == 0 && set.connectivity.indexCount() == 0
		&& set.connectivity.usable() && set.attributes.usable<A::Position>(), "Empty replacement is unusable");
	builder.addPolyline({{{0, 0, 0}}, {{1, 0, 0}}});
	check(builder.upload(set));
	require(set.connectivity.source().buffer == indexBuffer, "Empty replacement discarded reusable storage");
	builder.clear();
	auto empty = take(builder.upload(device));
	require(empty.attributes.instanceCount() == 0 && empty.connectivity.indexCount() == 0, "Wrong new empty set");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Verify uploaded CPU geometry and ownership using a real device.
auto main () -> int
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	int result = 0;
	try {
		auto device = fcg::Device::create();
		require(device.has_value(), SDL_GetError());
		dataAndMoves(*device);
		replacements(*device);
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
