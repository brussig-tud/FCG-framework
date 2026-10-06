# The framework image library: CPU images, format-independent loading, and the SDL_image backend.
include(Dependencies.cmake)
set(FCG_SDL_IMAGE_BUILT_HERE "${FCG_SDL_IMAGE_BUILT_HERE}" PARENT_SCOPE)
add_library(Image)
add_library(${FCG_PROJECT_NAME}::Image ALIAS Image)
set_target_properties(Image PROPERTIES VERSION ${FCG_FRAMEWORK_VERSION} SOVERSION ${FCG_FRAMEWORK_SOVERSION})
target_compile_features(Image PUBLIC ${FCG_CXX_STANDARD})
target_include_directories(Image PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)
generate_export_header(Image BASE_NAME FCG_IMAGE EXPORT_FILE_NAME include/FCG/image_export.h)
target_compile_definitions(Image PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:FCG_IMAGE_STATIC_DEFINE>")
# Third-party target export follows Core's existing build-interface-only packaging contract.
target_link_libraries(Image PUBLIC "$<BUILD_INTERFACE:SDL3::SDL3>")
get_target_property(image_backend_type SDL3_image::SDL3_image TYPE)
if(BUILD_SHARED_LIBS AND image_backend_type STREQUAL "SHARED_LIBRARY")
	# CMake exports private shared target dependencies even through BUILD_INTERFACE. Link its import/linker
	# artifact here so the installed Image target does not require an unexported third-party CMake target.
	# SDL_image's only public usage requirement is SDL3 headers, already provided above.
	target_include_directories(Image PRIVATE "$<TARGET_PROPERTY:SDL3_image::SDL3_image,INTERFACE_INCLUDE_DIRECTORIES>")
	target_link_libraries(Image PRIVATE "$<BUILD_INTERFACE:$<TARGET_LINKER_FILE:SDL3_image::SDL3_image>>")
	add_dependencies(Image SDL3_image::SDL3_image)
else()
	# Keep target propagation for static backends and their transitive codec link dependencies.
	target_link_libraries(Image PRIVATE "$<BUILD_INTERFACE:SDL3_image::SDL3_image>")
endif()
add_subdirectory(src)
