#pragma once
#include <nvrhi/nvrhi.h>
#include <vector>
#include <cstdint>
#include <string>

class Buffer
{
public:
    Buffer() = default;

    bool initialize(
        nvrhi::IDevice* device,
        const void* data,
        size_t size,
        nvrhi::ResourceStates initState,
        bool isUAV = false,
        const std::string& debugName = "Buffer"
    );

    std::vector<uint8_t> readback(nvrhi::IDevice* device, nvrhi::CommandListHandle commandList) const;

    nvrhi::BufferHandle getHandle() const { return buffer; }
    size_t getSize() const { return byteSize; }

private:
    static Buffer createReadback(nvrhi::IDevice* device, size_t size);

    void upload(nvrhi::IDevice* device, const void* data, size_t size);

    nvrhi::BufferHandle buffer;
    size_t byteSize = 0;
};
