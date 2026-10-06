# The framework core library.
add_library(Core)
add_library(${FCG_PROJECT_NAME}::Core ALIAS Core)
set_target_properties(Core PROPERTIES VERSION ${FCG_FRAMEWORK_VERSION} SOVERSION ${FCG_FRAMEWORK_SOVERSION})
target_include_directories(Core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(Core PUBLIC ${FCG_CXX_STANDARD})
generate_export_header(Core BASE_NAME ${FCG_PROJECT_NAME} EXPORT_FILE_NAME include/FCG/export.h)
target_compile_definitions(Core PUBLIC "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:FCG_FRAMEWORK_STATIC_DEFINE>")
target_include_directories(Core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

# Link against SDL3 and GLM. Public, so that clients writing applets also get the SDL GPU and GLM headers. For
# now the dependencies are limited to the build interface – properly exporting them is left for future packaging
# work.
target_link_libraries(
	Core PUBLIC "$<BUILD_INTERFACE:SDL3::SDL3>" "$<BUILD_INTERFACE:glm::glm>" "$<BUILD_INTERFACE:imgui::imgui>"
)
# Runtime SPIR-V translation for non-Vulkan backends (Metal, Direct3D 12).
target_link_libraries(Core PRIVATE "$<BUILD_INTERFACE:SDL3_shadercross::SDL3_shadercross>")
# Enable the experimental GLM GTX extensions (e.g. GLM_GTX_color_space used for color handling).
target_compile_definitions(Core PUBLIC GLM_ENABLE_EXPERIMENTAL)

# Collect sources
set(FCG_CORE_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(src)

# Embedded shaders (cmake/Shaders.cmake).
add_subdirectory(shaders)
target_link_libraries(Core PRIVATE "$<BUILD_INTERFACE:fcg-shaders>")
