# Framework-owned manifest: keep registrations outside binary build-option branches.
fcg_register_documentation(
    LIBRARY Core
    LINK_TARGET FCG-Framework::Core
    OVERVIEW fcg_core_overview
    MODULES fcg_textures fcg_fullscreen fcg_buffers fcg_runtime fcg_applets fcg_events fcg_devices fcg_windows fcg_render_state fcg_gui fcg_resources fcg_viewing fcg_camera_focus fcg_orbit_camera fcg_camera_2d fcg_utilities
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../core/include/FCG"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../core/tests/buffers"
        "${CMAKE_CURRENT_LIST_DIR}/../core/tests/textures"
        "${CMAKE_CURRENT_LIST_DIR}/../core/tests/cameras"
        "${CMAKE_CURRENT_LIST_DIR}/../core/tests/guides"
    PREDEFINED FCG_FRAMEWORK_EXPORT=
)

fcg_register_documentation(
    LIBRARY Image
    LINK_TARGET FCG-Framework::Image
    OVERVIEW fcg_image_overview
    MODULES fcg_images fcg_image_loading fcg_sdl_image fcg_radiance_hdr fcg_pfm fcg_openexr fcg_pixel_formats fcg_pixel_access
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../libs/image/include/FCG/Image"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../libs/image/tests"
    PREDEFINED FCG_IMAGE_EXPORT= FCG_IMAGE_HAS_OPENEXR=1
)

fcg_register_documentation(
    LIBRARY Render
    LINK_TARGET FCG-Framework::Render
    OVERVIEW fcg_render_overview
    MODULES fcg_primitives fcg_ellipsoids fcg_connected_primitives fcg_lines fcg_line_sets
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../libs/render/include/FCG/Render"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../libs/render/tests"
    PREDEFINED FCG_RENDER_EXPORT=
)

fcg_register_documentation(
    LIBRARY Extras
    LINK_TARGET FCG-Framework::Extras
    OVERVIEW fcg_extras_overview
    MODULES fcg_file_dialogs fcg_extra_gui
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../libs/extras/include/FCG/Extras"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../libs/extras/tests"
    PREDEFINED FCG_EXTRAS_EXPORT=
)
