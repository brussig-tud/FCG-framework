
cmake_minimum_required(VERSION 3.31)
project(tiff VERSION 4.7.2 LANGUAGES C)

# Configure-only stand-in for libtiff's targets and independently controlled linkage.
foreach(feature tools tests contrib docs cxx opengl install)
    option(tiff-${feature} "Fixture optional feature" ON)
    if(tiff-${feature} AND NOT MODE STREQUAL "tiff-overrides")
        message(FATAL_ERROR "Unexpected libtiff feature: tiff-${feature}")
    endif()
endforeach()
if(tiff-static)
    set(tiff_type STATIC)
else()
    set(tiff_type SHARED)
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stub.c" "int fixture_tiff (void) { return 0; }\n")
add_library(tiff ${tiff_type} "${CMAKE_CURRENT_BINARY_DIR}/stub.c")
add_library(TIFF::tiff ALIAS tiff)
target_include_directories(tiff PUBLIC "${IMAGE_DEPENDENCY_SOURCE}/VisualC/external/include")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/selection.cmake"
    "set(selected_shared \"${BUILD_SHARED_LIBS}\")\n"
    "set(selected_static \"${tiff-static}\")\n"
    "set(selected_tools \"${tiff-tools}\")\n"
    "set(selected_jpeg \"${jpeg}\")\n"
    "set(selected_pic \"${CMAKE_POSITION_INDEPENDENT_CODE}\")\n")
