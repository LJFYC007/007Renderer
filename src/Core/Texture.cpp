#include "Texture.h"
#include <cstring>
#include <iostream>

bool Texture::initialize(
    nvrhi::IDevice* device,
    uint32_t width,
    uint32_t height,
    nvrhi::Format format,
    nvrhi::ResourceStates initState,
    bool isUAV,
    const std::string& debugName
)
{
    width = width;
    height = height;

    nvrhi::TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.arraySize = 1;
    desc.mipLevels = 1;
    desc.sampleCount = 1;
    desc.sampleQuality = 0;
    desc.format = format;
    desc.dimension = nvrhi::TextureDimension::Texture2D;
    desc.debugName = debugName.c_str();
    desc.initialState = initState;
    desc.keepInitialState = false;
    desc.isRenderTarget = false;
    desc.isUAV = isUAV;
    desc.useClearValue = false;

    texture = device->createTexture(desc);
    return texture;
}
