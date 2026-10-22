
# Collect sources
set(SOURCES
	image.cpp
	image_color.cpp
	pixel_format.cpp
	pixel_access.cpp
	image_loader.cpp
	sdl_image.cpp
	hdr_common.cpp
	radiance_hdr.cpp
	pfm.cpp
)
if(FCG_IMAGE_OPENEXR)
	list(APPEND SOURCES openexr.cpp)
endif()
# stb's implementation macros must be confined to one translation unit.
set_source_files_properties(radiance_hdr.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

# Collect headers
set(HEADERS
	# Public interface
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/image.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/image_loader.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/sdl_image.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/radiance_hdr.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/pfm.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/openexr.h

	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/error.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/pixel_format.h
	${FCG_IMAGE_BASE_DIR}/include/FCG/Image/pixel_access.h

	# Internal
	image_internal.h
	image_upload_internal.h
	sdl_image_internal.h
	hdr_internal.h
)

# Add to our target
target_sources(Image PRIVATE ${SOURCES})
target_sources(Image PRIVATE ${HEADERS})
