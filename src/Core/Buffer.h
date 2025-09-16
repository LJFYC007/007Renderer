#pragma once
#include <nvrhi/nvrhi.h>
#include <vector>
#include <cstdint>
#include <string>

#include "Core/Device.h"

class Buffer
{
public:
    Buffer() = default;

    bool initialize(
        ref<Device> device,
        const void* data,
        size_t size,
        nvrhi::ResourceStates initState,
        bool isUAV = false,
        bool isConstantBuffer = false,
        const std::string& debugName = "Buffer"
    );

    void updateData(ref<Device> device, const void* data, size_t size);

    std::vector<uint8_t> readback(ref<Device> device) const;
    nvrhi::BufferHandle getHandle() const { return mBuffer; }
    size_t getSize() const { return mByteSize; }

private:
    static Buffer createReadback(ref<Device> device, size_t size);

    void upload(ref<Device> device, const void* data, size_t size);

    nvrhi::BufferHandle mBuffer;
    size_t mByteSize = 0;
};
