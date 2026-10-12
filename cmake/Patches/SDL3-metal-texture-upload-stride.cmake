# SDL 3.4.18's Metal transfer path derives the upload row pitch from the destination region instead of the caller's
# SDL_GPUTextureTransferInfo, so padded source rows are read as if the buffer were tightly packed. Vulkan and D3D12
# honor pixels_per_row and rows_per_layer, and so does Metal's own download path, so the upload must default each
# pitch to the region size when it is zero and otherwise use the caller's stride (upstream SDK issue 16474).
# Apply only to this pinned source fallback; installed SDL packages are untouched.
set(metal_upload_stride_source "${SDL3_SOURCE_DIR}/src/gpu/metal/SDL_gpu_metal.m")
if(EXISTS "${metal_upload_stride_source}")
    file(READ "${SDL3_SOURCE_DIR}/include/SDL3/SDL_version.h" metal_upload_stride_version)
    if(metal_upload_stride_version MATCHES "#define SDL_MINOR_VERSION[ \t]+4[\r\n]" AND metal_upload_stride_version MATCHES "#define SDL_MICRO_VERSION[ \t]+18[\r\n]")
        file(READ "${metal_upload_stride_source}" metal_upload_stride_contents)
        set(metal_upload_stride_before "              sourceBytesPerRow:BytesPerRow(destination->w, textureContainer->header.info.format)\n            // sourceBytesPerImage expects the stride between 2D images (slices) of a 3D texture, not the size of the entire region\n            sourceBytesPerImage:SDL_CalculateGPUTextureFormatSize(textureContainer->header.info.format, destination->w, destination->h, 1)")
        set(metal_upload_stride_after "              sourceBytesPerRow:BytesPerRow(source->pixels_per_row ? source->pixels_per_row : destination->w, textureContainer->header.info.format)\n            // sourceBytesPerImage expects the stride between 2D images (slices) of a 3D texture, not the size of the entire region\n            sourceBytesPerImage:BytesPerRow(source->pixels_per_row ? source->pixels_per_row : destination->w, textureContainer->header.info.format) * (source->rows_per_layer ? source->rows_per_layer : destination->h)")
        string(FIND "${metal_upload_stride_contents}" "${metal_upload_stride_before}" metal_upload_stride_position)
        if(NOT metal_upload_stride_position EQUAL -1)
            string(REPLACE "${metal_upload_stride_before}" "${metal_upload_stride_after}" metal_upload_stride_contents "${metal_upload_stride_contents}")
            file(WRITE "${metal_upload_stride_source}" "${metal_upload_stride_contents}")
            message(STATUS "Applied SDL 3.4.18 Metal texture upload stride correction")
        else()
            string(FIND "${metal_upload_stride_contents}" "${metal_upload_stride_after}" metal_upload_stride_position)
            if(metal_upload_stride_position EQUAL -1)
                message(FATAL_ERROR "Pinned SDL 3.4.18 Metal texture upload no longer matches its correction")
            endif()
        endif()
    endif()
endif()
