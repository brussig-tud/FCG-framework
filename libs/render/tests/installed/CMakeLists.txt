cmake_minimum_required(VERSION 3.31)
project(RenderInstalledHeaders LANGUAGES CXX)
find_package(FCG CONFIG REQUIRED PATHS "${RENDER_INSTALL_PREFIX}/lib/cmake/FCG" NO_DEFAULT_PATH)
# The installation contract supplies third-party headers separately.
add_library(installed-line-set-consumer OBJECT ../line_set_examples.cpp)
target_link_libraries(installed-line-set-consumer PRIVATE FCG-Framework::Render)
include("${DEPENDENCIES}")
target_include_directories(installed-line-set-consumer PRIVATE ${RENDER_CONSUMER_INCLUDE_DIRS})
