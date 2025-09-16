#include "Buffer.h"
#include <cstring>
#include <iostream>

bool Buffer::initialize(
    ref<Device> pDevice,
    const void* pData,
    size_t size,
    nvrhi::ResourceStates initState,
    bool isUAV,
    bool isConstantBuffer,
    const std::string& name
)
{
    mByteSize = size;

    nvrhi::BufferDesc desc;
    desc.byteSize = size;
    desc.structStride = sizeof(float);
    desc.debugName = name.c_str();
    desc.initialState = initState;
    desc.cpuAccess = nvrhi::CpuAccessMode::None;
    desc.canHaveUAVs = isUAV;
    desc.isConstantBuffer = isConstantBuffer;
    desc.keepInitialState = true;
    mBuffer = pDevice->getDevice()->createBuffer(desc);
    if (pData)
        upload(pDevice, pData, size);

    return mBuffer;
}

void Buffer::upload(ref<Device> pDevice, const void* pData, size_t size)
{
    if (!mBuffer)
        return;

    auto pUploadCmd = pDevice->getCommandList();
    pUploadCmd->open();
    pUploadCmd->writeBuffer(mBuffer, pData, size);
    pUploadCmd->close();
    pDevice->getDevice()->executeCommandList(pUploadCmd);
}

void Buffer::updateData(ref<Device> pDevice, const void* pData, size_t size)
{
    if (!mBuffer || !pData)
        return;
    upload(pDevice, pData, size);
}

Buffer Buffer::createReadback(ref<Device> pDevice, size_t size)
{
    Buffer buf;
    buf.mByteSize = size;

    nvrhi::BufferDesc desc;
    desc.byteSize = size;
    desc.initialState = nvrhi::ResourceStates::CopyDest;
    desc.cpuAccess = nvrhi::CpuAccessMode::Read;
    desc.debugName = "ReadbackBuffer";
    desc.keepInitialState = true;
    buf.mBuffer = pDevice->getDevice()->createBuffer(desc);
    return buf;
}

std::vector<uint8_t> Buffer::readback(ref<Device> pDevice) const
{
    nvrhi::DeviceHandle pNvrhiDevice = pDevice->getDevice();
    nvrhi::CommandListHandle pCommandList = pDevice->getCommandList();

    std::vector<uint8_t> result(mByteSize);
    auto staging = createReadback(pDevice, mByteSize);
    pCommandList->open();
    pCommandList->copyBuffer(staging.mBuffer, 0, mBuffer, 0, mByteSize);
    pCommandList->close();
    uint64_t fenceValue = pNvrhiDevice->executeCommandList(pCommandList);
    pNvrhiDevice->queueWaitForCommandList(nvrhi::CommandQueue::Graphics, nvrhi::CommandQueue::Graphics, fenceValue);

    void* pMapped = pNvrhiDevice->mapBuffer(staging.mBuffer, nvrhi::CpuAccessMode::Read);
    if (pMapped)
    {
        std::memcpy(result.data(), pMapped, mByteSize);
        pNvrhiDevice->unmapBuffer(staging.mBuffer);
    }
    return result;
}
