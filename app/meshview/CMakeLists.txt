
# The "Mesh Viewer" demo app.
set(APP_NAME meshview)
add_executable(${APP_NAME})
# File-filter declarations also include the image-loader interface.
target_link_libraries(${APP_NAME} PRIVATE Core Image Mesh Render Extras)
add_subdirectory(src)
set(APP_NAME ${APP_NAME} PARENT_SCOPE)

# Keep the startup mesh available independently of the working directory.
add_custom_command(TARGET meshview POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:meshview>/assets/mesh"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/assets/mesh/max_low.obj"
		"$<TARGET_FILE_DIR:meshview>/assets/mesh/max_low.obj"
)
include(GNUInstallDirs)
install(FILES "${PROJECT_SOURCE_DIR}/assets/mesh/max_low.obj"
	DESTINATION "${CMAKE_INSTALL_BINDIR}/assets/mesh" COMPONENT FCG_Runtime)
