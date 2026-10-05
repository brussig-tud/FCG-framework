//////
//
// Includes
//

// C++ STL
#include <algorithm>
#include <cstdlib>

// Embedded fixture resources
#include <first-resources.h>
#include <second-resources.h>



//////
//
// Entry point
//

/// Check namespace separation and removal of a registration without accessing any loose resource files.
auto main () -> int {
	const auto first = first::embedded::FS.find("logical/logical.comp.spv");
	const auto second = second::embedded::FS.find("logical/logical.comp.spv");
	if (first == first::embedded::FS.end() || second == second::embedded::FS.end())
		return EXIT_FAILURE;
	const auto firstBytes = (*first).bytes();
	const auto secondBytes = (*second).bytes();
	if (!firstBytes || !secondBytes || firstBytes->empty() || secondBytes->empty()
		|| std::ranges::equal(*firstBytes, *secondBytes))
		return EXIT_FAILURE;
	const bool extra = first::embedded::FS.find("obsolete/obsolete.comp.spv") != first::embedded::FS.end();
	if (extra != static_cast<bool>(EXPECT_EXTRA))
		return EXIT_FAILURE;
	if (first::embedded::FS.find("logical/work.comp.spv") != first::embedded::FS.end())
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
