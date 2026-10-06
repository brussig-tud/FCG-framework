
# Collect sources
set(SOURCES
	buffer.cpp
	applet/orbit_camera.cpp
	event.cpp
	device.cpp
	frame.cpp
	gui.cpp
	player.cpp
	render_state.cpp
	res.cpp
	run.cpp
	window.cpp
)

# Collect headers
set(HEADERS
	# Public interface
	${FCG_CORE_BASE_DIR}/include/FCG/buffer.h
	${FCG_CORE_BASE_DIR}/include/FCG/applet/orbit_camera.h
	${FCG_CORE_BASE_DIR}/include/FCG/applet.h
	${FCG_CORE_BASE_DIR}/include/FCG/event.h
	${FCG_CORE_BASE_DIR}/include/FCG/device.h
	${FCG_CORE_BASE_DIR}/include/FCG/frame.h
	${FCG_CORE_BASE_DIR}/include/FCG/gui.h
	${FCG_CORE_BASE_DIR}/include/FCG/player.h
	${FCG_CORE_BASE_DIR}/include/FCG/render_state.h
	${FCG_CORE_BASE_DIR}/include/FCG/res.h
	${FCG_CORE_BASE_DIR}/include/FCG/run.h
	${FCG_CORE_BASE_DIR}/include/FCG/util.h
	${FCG_CORE_BASE_DIR}/include/FCG/window.h

	# Internal
	#<nothing here yet>
)

# Add to our target
target_sources(Core PRIVATE ${SOURCES})
target_sources(Core PRIVATE ${HEADERS})
