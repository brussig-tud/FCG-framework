
//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <set>
#include <span>
#include <utility>

// FCG Framework
#include <FCG/Image/image_loader.h>
#include <FCG/Extras/file_dialog.h>

// Implemented header
#include "FCG/Extras/file_filters.h"



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Build owned filters using the advertised order and a caller-selected aggregate label.
template<class Format> auto fileFilters (
	std::span<const Format> formats, bool includeAllFiles, const char *aggregateLabel
) -> std::vector<fcg::extra::FileDialogFilter>
{
	// Collect distinct extensions and format groups in their first advertised order
	std::vector<fcg::extra::FileDialogFilter> filters;
	std::vector<std::string> aggregate;
	std::set<std::string> seenExtensions;
	std::set<std::vector<std::string>> seenGroups;
	for (const auto &format : formats)
	{
		std::vector<std::string> extensions;
		for (const auto &extension : format.extensions) {
			if (std::ranges::find(extensions, extension) == extensions.end())
				extensions.push_back(extension);
			if (extension != "*" && seenExtensions.insert(extension).second)
				aggregate.push_back(extension);
		}
		auto group = extensions;
		std::ranges::sort(group);
		if (!group.empty() && seenGroups.insert(std::move(group)).second)
			filters.push_back({format.name, std::move(extensions)});
	}

	// Surround the individual formats with the aggregate and optional unrestricted filter
	if (!aggregate.empty())
		filters.insert(filters.begin(), {aggregateLabel, std::move(aggregate)});
	if (includeAllFiles)
		filters.push_back({"All files", {"*"}});
	return filters;
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library's namespace for assorted extra utilities.
namespace fcg::extra {



//////
//
// Function implementations
//

auto imageFileFilters (std::span<const ImageFileFormat> formats, bool includeAllFiles) -> std::vector<FileDialogFilter> {
	return fileFilters(formats, includeAllFiles, "All supported images");
}

auto meshFileFilters (std::span<const MeshFileFormat> formats, bool includeAllFiles) -> std::vector<FileDialogFilter> {
	return fileFilters(formats, includeAllFiles, "All supported meshes");
}



//////
//
// Module namespace close
//

// namespace fcg::extra
}
