
# Compile all six public overloads against the actual Extras target, including shared-library exports.
add_library(extras-examples OBJECT file_dialog_examples.cpp)
target_link_libraries(extras-examples PRIVATE Extras)
add_executable(extras-public-smoke public.cpp $<TARGET_OBJECTS:extras-examples>)
target_link_libraries(extras-public-smoke PRIVATE Extras)
add_test(NAME extras-public-smoke COMMAND extras-public-smoke)
