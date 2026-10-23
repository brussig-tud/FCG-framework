# Compile the exact examples used by the public camera guides.
add_library(camera-examples OBJECT camera_examples.cpp camera_focus_examples.cpp)
target_link_libraries(camera-examples PRIVATE Core)

add_executable(camera-smoke main.cpp)
target_link_libraries(camera-smoke PRIVATE Core)
add_test(NAME camera-smoke COMMAND camera-smoke)

# Interactive GUI review after building (replace debug with the selected preset):
# cmake -E env FCG_CAMERA_PROBE_INTERACTIVE=1 build/debug/bin/camera-lifecycle
add_executable(camera-lifecycle lifecycle.cpp)
target_link_libraries(camera-lifecycle PRIVATE Core)
add_test(NAME camera-lifecycle COMMAND camera-lifecycle)
set_tests_properties(camera-lifecycle PROPERTIES
    TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "Validation (Error|Warning|Performance Warning);VUID-;without a matching push"
)
