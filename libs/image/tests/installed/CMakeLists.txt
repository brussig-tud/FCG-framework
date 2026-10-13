cmake_minimum_required(VERSION 3.31)
project(ImageInstalledHeaders LANGUAGES CXX)
find_package(FCG CONFIG REQUIRED PATHS "${PIXEL_INSTALL_PREFIX}/lib/cmake/FCG" NO_DEFAULT_PATH)
# The existing installation contract supplies third-party headers separately.
add_library(installed-pixel-consumer OBJECT ../pixel_examples.cpp)
target_link_libraries(installed-pixel-consumer PRIVATE FCG-Framework::Image)
include("${DEPENDENCIES}")
target_include_directories(installed-pixel-consumer PRIVATE ${PIXEL_CONSUMER_INCLUDE_DIRS})
