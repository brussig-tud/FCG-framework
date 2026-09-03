
# Collect headers
set(HEADERS
	convex_poly.h
)

# Add to our target
target_sources(${APP_NAME} PRIVATE ${HEADERS})
