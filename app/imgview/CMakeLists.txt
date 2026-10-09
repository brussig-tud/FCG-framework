
# The "Image Viewer" demo app.
set(APP_NAME imgview) # variable APP_NAME required by subdirectories
add_executable(${APP_NAME})

# Depend on the FCG framework
target_link_libraries(${APP_NAME} PRIVATE Core Image Render Extras)

# Collect sources
add_subdirectory(src)

# Setup compiler
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

# Provide feedback what our app target is called
set(APP_NAME ${APP_NAME} PARENT_SCOPE)

# Resolve the same executable-relative asset path in build trees and installed applications.
add_custom_command(TARGET imgview POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:imgview>/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/assets/cgvlogo.png"
		"$<TARGET_FILE_DIR:imgview>/assets/cgvlogo.png"
)
include(GNUInstallDirs)
install(FILES "${PROJECT_SOURCE_DIR}/assets/cgvlogo.png" DESTINATION "${CMAKE_INSTALL_BINDIR}/assets" COMPONENT FCG_Runtime)
