# Framework-owned manifest: keep registrations outside binary build-option branches.
fcg_register_documentation(
    LIBRARY Core
    LINK_TARGET FCG-framework::Core
    COMPONENTS fcg_textures fcg_fullscreen fcg_buffers fcg_runtime fcg_applets fcg_events fcg_devices fcg_windows fcg_render_state fcg_gui fcg_resources fcg_viewing fcg_camera_focus fcg_orbit_camera fcg_camera_2d fcg_utilities
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../core/include/FCG"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../core/tests/buffers"
        "${CMAKE_CURRENT_LIST_DIR}/../core/tests/textures"
        "${CMAKE_CURRENT_LIST_DIR}/../core/tests/cameras"
    PREDEFINED FCG_FRAMEWORK_EXPORT=
)

fcg_register_documentation(
    LIBRARY Image
    LINK_TARGET FCG-framework::Image
    GUIDE fcg_image_guide
    COMPONENTS fcg_images fcg_image_loading fcg_sdl_image
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../libs/image/include/FCG/Image"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../libs/image/tests"
    PREDEFINED FCG_IMAGE_EXPORT=
)

fcg_register_documentation(
    LIBRARY Render
    LINK_TARGET FCG-Framework::Render
    GUIDE fcg_render_guide
    COMPONENTS fcg_primitives
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../libs/render/include/FCG/Render"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../libs/render/tests"
    PREDEFINED FCG_RENDER_EXPORT=
)

fcg_register_documentation(
    LIBRARY Extras
    LINK_TARGET FCG-Framework::Extras
    GUIDE fcg_extras_guide
    COMPONENTS fcg_file_dialogs
    INPUTS "${CMAKE_CURRENT_LIST_DIR}/../libs/extras/include/FCG/Extras" "${CMAKE_CURRENT_LIST_DIR}/extras.dox"
    EXAMPLE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../libs/extras/tests"
    PREDEFINED FCG_EXTRAS_EXPORT=
)
