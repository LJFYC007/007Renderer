#include "Core/Device.h"
#include "Utils/Logger.h"
#include "Utils/ResourceIO.h"

namespace
{
uint32_t getChannelCount(nvrhi::Format format)
{
    switch (format)
    {
    case nvrhi::Format::R32_FLOAT:
    case nvrhi::Format::R16_FLOAT:
        return 1;
    case nvrhi::Format::RG32_FLOAT:
    case nvrhi::Format::RG16_FLOAT:
        return 2;
    case nvrhi::Format::RGB32_FLOAT:
        return 3;
    case nvrhi::Format::RGBA32_FLOAT:
    case nvrhi::Format::RGBA16_FLOAT:
        return 4;
    default:
        return 0;
    }
}

size_t computeRowSize(const nvrhi::TextureDesc& desc, uint32_t channelCount)
{
    return static_cast<size_t>(desc.width) * channelCount * sizeof(float);
}
} // namespace

namespace ResourceIO
{

bool uploadBuffer(ref<Device> device, nvrhi::BufferHandle buffer, const void* pData, size_t sizeBytes)
{
    if (!device || !buffer || !pData || sizeBytes == 0)
        return false;

    auto commandList = device->getCommandList();
    auto nvrhiDevice = device->getDevice();
    commandList->open();
    commandList->writeBuffer(buffer, pData, sizeBytes);
    commandList->close();
    nvrhiDevice->executeCommandList(commandList);
    return true;
}

bool uploadTexture(ref<Device> device, nvrhi::TextureHandle texture, const void* pData, size_t sizeBytes, size_t srcRowPitchBytes)
{
    if (!device || !texture || !pData || sizeBytes == 0)
        return false;

    auto nvrhiDevice = device->getDevice();
    auto commandList = device->getCommandList();

    const auto& desc = texture->getDesc();
    uint32_t channelCount = getChannelCount(desc.format);
    if (channelCount == 0)
    {
        LOG_ERROR("uploadTexture unsupported format: {}", static_cast<int>(desc.format));
        return false;
    }

    const size_t rowSizeBytes = computeRowSize(desc, channelCount);
    const size_t requiredSize = rowSizeBytes * desc.height;
    if (sizeBytes < requiredSize)
    {
        LOG_ERROR("uploadTexture insufficient data: required {} bytes, got {} bytes", requiredSize, sizeBytes);
        return false;
    }

    nvrhi::StagingTextureHandle stagingTexture = nvrhiDevice->createStagingTexture(desc, nvrhi::CpuAccessMode::Write);
    if (!stagingTexture)
    {
        LOG_ERROR("Failed to create staging texture for upload");
        return false;
    }

    nvrhi::TextureSlice slice;
    size_t mappedRowPitch = 0;
    void* mappedData = nvrhiDevice->mapStagingTexture(stagingTexture, slice, nvrhi::CpuAccessMode::Write, &mappedRowPitch);
    if (!mappedData)
    {
        LOG_ERROR("Failed to map staging texture for upload");
        return false;
    }

    const size_t effectiveSrcRowPitch = srcRowPitchBytes != 0 ? srcRowPitchBytes : rowSizeBytes;
    const uint8_t* src = static_cast<const uint8_t*>(pData);
    for (uint32_t row = 0; row < desc.height; ++row)
    {
        const auto* srcRow = src + row * effectiveSrcRowPitch;
        auto* dstRow = static_cast<uint8_t*>(mappedData) + row * mappedRowPitch;
        std::memcpy(dstRow, srcRow, rowSizeBytes);
    }

    nvrhiDevice->unmapStagingTexture(stagingTexture);

    commandList->open();
    commandList->copyTexture(texture, slice, stagingTexture, slice);
    commandList->close();

    uint64_t fenceValue = nvrhiDevice->executeCommandList(commandList);
    nvrhiDevice->queueWaitForCommandList(nvrhi::CommandQueue::Graphics, nvrhi::CommandQueue::Graphics, fenceValue);
    return true;
}

bool readbackBuffer(ref<Device> device, nvrhi::BufferHandle buffer, void* pData, size_t sizeBytes, const char* debugName)
{
    if (!device || !buffer || !pData || sizeBytes == 0)
        return false;

    if (!gReadbackHeap)
    {
        LOG_ERROR("Readback heap is not initialized. ");
        return false;
    }

    auto nvrhiDevice = device->getDevice();
    auto commandList = device->getCommandList();
    auto stagingBuffer = gReadbackHeap->allocateBuffer(sizeBytes);

    commandList->open();
    commandList->copyBuffer(stagingBuffer, 0, buffer, 0, sizeBytes);
    commandList->close();

    uint64_t fenceValue = nvrhiDevice->executeCommandList(commandList);
    nvrhiDevice->queueWaitForCommandList(nvrhi::CommandQueue::Graphics, nvrhi::CommandQueue::Graphics, fenceValue);
    std::memcpy(pData, gReadbackHeap->mMappedBuffer, sizeBytes);
    return true;
}

bool readbackTexture(ref<Device> device, nvrhi::TextureHandle texture, void* pData, size_t sizeBytes, size_t dstRowPitchBytes)
{
    if (!device || !texture || !pData || sizeBytes == 0)
        return false;

    auto nvrhiDevice = device->getDevice();
    auto commandList = device->getCommandList();

    const auto& desc = texture->getDesc();
    uint32_t channelCount = getChannelCount(desc.format);
    if (channelCount == 0)
    {
        LOG_ERROR("readbackTexture unsupported format: {}", static_cast<int>(desc.format));
        return false;
    }

    const size_t rowSizeBytes = computeRowSize(desc, channelCount);
    const size_t requiredSize = rowSizeBytes * desc.height;
    if (sizeBytes < requiredSize)
    {
        LOG_ERROR("readbackTexture insufficient destination size: required {} bytes, got {} bytes", requiredSize, sizeBytes);
        return false;
    }

    nvrhi::StagingTextureHandle stagingTexture = nvrhiDevice->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
    if (!stagingTexture)
    {
        LOG_ERROR("Failed to create staging texture for readback");
        return false;
    }

    nvrhi::TextureSlice slice;
    commandList->open();
    commandList->copyTexture(stagingTexture, slice, texture, slice);
    commandList->close();

    uint64_t fenceValue = nvrhiDevice->executeCommandList(commandList);
    nvrhiDevice->queueWaitForCommandList(nvrhi::CommandQueue::Graphics, nvrhi::CommandQueue::Graphics, fenceValue);

    size_t mappedRowPitch = 0;
    void* mappedData = nvrhiDevice->mapStagingTexture(stagingTexture, slice, nvrhi::CpuAccessMode::Read, &mappedRowPitch);
    if (!mappedData)
    {
        LOG_ERROR("Failed to map staging texture for readback");
        return false;
    }

    const size_t effectiveDstRowPitch = dstRowPitchBytes != 0 ? dstRowPitchBytes : rowSizeBytes;
    auto* dst = static_cast<uint8_t*>(pData);
    for (uint32_t row = 0; row < desc.height; ++row)
    {
        const auto* srcRow = static_cast<const uint8_t*>(mappedData) + row * mappedRowPitch;
        auto* dstRow = dst + row * effectiveDstRowPitch;
        std::memcpy(dstRow, srcRow, rowSizeBytes);
    }

    nvrhiDevice->unmapStagingTexture(stagingTexture);
    return true;
}

} // namespace ResourceIO

