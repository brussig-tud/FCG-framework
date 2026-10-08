
# The framework image library: CPU images, format-independent loading, and the SDL_image backend.
add_library(Image)
add_library(${FCG_PROJECT_NAME}::Image ALIAS Image)
set_target_properties(Image PROPERTIES VERSION ${FCG_FRAMEWORK_VERSION} SOVERSION ${FCG_FRAMEWORK_SOVERSION})
target_compile_features(Image PUBLIC ${FCG_CXX_STANDARD})
target_include_directories(Image PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)
generate_export_header(Image BASE_NAME FCG_IMAGE EXPORT_FILE_NAME include/FCG/Image/export.h)
target_compile_definitions(Image PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:FCG_IMAGE_STATIC_DEFINE>")

# Third-party target export follows Core's existing build-interface-only packaging contract.
target_link_libraries(Image
	PUBLIC Core "$<BUILD_INTERFACE:SDL3::SDL3>"
	PRIVATE "$<BUILD_INTERFACE:fcg-sdl-image>"
)

# Collect sources
set(FCG_IMAGE_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(src)
