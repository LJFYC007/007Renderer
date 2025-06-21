#pragma once
#include <nvrhi/nvrhi.h>
#include <vector>
#include <cstdint>
#include <string>

class Texture
{
public:
    Texture() = default;

    bool initialize(
        nvrhi::IDevice* device,
        uint32_t width,
        uint32_t height,
        nvrhi::Format format,
        nvrhi::ResourceStates initState,
        bool isUAV = false,
        const std::string& debugName = "Texture"
    );

    nvrhi::TextureHandle getHandle() const { return texture; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    size_t getElementSize() const { return elementSize; }

private:
    nvrhi::TextureHandle texture;
    uint32_t width = 0;
    uint32_t height = 0;
    size_t elementSize = 0;
};
