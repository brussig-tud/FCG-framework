
# Collect sources
set(SOURCES
	file_dialog.cpp
	file_filters.cpp
)

# Collect headers
set(HEADERS
	# Public interface
	${FCG_EXTRAS_BASE_DIR}/include/FCG/Extras/file_dialog.h
	${FCG_EXTRAS_BASE_DIR}/include/FCG/Extras/file_filters.h

	# Internal
	#<none yet>
)

# Add to our target
target_sources(Extras PRIVATE ${SOURCES})
target_sources(Extras PRIVATE ${HEADERS})
