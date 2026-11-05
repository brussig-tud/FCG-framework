cmake_minimum_required(VERSION 3.31)
project(MeshInstalledConsumer LANGUAGES CXX)
find_package(FCG CONFIG REQUIRED PATHS "${MESH_INSTALL_PREFIX}/lib/cmake/FCG" NO_DEFAULT_PATH)
include("${DEPENDENCIES}")
add_executable(consumer ../global.cpp ../mesh_processing_examples.cpp)
target_link_libraries(consumer PRIVATE FCG-Framework::Mesh ${mesh_libraries})
target_include_directories(consumer SYSTEM PRIVATE ${mesh_includes})
target_compile_definitions(consumer PRIVATE ${mesh_definitions})
set_target_properties(consumer PROPERTIES BUILD_RPATH "${MESH_INSTALL_PREFIX}/lib")
# Record each configuration's executable path separately for multi-configuration generators
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/consumer-$<CONFIG>.cmake"
    CONTENT "set(consumer \"$<TARGET_FILE:consumer>\")\n")
