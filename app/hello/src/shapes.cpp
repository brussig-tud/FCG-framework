
//////
//
// Includes
//

// C++ STL
#include <cstring>
#include <vector>

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "shapes.h"



//////
//
// Helper
//

namespace {
	/// Create a GPU buffer with the given size and usage flags.
	auto createBuffer(SDL_GPUDevice *device, std::size_t size, SDL_GPUBufferUsageFlags usage) -> SDL_GPUBuffer* {
		if (size == 0) {
			return nullptr;
		}

		const SDL_GPUBufferCreateInfo createInfo = {
			.usage = usage,
			.size = static_cast<Uint32>(size)
		};
		return SDL_CreateGPUBuffer(device, &createInfo);
	}
}



//////
//
// SimpleShape
//

SimpleShape::~SimpleShape() {
	if (m_device != nullptr) {
		if (m_vertexBuffer != nullptr) {
			SDL_ReleaseGPUBuffer(m_device, m_vertexBuffer);
		}
		if (m_indexBuffer != nullptr) {
			SDL_ReleaseGPUBuffer(m_device, m_indexBuffer);
		}
	}
}

void SimpleShape::uploadGeometry(
	fcg::Device &device,
	const void *vertexData, std::size_t vertexDataSize,
	const std::uint32_t *indexData, std::size_t numIndices
) {
	SDL_GPUDevice *gpuDevice = device.handle();

	// Cache the device pointer so the destructor can release buffers later.
	m_device = gpuDevice;

	// Create new GPU buffers. We build new ones first, then release the old ones after waiting for
	// the GPU to be idle, so any in-flight draw commands still see valid memory.
	SDL_GPUBuffer *newVertexBuffer = createBuffer(
		gpuDevice, vertexDataSize,
		SDL_GPU_BUFFERUSAGE_VERTEX
	);

	const std::size_t indexDataSize = numIndices * sizeof(std::uint32_t);
	SDL_GPUBuffer *newIndexBuffer = createBuffer(
		gpuDevice, indexDataSize,
		SDL_GPU_BUFFERUSAGE_INDEX
	);

	// Upload vertex data via a transfer buffer.
	if (newVertexBuffer != nullptr && vertexDataSize > 0) {
		const SDL_GPUTransferBufferCreateInfo transferInfo = {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = static_cast<Uint32>(vertexDataSize)
		};
		SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);
		if (transferBuffer != nullptr) {
			void *dst = SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false);
			if (dst != nullptr) {
				std::memcpy(dst, vertexData, vertexDataSize);
				SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);
			}

			SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpuDevice);
			SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
			const SDL_GPUTransferBufferLocation src = { .transfer_buffer = transferBuffer, .offset = 0 };
			const SDL_GPUBufferRegion dstRegion = {
				.buffer = newVertexBuffer,
				.offset = 0,
				.size = static_cast<Uint32>(vertexDataSize)
			};
			SDL_UploadToGPUBuffer(copyPass, &src, &dstRegion, false);
			SDL_EndGPUCopyPass(copyPass);
			SDL_SubmitGPUCommandBuffer(cmd);
			SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
		}
	}

	// Upload index data via a transfer buffer.
	if (newIndexBuffer != nullptr && indexDataSize > 0) {
		const SDL_GPUTransferBufferCreateInfo transferInfo = {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = static_cast<Uint32>(indexDataSize)
		};
		SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);
		if (transferBuffer != nullptr) {
			void *dst = SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false);
			if (dst != nullptr) {
				std::memcpy(dst, indexData, indexDataSize);
				SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);
			}

			SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpuDevice);
			SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
			const SDL_GPUTransferBufferLocation src = { .transfer_buffer = transferBuffer, .offset = 0 };
			const SDL_GPUBufferRegion dstRegion = {
				.buffer = newIndexBuffer,
				.offset = 0,
				.size = static_cast<Uint32>(indexDataSize)
			};
			SDL_UploadToGPUBuffer(copyPass, &src, &dstRegion, false);
			SDL_EndGPUCopyPass(copyPass);
			SDL_SubmitGPUCommandBuffer(cmd);
			SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
		}
	}

	// Wait until the GPU is done with the old buffers before releasing them.
	device.waitIdle();

	if (m_vertexBuffer != nullptr) {
		SDL_ReleaseGPUBuffer(gpuDevice, m_vertexBuffer);
	}
	if (m_indexBuffer != nullptr) {
		SDL_ReleaseGPUBuffer(gpuDevice, m_indexBuffer);
	}

	m_vertexBuffer = newVertexBuffer;
	m_indexBuffer = newIndexBuffer;
	m_numIndices = numIndices;
}
