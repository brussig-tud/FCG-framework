//////
//
// Includes
//

// C++ STL
#include <cstring>
#include <iostream>
#include <stdexcept>

// Sample shapes
#include "shapes/convex_poly.h"
#include "shapes/platonic_solid.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Expose only the controls needed to test replacement, failure, and empty geometry on the real sample base class.
class PolygonProbe : public ConvexPolygon
{
public:
	using SimpleShape::markDirty;
	using SimpleShape::uploadGeometry;
};

/// Preserve release-build assertions and report wrapper failures clearly.
void require (bool condition, const char *message) {
	if (!condition)
		throw std::runtime_error(message);
}

/// Read the actual sample buffer and compare it to the generated CPU data.
void verify (const fcg::Buffer &buffer, std::span<const std::byte> expected) {
	auto ticket = buffer.readback();
	if (!ticket)
		throw std::runtime_error(ticket.error().message);
	if (auto result = ticket->wait(); !result)
		throw std::runtime_error(result.error().message);
	auto mapping = ticket->map();
	if (!mapping)
		throw std::runtime_error(mapping.error().message);
	require(mapping->data().size() == expected.size(), "Sample geometry size differs");
	require(std::memcmp(mapping->data().data(), expected.data(), expected.size()) == 0, "Sample geometry contents differ");
}

// Anonymous namespace end
}



//////
//
// Functions
//

/// Regenerate real sample geometry, exercise transactional failure, and release in-flight replacements.
auto main () -> int {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << SDL_GetError() << '\n';
		return 1;
	}
	int result = 0;
	try {
		auto device = fcg::Device::create();
		auto other = fcg::Device::create();
		require(device.has_value() && other.has_value(), "Cannot create sample test devices");
		PolygonProbe polygon;
		require(!polygon.vertexBuffer() && !polygon.indexBuffer() && !polygon.numIndices(),
			"New sample already has GPU geometry");
		PlatonicSolid solid;
		for (unsigned i = 0; i < 4; ++i) {
			polygon.markDirty();
			polygon.update(*device);
			require(!polygon.dirty() && polygon.vertexBuffer() && polygon.indexBuffer() && polygon.numIndices(),
				"Sample upload did not commit complete geometry");
			verify(*polygon.vertexBuffer(), std::as_bytes(polygon.vertices()));
			verify(*polygon.indexBuffer(), std::as_bytes(polygon.indices()));
		}
		solid.update(*device);
		require(solid.vertexBuffer() && solid.indexBuffer(), "Solid upload did not commit geometry");
		verify(*solid.vertexBuffer(), std::as_bytes(solid.vertices()));
		verify(*solid.indexBuffer(), std::as_bytes(solid.indices()));

		const auto *vertex = polygon.vertexBuffer()->handle();
		const auto *index = polygon.indexBuffer()->handle();
		const auto count = polygon.numIndices();
		polygon.markDirty();
		polygon.update(*other); // Deliberate device mismatch is a recoverable replacement failure.
		require(polygon.dirty() && polygon.vertexBuffer() && polygon.indexBuffer()
			&& polygon.vertexBuffer()->handle() == vertex
			&& polygon.indexBuffer()->handle() == index && polygon.numIndices() == count,
			"Failed replacement changed existing geometry");
		verify(*polygon.vertexBuffer(), std::as_bytes(polygon.vertices()));
		require(polygon.uploadGeometry(*device, {}, {}), "Empty geometry replacement failed");
		require(!polygon.vertexBuffer() && !polygon.indexBuffer() && !polygon.numIndices(),
			"Empty sample geometry retained allocations");
		polygon.update(*device);
		require(!polygon.dirty() && polygon.numIndices() > 0, "Sample did not recover after failed/empty uploads");
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	SDL_Quit();
	return result;
}
