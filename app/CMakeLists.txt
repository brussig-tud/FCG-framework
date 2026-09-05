
# All (optional) executables of the FCG exercise framework
# - the "Hello FCG" demo.
# The "Hello FCG" demo app.
option(FCG_BUILD_APP_HELLO "Build the 'Hello FCG' demo app" ON)
if (FCG_BUILD_APP_HELLO)
	add_subdirectory(hello)
	list(APPEND FCG_APP_TARGETS ${APP_NAME})
endif()

# Publish actually included apps
set(FCG_APP_TARGETS ${FCG_APP_TARGETS} PARENT_SCOPE)
