
# Collect sources
set(SOURCES
	main.cpp
	shapes.cpp
)

# Collect headers
set(HEADERS
	shapes.h
	shapes/convex_poly.h
	shapes/platonic_solid.h
)

# Add to our target
target_sources(${APP_NAME} PRIVATE ${SOURCES})
target_sources(${APP_NAME} PRIVATE ${HEADERS})
