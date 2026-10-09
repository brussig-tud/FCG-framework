
# Various extra utilities, like native desktop file dialogs.
add_library(Extras)
add_library(${FCG_PROJECT_NAME}::Extras ALIAS Extras)
set_target_properties(Extras PROPERTIES VERSION ${FCG_FRAMEWORK_VERSION} SOVERSION ${FCG_FRAMEWORK_SOVERSION})
target_compile_features(Extras PUBLIC ${FCG_CXX_STANDARD})
target_include_directories(Extras PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)
generate_export_header(Extras BASE_NAME FCG_EXTRAS EXPORT_FILE_NAME include/FCG/Extras/export.h)
target_compile_definitions(Extras PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:FCG_EXTRAS_STATIC_DEFINE>")

# Link framework dependencies
target_link_libraries(Extras PUBLIC Core)
target_link_libraries(Extras PRIVATE Image) # <- clients who need our Image-related features will link to it anyway

# Collect sources
set(FCG_EXTRAS_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(src)
