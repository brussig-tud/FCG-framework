
# Include source subdirectories
include_directories(shapes)

# Collect sources
set(SOURCES
	main.cpp
	shapes.cpp
)

# Collect headers
set(HEADERS
	shapes.h
)

# Add to our target
target_sources(${APP_NAME} PRIVATE ${SOURCES})
target_sources(${APP_NAME} PRIVATE ${HEADERS})
