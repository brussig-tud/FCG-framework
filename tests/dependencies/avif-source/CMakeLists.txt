cmake_minimum_required(VERSION 3.31)
project(libavif VERSION 1.4.2 LANGUAGES C)

# Configure-only stand-in: expose the same target/options that SDL_image and the resolver inspect.
if(BUILD_SHARED_LIBS)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stub.c" "int fixture_avif (void) { return 0; }\n")
    add_library(avif SHARED "${CMAKE_CURRENT_BINARY_DIR}/stub.c")
    target_include_directories(avif PUBLIC "${IMAGE_DEPENDENCY_SOURCE}/VisualC/external/include")
else()
    add_library(avif INTERFACE)
    target_include_directories(avif INTERFACE "${IMAGE_DEPENDENCY_SOURCE}/VisualC/external/include")
endif()
set(AVIF_CODEC_AOM_DECODE ON)
foreach(codec AOM DAV1D LIBGAV1 AVM)
    if(AVIF_CODEC_${codec} AND NOT AVIF_CODEC_${codec} STREQUAL "OFF")
        set(AVIF_CODEC_${codec}_ENABLED ON)
    endif()
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/selection.cmake"
    "set(selected_aom \"${AVIF_CODEC_AOM}\")\n"
    "set(selected_dav1d \"${AVIF_CODEC_DAV1D}\")\n"
    "set(selected_gav1 \"${AVIF_CODEC_LIBGAV1}\")\n"
    "set(selected_encode \"${AVIF_CODEC_AOM_ENCODE}\")\n"
    "set(selected_shared \"${BUILD_SHARED_LIBS}\")\n"
    "set(selected_cpu \"${AOM_TARGET_CPU}\")\n")
foreach(feature BUILD_APPS BUILD_TESTS BUILD_EXAMPLES BUILD_MAN_PAGES LIBYUV)
    if(AVIF_${feature})
        message(FATAL_ERROR "Unexpected fixture dependency feature: AVIF_${feature}")
    endif()
endforeach()
