
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
// Module namespace open
//

// Our library namespace.
namespace fcg::extra {



//////
//
// Function implementations
//

auto imageFileFilters (std::span<const ImageFileFormat> formats) -> std::vector<FileDialogFilter>
{
	std::vector<FileDialogFilter> filters;
	std::vector<std::string> aggregate;
	std::set<std::string> seenExtensions;
	std::set<std::vector<std::string>> seenGroups;
	for (const auto &format : formats)
	{
		std::vector<std::string> extensions;
		for (const auto &extension : format.extensions)
		{
			if (std::ranges::find(extensions, extension) == extensions.end())
				extensions.push_back(extension);
			if (seenExtensions.insert(extension).second)
				aggregate.push_back(extension);
		}
		auto group = extensions;
		std::ranges::sort(group);
		if (!group.empty() && seenGroups.insert(std::move(group)).second)
			filters.push_back({format.name, std::move(extensions)});
	}
	if (!aggregate.empty())
		filters.insert(filters.begin(), {"All supported images", std::move(aggregate)});
	filters.push_back({"All files", {"*"}});
	return filters;
}



//////
//
// Module namespace close
//

// namespace fcg::extra
}
