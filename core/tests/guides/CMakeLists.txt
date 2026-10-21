# Compile the exact examples used by the Core module guides.
add_library(guide-examples OBJECT
	applet_examples.cpp
	runtime_examples.cpp
	presentation_examples.cpp
	event_examples.cpp
	device_examples.cpp
	window_examples.cpp
	render_state_examples.cpp
	gui_examples.cpp
	resource_examples.cpp
	utility_examples.cpp
)
target_link_libraries(guide-examples PRIVATE Core)
