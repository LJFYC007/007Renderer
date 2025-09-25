#pragma once
#include <nvrhi/nvrhi.h>

#include "Core/Pointer.h"

class Device;

namespace ResourceIO
{
/*
    Upload data from CPU memory to GPU buffer
    \param device The graphics device handle
    \param buffer Target GPU buffer to upload data to
    \param pData Pointer to source data in CPU memory
    \param sizeBytes Size of data to upload in bytes
    \return True if upload succeeds, false otherwise
*/
bool uploadBuffer(ref<Device> device, nvrhi::BufferHandle buffer, const void* pData, size_t sizeBytes);

/*
    Upload texture data from CPU memory to GPU texture
    \param device The graphics device handle
    \param texture Target GPU texture to upload data to
    \param pData Pointer to source texture data in CPU memory
    \param sizeBytes Total size of texture data in bytes
    \param srcRowPitchBytes Bytes per row in source data (0 = tightly packed)
    \return True if upload succeeds, false otherwise
*/
bool uploadTexture(ref<Device> device, nvrhi::TextureHandle texture, const void* pData, size_t sizeBytes, size_t srcRowPitchBytes = 0);

/*
    Read back data from GPU buffer to CPU memory
    \param device The graphics device handle
    \param buffer Source GPU buffer to read data from
    \param pData Pointer to destination buffer in CPU memory
    \param sizeBytes Size of data to read in bytes
    \param debugName Optional debug name for profiling/debugging
    \return True if readback succeeds, false otherwise
*/
bool readbackBuffer(ref<Device> device, nvrhi::BufferHandle buffer, void* pData, size_t sizeBytes, const char* debugName = "ReadbackBuffer");

/*
    Read back texture data from GPU texture to CPU memory
    \param device The graphics device handle
    \param texture Source GPU texture to read data from
    \param pData Pointer to destination buffer in CPU memory
    \param sizeBytes Total size of texture data in bytes
    \param dstRowPitchBytes Bytes per row in destination data (0 = tightly packed)
    \return True if readback succeeds, false otherwise
*/
bool readbackTexture(ref<Device> device, nvrhi::TextureHandle texture, void* pData, size_t sizeBytes, size_t dstRowPitchBytes = 0);
} // namespace ResourceIO

class ReadbackHeap
{
public:
    ReadbackHeap(ref<Device> pDevice) : mpDevice(pDevice) {};
    ~ReadbackHeap();

    nvrhi::BufferHandle allocateBuffer(size_t size);

    void* mMappedBuffer = nullptr;

private:
    ref<Device> mpDevice;
    nvrhi::BufferHandle mpBuffer;
    size_t mBufferSize = 1;
};

// Global readback heap instance
extern ref<ReadbackHeap> gReadbackHeap;
