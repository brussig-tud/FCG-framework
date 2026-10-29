
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cstdint>
#include <string>

// SDL3 library
#include <SDL3/SDL.h>

// Implemented header
#include <FCG/Render/line_set_builder.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Remove every source while retaining the collections' reusable allocations.
void clearSet (fcg::LineSet &set)
{
	(void)set.attributes.setAttributes([] (fcg::PrimitiveAttributes::Update &update) {
		update.clear<fcg::Attribute::Position>();
		update.clear<fcg::Attribute::Normal>();
		update.clear<fcg::Attribute::Radius>();
		update.clear<fcg::Attribute::Extent>();
		update.clear<fcg::Attribute::Orientation>();
		update.clear<fcg::Attribute::Color>();
	});
	set.connectivity.clear();
}

/// Capture an SDL diagnostic before upload cleanup changes it.
auto uploadFailure (const char *operation) -> std::unexpected<fcg::RenderError> {
	return std::unexpected(fcg::RenderError{fcg::RenderErrorCode::SDLFailure,
		std::string(operation) + ": " + SDL_GetError()});
}

// Anonymous namespace end
}



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
// LineSetBuilder::Polyline

void LineSetBuilder::Polyline::addNode (LineNode node) {
	nodes.push_back(std::move(node));
}

void LineSetBuilder::Polyline::addNode (glm::vec3 position, std::optional<float> width) {
	nodes.emplace_back(position, width);
}

void LineSetBuilder::Polyline::addNode (glm::vec3 position, glm::vec4 color, std::optional<float> width) {
	nodes.emplace_back(position, color, width);
}


////
// LineSetBuilder

auto LineSetBuilder::create () -> LineSetBuilder {
	return {};
}

void LineSetBuilder::setConstantWidth (float width) {
	constantWidth = width;
}

void LineSetBuilder::clearConstantWidth () {
	constantWidth.reset();
}

void LineSetBuilder::setConstantColor (glm::vec4 color) {
	constantColor = color;
}

void LineSetBuilder::clearConstantColor () {
	constantColor.reset();
}

void LineSetBuilder::addPolyline (std::span<const LineNode> nodes, bool closed) {
	append(std::vector<LineNode>(nodes.begin(), nodes.end()), closed);
}

void LineSetBuilder::addPolyline (std::initializer_list<LineNode> nodes, bool closed) {
	addPolyline(std::span(nodes.begin(), nodes.size()), closed);
}

void LineSetBuilder::append (std::vector<LineNode> draft, bool closed)
{
	// Prepare path capacity before changing geometry; node copies cannot throw
	if (draft.empty())
		return;
	if (paths.size() == paths.capacity())
		paths.reserve(std::max(paths.size() + 1, paths.capacity() * 2));
	const auto first = nodes.size();
	nodes.insert(nodes.end(), draft.begin(), draft.end());
	paths.push_back({first, draft.size(), closed});
}

void LineSetBuilder::clear () {
	nodes.clear();
	paths.clear();
}

auto LineSetBuilder::upload (Device &device) const -> std::expected<LineSet, RenderError> {
	LineSet result{PrimitiveAttributes(device), PrimitiveConnectivity(device)};
	if (auto uploaded = upload(result); !uploaded)
		return std::unexpected(std::move(uploaded.error()));
	return result;
}

auto LineSetBuilder::upload (LineSet &destination) const -> std::expected<void, RenderError>
{
	// Pack every CPU array before touching the destination
	std::vector<glm::vec4> positions;
	std::vector<glm::vec4> colors;
	std::vector<float> radii;
	std::vector<std::uint32_t> indices;
	const bool nodeColors = !constantColor && !nodes.empty() && nodes.front().color.has_value();
	const bool nodeWidths = !constantWidth && !nodes.empty() && nodes.front().width.has_value();
	positions.reserve(nodes.size());
	if (nodeColors)
		colors.reserve(nodes.size());
	if (nodeWidths)
		radii.reserve(nodes.size());
	std::size_t indexCount = 0;
	for (const auto &path : paths)
		indexCount += 2 * (path.count - 1 + path.closed);
	indices.reserve(indexCount);
	for (const auto &node : nodes)
	{
		positions.emplace_back(node.position, 1.f);
		if (nodeColors)
			colors.push_back(*node.color);
		if (nodeWidths)
			radii.push_back(*node.width * .5f);
	}
	for (const auto &path : paths)
	{
		for (std::size_t i = 1; i < path.count; ++i) {
			indices.push_back((std::uint32_t)(path.first + i - 1));
			indices.push_back((std::uint32_t)(path.first + i));
		}
		if (path.closed) {
			indices.push_back((std::uint32_t)(path.first + path.count - 1));
			indices.push_back((std::uint32_t)path.first);
		}
	}

	// Clear empty replacements without acquiring GPU commands
	if (positions.empty()) {
		clearSet(destination);
		return {};
	}

	// Own cleanup through all operational failures and exceptions after packing
	struct UploadScope
	{
		/// The destination to empty unless the entire upload succeeds.
		LineSet &destination;

		/// Acquired command buffer, cleared before submission consumes it.
		SDL_GPUCommandBuffer *command = nullptr;

		/// Active copy pass, cleared when it is ended.
		SDL_GPUCopyPass *pass = nullptr;

		/// Whether all replacement data has been successfully submitted.
		bool submitted = false;

		/// End recording, cancel unsubmitted work, and empty a failed destination.
		~UploadScope()
		{
			if (pass)
				SDL_EndGPUCopyPass(pass);
			if (command)
				SDL_CancelGPUCommandBuffer(command);
			if (!submitted)
				clearSet(destination);
		}
	} scope{destination};
	scope.command = SDL_AcquireGPUCommandBuffer(destination.attributes.device().handle());
	if (!scope.command)
		return uploadFailure("Acquiring line set upload");
	scope.pass = SDL_BeginGPUCopyPass(scope.command);
	if (!scope.pass)
		return uploadFailure("Beginning line set upload");

	// Replace attributes and connectivity in the same submission, reusing collection storage
	auto attributes = destination.attributes.setAttributes(scope.pass, [&] (PrimitiveAttributes::Update &update) {
		update.clear<Attribute::Normal>();
		update.clear<Attribute::Extent>();
		update.clear<Attribute::Orientation>();
		update.set<Attribute::Position>(std::span(positions));
		if (constantColor)
			update.set<Attribute::Color>(*constantColor);
		else if (nodeColors)
			update.set<Attribute::Color>(std::span(colors));
		else
			update.clear<Attribute::Color>();
		if (constantWidth)
			update.set<Attribute::Radius>(*constantWidth * .5f);
		else if (nodeWidths)
			update.set<Attribute::Radius>(std::span(radii));
		else
			update.clear<Attribute::Radius>();
	});
	if (!attributes)
		return attributes;
	if (auto connectivity = destination.connectivity.setIndices(scope.pass, indices); !connectivity)
		return connectivity;

	// Submission consumes the command buffer even when it reports failure
	SDL_EndGPUCopyPass(std::exchange(scope.pass, nullptr));
	if (!SDL_SubmitGPUCommandBuffer(std::exchange(scope.command, nullptr)))
		return uploadFailure("Submitting line set upload");
	scope.submitted = true;
	return {};
}



//////
//
// Module namespace close
//

} // namespace fcg