ReadbackHeap::~ReadbackHeap()
{
    if (mpBuffer)
        mpDevice->getDevice()->unmapBuffer(mpBuffer);
}

nvrhi::BufferHandle ReadbackHeap::allocateBuffer(size_t size)
{
    if (size < mBufferSize && mpBuffer)
        return mpBuffer;

    // Grow the buffer size exponentially
    while (mBufferSize < size)
        mBufferSize = mBufferSize * 2;

    auto nvrhiDevice = mpDevice->getDevice();
    if (mpBuffer)
        nvrhiDevice->unmapBuffer(mpBuffer);

    // Allocate a readback buffer
    nvrhi::BufferDesc desc;
    desc.byteSize = mBufferSize;
    desc.initialState = nvrhi::ResourceStates::CopyDest;
    desc.cpuAccess = nvrhi::CpuAccessMode::Read;
    desc.keepInitialState = true;
    desc.debugName = "ReadBackHeapBuffer";

    mpBuffer = nvrhiDevice->createBuffer(desc);
    mMappedBuffer = nvrhiDevice->mapBuffer(mpBuffer, nvrhi::CpuAccessMode::Read);
    if (!mMappedBuffer)
    {
        LOG_ERROR("Failed to map ReadBackHeap buffer");
        return nullptr;
    }
    return mpBuffer;
}

// Global readback heap instance definition
ref<ReadbackHeap> gReadbackHeap = nullptr;
