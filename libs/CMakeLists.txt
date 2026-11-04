
# Add auxiliary framework libraries
add_subdirectory(image)
add_subdirectory(mesh)
add_subdirectory(render)
add_subdirectory(extras)
set(FCG_ADDITIONAL_LIBS Image Mesh Render Extras)

# Report the library inventory to the parent
set(FCG_ADDITIONAL_LIBS "${FCG_ADDITIONAL_LIBS}" PARENT_SCOPE)
