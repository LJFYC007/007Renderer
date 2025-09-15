// Include TinyEXR implementation with miniz support
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <algorithm>
#include <cstring>

#include "ExrUtils.h"
#include "Core/Device.h"
#include "Utils/Logger.h"

void ExrUtils::saveTextureToExr(ref<Device> device, nvrhi::TextureHandle texture, const std::string& filePath)
{
    if (!device || !texture)
        LOG_ERROR_RETURN("Invalid device or texture");

    const auto& desc = texture->getDesc();
    std::vector<float> imageData;
    copyTextureDataToCPU(device, texture, imageData);

    // Prepare EXR image data
    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    // Allocate channel pointers
    uint32_t channelCount = getChannelCount(desc.format);
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

nvrhi::TextureHandle ExrUtils::loadExrToTexture(ref<Device> device, const std::string& filePath)
{
    if (!device)
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
                           .setDebugName("EXR Loaded Texture");

    // Create NVRHI texture
    nvrhi::TextureHandle texture = device->getDevice()->createTexture(textureDesc);
    if (!texture)
    {
        LOG_ERROR("Failed to create NVRHI texture");
        free(imageData);
        return nullptr;
    }

    copyCPUDataToTexture(device, texture, std::vector<float>(imageData, imageData + width * height * 4));
    free(imageData);
    LOG_INFO("Successfully loaded EXR file: {}", filePath);
    return texture;
}

void ExrUtils::copyTextureDataToCPU(ref<Device> device, nvrhi::TextureHandle texture, std::vector<float>& outData)
{
    auto desc = texture->getDesc();
    nvrhi::StagingTextureHandle stagingTexture = device->getDevice()->createStagingTexture(desc, nvrhi::CpuAccessMode::Read);
    if (!stagingTexture)
        LOG_ERROR_RETURN("Failed to create staging texture");

    // Copy from GPU texture to staging texture
    nvrhi::DeviceHandle nvrhiDevice = device->getDevice();
    nvrhi::CommandListHandle commandList = device->getCommandList();
    nvrhi::TextureSlice slice;

    commandList->open();
    commandList->setTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
    commandList->copyTexture(stagingTexture, slice, texture, slice);
    commandList->close();

    // Execute using the correct API - single command list
    uint64_t fenceValue = nvrhiDevice->executeCommandList(commandList);
    nvrhiDevice->queueWaitForCommandList(nvrhi::CommandQueue::Graphics, nvrhi::CommandQueue::Graphics, fenceValue);

    // Map staging texture and copy data
    size_t rowPitch;
    void* mappedData = device->getDevice()->mapStagingTexture(stagingTexture, slice, nvrhi::CpuAccessMode::Read, &rowPitch);
    if (!mappedData)
        LOG_ERROR_RETURN("Failed to map staging texture");

    uint32_t channelCount = getChannelCount(desc.format);
    outData.resize(desc.width * desc.height * channelCount);

    // Copy row by row to handle potential padding
    for (uint32_t row = 0; row < desc.height; ++row)
    {
        const float* srcRow = reinterpret_cast<const float*>(static_cast<const uint8_t*>(mappedData) + row * rowPitch);
        float* dstRow = outData.data() + row * desc.width * channelCount;
        memcpy(dstRow, srcRow, desc.width * channelCount * sizeof(float));
    }

    device->getDevice()->unmapStagingTexture(stagingTexture);
}

void ExrUtils::copyCPUDataToTexture(ref<Device> device, nvrhi::TextureHandle& texture, const std::vector<float> inData)
{
    auto desc = texture->getDesc();
    nvrhi::StagingTextureHandle stagingTexture = device->getDevice()->createStagingTexture(desc, nvrhi::CpuAccessMode::Write);
    if (!stagingTexture)
        LOG_ERROR_RETURN("Failed to create staging texture for upload");

    // Map staging texture and copy data
    nvrhi::TextureSlice slice;
    size_t rowPitch;
    void* mappedData = device->getDevice()->mapStagingTexture(stagingTexture, slice, nvrhi::CpuAccessMode::Write, &rowPitch);
    if (!mappedData)
        LOG_ERROR_RETURN("Failed to map staging texture for upload");

    // Copy row by row to handle potential padding
    uint32_t channelCount = getChannelCount(desc.format);
    for (uint32_t row = 0; row < desc.height; ++row)
    {
        const float* srcRow = inData.data() + row * desc.width * channelCount;
        float* dstRow = reinterpret_cast<float*>(static_cast<uint8_t*>(mappedData) + row * rowPitch);
        memcpy(dstRow, srcRow, desc.width * channelCount * sizeof(float));
    }

    device->getDevice()->unmapStagingTexture(stagingTexture);

    // Copy from staging texture to GPU texture
    nvrhi::DeviceHandle nvrhiDevice = device->getDevice();
    nvrhi::CommandListHandle commandList = device->getCommandList();

    commandList->open();
    commandList->setTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
    commandList->copyTexture(texture, slice, stagingTexture, slice);
    commandList->close();

    // Execute command list and wait for completion
    uint64_t fenceValue = nvrhiDevice->executeCommandList(commandList);
    nvrhiDevice->queueWaitForCommandList(nvrhi::CommandQueue::Graphics, nvrhi::CommandQueue::Graphics, fenceValue);
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
