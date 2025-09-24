// Include TinyEXR implementation with miniz support
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "ExrUtils.h"
#include "Core/Device.h"
#include "Utils/Logger.h"
#include "Utils/ResourceIO.h"

void ExrUtils::saveTextureToExr(ref<Device> pDevice, nvrhi::TextureHandle texture, const std::string& filePath)
{
    if (!pDevice || !texture)
        LOG_ERROR_RETURN("Invalid device or texture");

    const auto& desc = texture->getDesc();
    if (!isSupportedFormat(desc.format))
    {
        LOG_ERROR("Unsupported texture format for EXR export");
        return;
    }

    const uint32_t channelCount = getChannelCount(desc.format);
    if (channelCount == 0)
    {
        LOG_ERROR("Failed to determine channel count for EXR export");
        return;
    }

    std::vector<float> imageData(static_cast<size_t>(desc.width) * desc.height * channelCount);
    if (!ResourceIO::readbackTexture(pDevice, texture, imageData.data(), imageData.size() * sizeof(float)))
    {
        LOG_ERROR("Failed to read back texture data for EXR export");
        return;
    }

    // Prepare EXR image data
    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    // Allocate channel pointers
    std::vector<float*> channelData(channelCount);
    std::vector<std::vector<float>> channels(channelCount);
    // Separate channels - rearrange to match EXR alphabetical order
    for (uint32_t c = 0; c < channelCount; ++c)
    {
        channels[c].resize(desc.width * desc.height);

        // Map RGBA data to EXR's alphabetical channel order
        uint32_t sourceChannel = channelCount - c - 1;
        for (uint32_t i = 0; i < desc.width * desc.height; ++i)
            channels[c][i] = imageData[i * channelCount + sourceChannel];
        channelData[c] = channels[c].data();
    }

    image.images = reinterpret_cast<unsigned char**>(channelData.data());
    image.width = static_cast<int>(desc.width);
    image.height = static_cast<int>(desc.height);
    header.num_channels = channelCount;
    header.channels = static_cast<EXRChannelInfo*>(malloc(sizeof(EXRChannelInfo) * channelCount));

    // EXR requires channel names in alphabetical order
    static const char* channelMap[][4] = {
        {"R"},               // 1 channel
        {"G", "R"},          // 2 channels
        {"B", "G", "R"},     // 3 channels
        {"A", "B", "G", "R"} // 4 channels
    };

    for (uint32_t c = 0; c < channelCount; ++c)
    {
        strncpy(header.channels[c].name, channelMap[channelCount - 1][c], 255);
        header.channels[c].name[255] = '\0';
        header.channels[c].pixel_type = TINYEXR_PIXELTYPE_FLOAT;
        header.channels[c].p_linear = 0;
    }

    header.pixel_types = static_cast<int*>(malloc(sizeof(int) * channelCount));
    header.requested_pixel_types = static_cast<int*>(malloc(sizeof(int) * channelCount));
    for (uint32_t c = 0; c < channelCount; ++c)
    {
        header.pixel_types[c] = TINYEXR_PIXELTYPE_FLOAT;
        header.requested_pixel_types[c] = TINYEXR_PIXELTYPE_FLOAT;
    }

    // Save EXR file
    const char* err = nullptr;
    int ret = SaveEXRImageToFile(&image, &header, filePath.c_str(), &err);

    // Cleanup
    free(header.channels);
    free(header.pixel_types);
    free(header.requested_pixel_types);

    if (ret != TINYEXR_SUCCESS)
    {
        LOG_ERROR("Failed to save EXR file: {}", err ? err : "Unknown error");
        if (err)
            FreeEXRErrorMessage(err);
        return;
    }
    LOG_INFO("Successfully saved EXR file: {}", filePath);
}

nvrhi::TextureHandle ExrUtils::loadExrToTexture(ref<Device> pDevice, const std::string& filePath)
{
    if (!pDevice)
    {
        LOG_ERROR("Invalid device");
        return nullptr;
    }

    float* imageData = nullptr;
    int width, height;
    const char* err = nullptr;

    int ret = LoadEXR(&imageData, &width, &height, filePath.c_str(), &err);
    if (ret != TINYEXR_SUCCESS)
    {
        LOG_ERROR("Failed to load EXR file: {}", err ? err : "Unknown error");
        if (err)
            FreeEXRErrorMessage(err);
        return nullptr;
    }

    // Create texture description - LoadEXR always returns RGBA format
    auto textureDesc = nvrhi::TextureDesc()
                           .setDimension(nvrhi::TextureDimension::Texture2D)
                           .setFormat(nvrhi::Format::RGBA32_FLOAT)
                           .setWidth(static_cast<uint32_t>(width))
                           .setHeight(static_cast<uint32_t>(height))
                           .setMipLevels(1)
                           .setArraySize(1)
                           .setIsRenderTarget(false)
                           .setIsUAV(true)
                           .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                           .setKeepInitialState(true)
                           .setDebugName("EXR Loaded Texture"); // Create NVRHI texture
    nvrhi::TextureHandle texture = pDevice->getDevice()->createTexture(textureDesc);
    if (!texture)
    {
        LOG_ERROR("Failed to create NVRHI texture");
        free(imageData);
        return nullptr;
    }

    std::vector<float> cpuData(imageData, imageData + static_cast<size_t>(width) * height * 4);
    free(imageData);

    if (!ResourceIO::uploadTexture(pDevice, texture, cpuData.data(), cpuData.size() * sizeof(float)))
    {
        LOG_ERROR("Failed to upload EXR data to GPU texture");
        texture = nullptr;
        return nullptr;
    }

    LOG_INFO("Successfully loaded EXR file: {}", filePath);
    return texture;
}

uint32_t ExrUtils::getChannelCount(nvrhi::Format format)
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
        LOG_WARN("Unsupported format, assuming 4 channels");
        return 4;
    }
}

bool ExrUtils::isSupportedFormat(nvrhi::Format format)
{
    switch (format)
    {
    case nvrhi::Format::R32_FLOAT:
    case nvrhi::Format::R16_FLOAT:
    case nvrhi::Format::RG32_FLOAT:
    case nvrhi::Format::RG16_FLOAT:
    case nvrhi::Format::RGB32_FLOAT:
    case nvrhi::Format::RGBA32_FLOAT:
    case nvrhi::Format::RGBA16_FLOAT:
        return true;
    default:
        return false;
    }
}
