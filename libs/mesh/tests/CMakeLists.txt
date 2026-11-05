# Compile every snippet used by the public guides.
add_library(mesh-examples OBJECT mesh_examples.cpp)
target_link_libraries(mesh-examples PRIVATE ${FCG_PROJECT_NAME}::Mesh)
add_executable(mesh-smoke main.cpp)
target_link_libraries(mesh-smoke PRIVATE ${FCG_PROJECT_NAME}::Mesh)
add_test(NAME mesh-smoke COMMAND mesh-smoke "${CMAKE_CURRENT_BINARY_DIR}/scratch")
# No concrete handler reference: validates automatic backend retention in static and shared builds.
add_executable(mesh-global-smoke global.cpp)
target_link_libraries(mesh-global-smoke PRIVATE ${FCG_PROJECT_NAME}::Mesh)
add_test(NAME mesh-global-smoke COMMAND mesh-global-smoke)
add_library(mesh-test-registration OBJECT registration.cpp)
target_link_libraries(mesh-test-registration PRIVATE ${FCG_PROJECT_NAME}::Mesh)
add_executable(mesh-registration-smoke registration_main.cpp)
target_link_libraries(mesh-registration-smoke PRIVATE ${FCG_PROJECT_NAME}::Mesh mesh-test-registration)
add_test(NAME mesh-registration-smoke COMMAND mesh-registration-smoke)

# Explicit dependencies mirror the installed framework's current third-party contract.
get_target_property(mesh_openmesh_includes OpenMeshCore INTERFACE_INCLUDE_DIRECTORIES)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dependencies-$<CONFIG>.cmake" CONTENT
"set(mesh_includes \"$<TARGET_GENEX_EVAL:${FCG_OPENMESH_TARGET},${mesh_openmesh_includes}>\")
set(mesh_definitions \"$<TARGET_PROPERTY:fcg-openmesh,INTERFACE_COMPILE_DEFINITIONS>\")
set(mesh_libraries \"$<TARGET_LINKER_FILE:${FCG_OPENMESH_TARGET}>;$<TARGET_LINKER_FILE:${FCG_tinyobjloader_TARGET}>;$<TARGET_LINKER_FILE:${FCG_tinyply_TARGET}>\")
set(mesh_tinyobj_includes \"$<TARGET_PROPERTY:${FCG_tinyobjloader_TARGET},INTERFACE_INCLUDE_DIRECTORIES>\")
set(mesh_tinyply_includes \"$<TARGET_PROPERTY:${FCG_tinyply_TARGET},INTERFACE_INCLUDE_DIRECTORIES>\")
")
if(FCG_INCLUDE_PACKAGING)
    add_test(NAME mesh-installed-consumer COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed"
        "-DBINARY=${CMAKE_CURRENT_BINARY_DIR}/installed-consumer"
        "-DCOMPILER=${CMAKE_CXX_COMPILER}"
        "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DCONFIG=$<CONFIG>"
        "-DPREFIX=${PROJECT_BINARY_DIR}/tests/install-prefix"
        "-DDEPENDENCIES=${CMAKE_CURRENT_BINARY_DIR}/dependencies-$<CONFIG>.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/installed/run.cmake")
    set_tests_properties(mesh-installed-consumer PROPERTIES DEPENDS install-smoke-verify)
endif()
add_test(NAME mesh-dependencies COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dependencies"
    "-DBINARY=${CMAKE_CURRENT_BINARY_DIR}/dependencies"
    "-DFCG_SOURCE=${PROJECT_SOURCE_DIR}"
    "-DOPENMESH_SOURCE=${OpenMesh_SOURCE_DIR}"
    "-DTINYOBJ_SOURCE=${tinyobjloader_SOURCE_DIR}"
    "-DTINYPLY_SOURCE=${tinyply_SOURCE_DIR}"
    "-DCOMPILER=${CMAKE_CXX_COMPILER}"
    "-DDEPENDENCIES=${CMAKE_CURRENT_BINARY_DIR}/dependencies-$<CONFIG>.cmake"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/run.cmake")
