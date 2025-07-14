#include "Buffer.h"
#include <cstring>
#include <iostream>

bool Buffer::initialize(
    ref<Device> device,
    const void* data,
    size_t size,
    nvrhi::ResourceStates initState,
    bool isUAV,
    bool isConstantBuffer,
    const std::string& name
)
{
    byteSize = size;

    nvrhi::BufferDesc desc;
    desc.byteSize = size;
    desc.structStride = sizeof(float);
    desc.debugName = name.c_str();
    desc.initialState = initState;
    desc.cpuAccess = nvrhi::CpuAccessMode::None;
    desc.canHaveUAVs = isUAV;
    desc.isConstantBuffer = isConstantBuffer;

    buffer = device->getDevice()->createBuffer(desc);

    if (data)
        upload(device, data, size);

    return buffer;
}

void Buffer::upload(ref<Device> device, const void* data, size_t size)
{
    if (!buffer)
        return;

    auto uploadCmd = device->getCommandList();
    uploadCmd->open();
    uploadCmd->beginTrackingBufferState(buffer, nvrhi::ResourceStates::ShaderResource);
    uploadCmd->writeBuffer(buffer, data, size);
    uploadCmd->close();
    device->getDevice()->executeCommandList(uploadCmd);
}

void Buffer::updateData(ref<Device> device, const void* data, size_t size)
{
    if (!buffer || !data)
        return;

    upload(device, data, size);
}

Buffer Buffer::createReadback(ref<Device> device, size_t size)
{
    Buffer buf;
    buf.byteSize = size;

    nvrhi::BufferDesc desc;
    desc.byteSize = size;
    desc.initialState = nvrhi::ResourceStates::CopyDest;
    desc.cpuAccess = nvrhi::CpuAccessMode::Read;
    desc.debugName = "ReadbackBuffer";

    buf.buffer = device->getDevice()->createBuffer(desc);
    return buf;
}

std::vector<uint8_t> Buffer::readback(ref<Device> device) const
{
    nvrhi::DeviceHandle nvrhiDevice = device->getDevice();
    nvrhi::CommandListHandle commandList = device->getCommandList();

    std::vector<uint8_t> result(byteSize);
    commandList->open();
    commandList->beginTrackingBufferState(buffer, nvrhi::ResourceStates::CopySource);

    auto staging = createReadback(device, byteSize);
    commandList->beginTrackingBufferState(staging.buffer, nvrhi::ResourceStates::CopyDest);

    commandList->copyBuffer(staging.buffer, 0, buffer, 0, byteSize);
    commandList->close();
    nvrhiDevice->executeCommandList(commandList);
    nvrhiDevice->waitForIdle();

    void* mapped = nvrhiDevice->mapBuffer(staging.buffer, nvrhi::CpuAccessMode::Read);
    if (mapped)
    {
        std::memcpy(result.data(), mapped, byteSize);
        nvrhiDevice->unmapBuffer(staging.buffer);
    }
    return result;
}
