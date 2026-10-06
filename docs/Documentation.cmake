# Framework-owned manifest: keep registrations outside binary build-option branches.
fcg_register_documentation(
    LIBRARY Core
    LINK_TARGET FCG-framework::Core
    COMPONENTS fcg_buffers fcg_runtime fcg_applets fcg_events fcg_devices fcg_windows fcg_render_state fcg_gui fcg_resources fcg_orbit_camera fcg_utilities
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../include/FCG"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../tests/buffers"
    PREDEFINED FCG_FRAMEWORK_EXPORT=
)
