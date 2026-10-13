
# Collect sources
set(SOURCES
	image.cpp
	pixel_format.cpp
	pixel_access.cpp
	image_loader.cpp
	sdl_image.cpp
)

# Collect headers
set(HEADERS
	# Public interface
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/image.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/image_loader.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/sdl_image.h

	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/error.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/pixel_format.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/pixel_access.h

	# Internal
	image_internal.h
)

# Add to our target
target_sources(Image PRIVATE ${SOURCES})
target_sources(Image PRIVATE ${HEADERS})
