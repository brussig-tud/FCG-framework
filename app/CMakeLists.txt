
# Include (optional) demo apps of the FCG exercise framework
option(FCG_BUILD_APPS "Enable building the demo apps – disabling overrides the switches for the individual apps" ON)
if (FCG_BUILD_APPS)
	# The "Hello FCG!" demo.
	option(FCG_BUILD_APP_HELLO "Build the 'Hello FCG!' demo app" ON)
	if (FCG_BUILD_APP_HELLO)
		add_subdirectory(hello) # <- also sets APP_NAME for us to use in the line below
		list(APPEND FCG_APP_TARGETS ${APP_NAME})
	endif()

	# The "Image Viewer" demo.
	option(FCG_BUILD_APP_IMGVIEW "Build the 'Image Viewer' demo app" ON)
	if (FCG_BUILD_APP_IMGVIEW)
		add_subdirectory(imgview) # <- also sets APP_NAME for us to use in the line below
		list(APPEND FCG_APP_TARGETS ${APP_NAME})
	endif()

	# Publish actually included apps
	set(FCG_APP_TARGETS ${FCG_APP_TARGETS} PARENT_SCOPE)
endif()
