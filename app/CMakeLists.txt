
# All (optional) executables of the FCG exercise framework
# - the "Hello FCG" demo.
# The "Hello FCG" demo app.
option(FCG_BUILD_APP_HELLO "Build the 'Hello FCG!' demo app" ON)
if (FCG_BUILD_APP_HELLO)
	add_subdirectory(hello) # <- also sets APP_NAME for us to use in the line below
	list(APPEND FCG_APP_TARGETS ${APP_NAME})
endif()
# - the "Image Viewer" demo.
# The "Hello FCG" demo app.
option(FCG_BUILD_APP_IMGVIEW "Build the 'Image Viewer' demo app" ON)
if (FCG_BUILD_APP_IMGVIEW)
	add_subdirectory(imgview) # <- also sets APP_NAME for us to use in the line below
	list(APPEND FCG_APP_TARGETS ${APP_NAME})
endif()

# Publish actually included apps
set(FCG_APP_TARGETS ${FCG_APP_TARGETS} PARENT_SCOPE)
