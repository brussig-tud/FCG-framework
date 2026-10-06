cmake_minimum_required(VERSION 3.31)
project(DocumentationFixture NONE)
include("${FCG_DOCUMENTATION_CMAKE}")
option(FCG_BUILD_DOCUMENTATION "Build documentation" "${PROJECT_IS_TOP_LEVEL}")
# This independent project resolves its tooling at startup, before documentation registration.
include("${FCG_DEPENDENCIES_DIR}/Doxygen.cmake")
add_library(First INTERFACE)
set(first_components fixture_first)
set(second_components fixture_second)
if(MOVE_COMPONENT)
    list(APPEND second_components fixture_mobile)
else()
    list(APPEND first_components fixture_mobile)
endif()
fcg_register_documentation(
    LIBRARY First LINK_TARGET Fixture::First GUIDE fixture_library_guide COMPONENTS ${first_components}
    INPUTS "public headers" "public headers/first.h"
    PREDEFINED FIXTURE_EXPORT=
)
add_subdirectory(subdir)
# Second deliberately has no binary target.
fcg_register_documentation(
    LIBRARY Second LINK_TARGET Fixture::Second COMPONENTS ${second_components}
    INPUTS "public headers/nested/second.hpp"
)
if(CONFLICT)
    fcg_register_documentation(LIBRARY Second LINK_TARGET Fixture::Second COMPONENTS fixture_first)
endif()
if(CONFLICT_GUIDE)
    fcg_register_documentation(LIBRARY First LINK_TARGET Fixture::First GUIDE conflicting_guide)
endif()
if(BAD_REFERENCE)
    fcg_register_documentation(LIBRARY First LINK_TARGET Fixture::First INPUTS "bad.h")
endif()
fcg_add_documentation(TEMPLATE "${FCG_DOXYFILE_TEMPLATE}" MAIN_PAGE "${CMAKE_CURRENT_SOURCE_DIR}/main.dox")
