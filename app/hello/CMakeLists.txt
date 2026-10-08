
# The "Hello FCG!" demo app.
set(APP_NAME hello) # variable APP_NAME required by subdirectories
add_executable(${APP_NAME})

# Depend on the FCG framework
target_link_libraries(${APP_NAME} PRIVATE Core)

# Collect sources
add_subdirectory(src)
add_subdirectory(shader)

# Setup compiler
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

# Provide feedback what our app target is called
set(APP_NAME ${APP_NAME} PARENT_SCOPE)
