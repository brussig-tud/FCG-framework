
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

# Collect interface headers
set(HEADERS
	${PROJECT_SOURCE_DIR}/include/FCG/buffer.h
	${PROJECT_SOURCE_DIR}/include/FCG/applet/orbit_camera.h
	${PROJECT_SOURCE_DIR}/include/FCG/applet.h
	${PROJECT_SOURCE_DIR}/include/FCG/event.h
	${PROJECT_SOURCE_DIR}/include/FCG/device.h
	${PROJECT_SOURCE_DIR}/include/FCG/frame.h
	${PROJECT_SOURCE_DIR}/include/FCG/gui.h
	${PROJECT_SOURCE_DIR}/include/FCG/player.h
	${PROJECT_SOURCE_DIR}/include/FCG/render_state.h
	${PROJECT_SOURCE_DIR}/include/FCG/res.h
	${PROJECT_SOURCE_DIR}/include/FCG/run.h
	${PROJECT_SOURCE_DIR}/include/FCG/util.h
	${PROJECT_SOURCE_DIR}/include/FCG/window.h
)

# Add to our target
target_sources(Core PRIVATE ${SOURCES})
target_sources(Core PRIVATE ${HEADERS})
