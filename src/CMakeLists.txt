
# Collect sources
set(SOURCES
	applet/orbit_camera.cpp
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
	${CMAKE_SOURCE_DIR}/include/FCG/applet/orbit_camera.h
	${CMAKE_SOURCE_DIR}/include/FCG/applet.h
	${CMAKE_SOURCE_DIR}/include/FCG/device.h
	${CMAKE_SOURCE_DIR}/include/FCG/frame.h
	${CMAKE_SOURCE_DIR}/include/FCG/gui.h
	${CMAKE_SOURCE_DIR}/include/FCG/player.h
	${CMAKE_SOURCE_DIR}/include/FCG/render_state.h
	${CMAKE_SOURCE_DIR}/include/FCG/res.h
	${CMAKE_SOURCE_DIR}/include/FCG/run.h
	${CMAKE_SOURCE_DIR}/include/FCG/util.h
	${CMAKE_SOURCE_DIR}/include/FCG/window.h
)

# Add to our target
target_sources(Core PRIVATE ${SOURCES})
target_sources(Core PRIVATE ${HEADERS})
