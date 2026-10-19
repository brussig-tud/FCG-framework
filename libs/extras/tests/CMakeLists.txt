
# Compile the dialog snippets and Image-dependent filter example against their public targets.
add_library(extras-examples OBJECT file_dialog_examples.cpp)
target_link_libraries(extras-examples PRIVATE Extras Image)
add_executable(extras-public-smoke public.cpp $<TARGET_OBJECTS:extras-examples>)
target_link_libraries(extras-public-smoke PRIVATE Extras Image)
add_test(NAME extras-public-smoke COMMAND extras-public-smoke)

# Exercise the implementation's private backend while public.cpp checks the actual library exports.
add_executable(extras-completion-smoke completion.cpp)
target_compile_definitions(extras-completion-smoke PRIVATE FCG_EXTRAS_STATIC_DEFINE)
target_include_directories(extras-completion-smoke PRIVATE "$<TARGET_PROPERTY:Extras,INTERFACE_INCLUDE_DIRECTORIES>")
target_link_libraries(extras-completion-smoke PRIVATE Core)
add_test(NAME extras-completion-smoke COMMAND extras-completion-smoke)
set_tests_properties(extras-completion-smoke PROPERTIES TIMEOUT 15)
