
# Add auxiliary framework libraries
add_subdirectory(image)
set(FCG_ADDITIONAL_LIBS Image)

# Report the library inventory to the parent
set(FCG_ADDITIONAL_LIBS "${FCG_ADDITIONAL_LIBS}" PARENT_SCOPE)
