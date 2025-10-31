#include "TextureManager.h"
#include "Core/Device.h"
#include "Utils/Logger.h"
#include "Utils/ResourceIO.h"

TextureManager::TextureManager(ref<Device> device) : mpDevice(device) {}

void TextureManager::initialize()
{
    // Create default 1x1 white texture
    float defaultTextureData[] = {1.0f, 1.0f, 1.0f, 1.0f};
    auto textureDesc = nvrhi::TextureDesc()
                           .setDimension(nvrhi::TextureDimension::Texture2D)
                           .setWidth(1)
                           .setHeight(1)
                           .setMipLevels(1)
                           .setFormat(nvrhi::Format::RGBA32_FLOAT)
                           .setInitialState(nvrhi::ResourceStates::ShaderResource)
                           .setKeepInitialState(true)
                           .setDebugName("Default White Texture");

    auto nvrhiDevice = mpDevice->getDevice();
    mDefaultTexture = nvrhiDevice->createTexture(textureDesc);

    if (mDefaultTexture)
    {
        size_t dataSize = 4 * sizeof(float);
        if (!ResourceIO::uploadTexture(mpDevice, mDefaultTexture, defaultTextureData, dataSize))
            LOG_ERROR("Failed to upload default texture");
    }
}

uint32_t TextureManager::loadTexture(const float* data, uint32_t width, uint32_t height, uint32_t channels, const std::string& debugName)
{
    if (!data || width == 0 || height == 0 || channels == 0)
    {
        LOG_ERROR("Invalid texture parameters for '{}'", debugName);
        return kInvalidTextureId;
    }

    uint32_t gpuChannels = channels;
    nvrhi::Format format = determineFormat(channels, gpuChannels);

    auto textureDesc = nvrhi::TextureDesc()
                           .setDimension(nvrhi::TextureDimension::Texture2D)
                           .setWidth(width)
                           .setHeight(height)
                           .setMipLevels(1)
                           .setFormat(format)
                           .setInitialState(nvrhi::ResourceStates::ShaderResource)
                           .setKeepInitialState(true)
                           .setDebugName(debugName);

    auto nvrhiDevice = mpDevice->getDevice();
    nvrhi::TextureHandle texture = nvrhiDevice->createTexture(textureDesc);
    if (!texture)
    {
        LOG_ERROR("Failed to create texture '{}'", debugName);
        return kInvalidTextureId;
    }

    std::vector<float> uploadData = prepareUploadData(data, width, height, channels, gpuChannels);

    size_t dataSize = width * height * gpuChannels * sizeof(float);
    if (!ResourceIO::uploadTexture(mpDevice, texture, uploadData.data(), dataSize))
    {
        LOG_ERROR("Failed to upload texture '{}'", debugName);
        return kInvalidTextureId;
    }

    mTextures.push_back(texture);
    LOG_DEBUG("Loaded texture '{}' ({}x{}, {} channels)", debugName, width, height, channels);
    return static_cast<uint32_t>(mTextures.size() - 1);
}

nvrhi::TextureHandle TextureManager::getTexture(uint32_t textureId) const
{
    if (textureId < mTextures.size())
        return mTextures[textureId];
    return nullptr;
}

nvrhi::Format TextureManager::determineFormat(uint32_t channels, uint32_t& outGpuChannels) const
{
    nvrhi::Format format;
    outGpuChannels = channels;

    if (channels == 1)
        format = nvrhi::Format::R32_FLOAT;
    else if (channels == 2)
        format = nvrhi::Format::RG32_FLOAT;
    else if (channels == 3 || channels == 4)
    {
        format = nvrhi::Format::RGBA32_FLOAT;
        outGpuChannels = 4;
    }
    else
    {
        LOG_WARN("Unsupported channel count: {}, defaulting to RGBA", channels);
        format = nvrhi::Format::RGBA32_FLOAT;
        outGpuChannels = 4;
    }

    return format;
}

std::vector<float> TextureManager::prepareUploadData(
    const float* data,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    uint32_t gpuChannels
) const
{
    std::vector<float> uploadData;

    // RGB needs conversion to RGBA
    if (channels == 3 && gpuChannels == 4)
    {
        uploadData.resize(width * height * 4);
        for (uint32_t i = 0; i < width * height; ++i)
        {
            uploadData[i * 4] = data[i * 3];
            uploadData[i * 4 + 1] = data[i * 3 + 1];
            uploadData[i * 4 + 2] = data[i * 3 + 2];
            uploadData[i * 4 + 3] = 1.0f;
        }
    }
    else
        uploadData.assign(data, data + width * height * channels);

    return uploadData;
}
