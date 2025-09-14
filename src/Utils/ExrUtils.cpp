// Include TinyEXR implementation with miniz support
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <algorithm>
#include <cstring>

#include "ExrUtils.h"
#include "Core/Device.h"
#include "Utils/Logger.h"

void ExrUtils::saveTextureToExr(
    ref<Device> device,
    ID3D12Resource* textureResource,
    const std::string& filePath,
    uint32_t width,
    uint32_t height,
    nvrhi::Format format
)
{
    if (!device || !textureResource)
        LOG_ERROR_RETURN("Invalid device or texture resource");
    if (!isSupportedFormat(format))
        LOG_ERROR_RETURN("Unsupported format for EXR export: {}", static_cast<int>(format));

    // Copy texture data from GPU to CPU
    std::vector<float> imageData;
    copyTextureDataToCPU(device, textureResource, width, height, format, imageData);
    uint32_t channelCount = getChannelCount(format);

    // Prepare EXR image data
    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = channelCount;

    // Allocate channel pointers
    std::vector<float*> channelData(channelCount);
    std::vector<std::vector<float>> channels(channelCount);
    // Separate channels - rearrange to match EXR alphabetical order
    for (uint32_t c = 0; c < channelCount; ++c)
    {
        channels[c].resize(width * height);

        // Map RGBA data to EXR's alphabetical channel order
        uint32_t sourceChannel = channelCount - c - 1;
        for (uint32_t i = 0; i < width * height; ++i)
            channels[c][i] = imageData[i * channelCount + sourceChannel];
        channelData[c] = channels[c].data();
    }

    image.images = reinterpret_cast<unsigned char**>(channelData.data());
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);

    header.num_channels = channelCount;
    header.channels = static_cast<EXRChannelInfo*>(malloc(sizeof(EXRChannelInfo) * channelCount));
    // Set channel names - EXR stores channels in alphabetical order (A, B, G, R)
    // So we need to arrange our channel names to match the expected data order
    const char* channelNames[4];
    if (channelCount == 4)
    {
        channelNames[0] = "A"; // Alpha channel first (alphabetically)
        channelNames[1] = "B"; // Blue channel second
        channelNames[2] = "G"; // Green channel third
        channelNames[3] = "R"; // Red channel last
    }
    else if (channelCount == 3)
    {
        channelNames[0] = "B"; // Blue first in alphabetical order
        channelNames[1] = "G"; // Green second
        channelNames[2] = "R"; // Red third
    }
    else if (channelCount == 2)
    {
        channelNames[0] = "G"; // Green first (treating as RG -> GR)
        channelNames[1] = "R"; // Red second
    }
    else if (channelCount == 1)
    {
        channelNames[0] = "R"; // Single channel as Red
    }

    for (uint32_t c = 0; c < channelCount; ++c)
    {
        strncpy(header.channels[c].name, channelNames[c], 255);
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

void ExrUtils::saveTextureToExr(ref<Device> device, nvrhi::TextureHandle texture, const std::string& filePath)
{
    if (!device || !texture)
        LOG_ERROR_RETURN("Invalid device or texture");

    const auto& desc = texture->getDesc();
    // Get the native D3D12 resource
    auto nativeObject = texture->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
    ID3D12Resource* d3d12Resource = static_cast<ID3D12Resource*>(nativeObject.pointer);
    saveTextureToExr(device, d3d12Resource, filePath, desc.width, desc.height, desc.format);
}

nvrhi::TextureHandle ExrUtils::loadExrToTexture(ref<Device> device, const std::string& filePath)
{
    if (!device)
    {
        LOG_ERROR("Invalid device");
        return nullptr;
    }

    EXRVersion exrVersion;
    int ret = ParseEXRVersionFromFile(&exrVersion, filePath.c_str());
    if (ret != TINYEXR_SUCCESS)
    {
        LOG_ERROR("Failed to parse EXR version");
        return nullptr;
    }

    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    const char* err = nullptr;
    ret = ParseEXRHeaderFromFile(&header, &exrVersion, filePath.c_str(), &err);
    if (ret != TINYEXR_SUCCESS)
    {
        LOG_ERROR("Failed to parse EXR header: {}", err ? err : "Unknown error");
        if (err)
            FreeEXRErrorMessage(err);
        FreeEXRHeader(&header);
        return nullptr;
    }

    // Request FLOAT pixels
    for (int i = 0; i < header.num_channels; i++)
        header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;

    ret = LoadEXRImageFromFile(&image, &header, filePath.c_str(), &err);
    if (ret != TINYEXR_SUCCESS)
    {
        LOG_ERROR("Failed to load EXR image: {}", err ? err : "Unknown error");
        if (err)
            FreeEXRErrorMessage(err);
        FreeEXRHeader(&header);
        return nullptr;
    }

    uint32_t width = static_cast<uint32_t>(image.width);
    uint32_t height = static_cast<uint32_t>(image.height);
    uint32_t channelCount = static_cast<uint32_t>(image.num_channels);

    // Determine format based on channel count
    nvrhi::Format format;
    switch (channelCount)
    {
    case 1:
        format = nvrhi::Format::R32_FLOAT;
        break;
    case 2:
        format = nvrhi::Format::RG32_FLOAT;
        break;
    case 3:
    case 4:
        format = nvrhi::Format::RGBA32_FLOAT;
        break;
    default:
        LOG_ERROR("Unsupported channel count: {}", channelCount);
        FreeEXRImage(&image);
        FreeEXRHeader(&header);
        return nullptr;
    }

    // Create texture description
    auto textureDesc = nvrhi::TextureDesc()
                           .setDimension(nvrhi::TextureDimension::Texture2D)
                           .setFormat(format)
                           .setWidth(width)
                           .setHeight(height)
                           .setMipLevels(1)
                           .setArraySize(1)
                           .setIsRenderTarget(false)
                           .setIsUAV(true)
                           .setInitialState(nvrhi::ResourceStates::Common)
                           .setKeepInitialState(false)
                           .setDebugName("EXR Loaded Texture");

    // Create NVRHI texture
    nvrhi::TextureHandle texture = device->getDevice()->createTexture(textureDesc);
    if (!texture)
    {
        LOG_ERROR("Failed to create NVRHI texture");
        FreeEXRImage(&image);
        FreeEXRHeader(&header);
        return nullptr;
    }

    // Convert image data to interleaved format
    std::vector<float> imageData(width * height * 4, 0.0f); // Always use 4 channels for simplicity

    // Find channel indices (EXR channels might be in different order)
    int rIdx = -1, gIdx = -1, bIdx = -1, aIdx = -1;
    for (int c = 0; c < header.num_channels; ++c)
    {
        if (strcmp(header.channels[c].name, "R") == 0)
            rIdx = c;
        else if (strcmp(header.channels[c].name, "G") == 0)
            gIdx = c;
        else if (strcmp(header.channels[c].name, "B") == 0)
            bIdx = c;
        else if (strcmp(header.channels[c].name, "A") == 0)
            aIdx = c;
    }

    // If specific channels not found, use order
    if (rIdx == -1)
        rIdx = 0;
    if (gIdx == -1 && channelCount > 1)
        gIdx = 1;
    if (bIdx == -1 && channelCount > 2)
        bIdx = 2;
    if (aIdx == -1 && channelCount > 3)
        aIdx = 3;

    // Interleave the data
    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            uint32_t pixelIdx = y * width + x;
            uint32_t dataIdx = pixelIdx * 4;

            // Copy R channel
            if (rIdx >= 0 && rIdx < image.num_channels)
            {
                float* channelData = reinterpret_cast<float*>(image.images[rIdx]);
                imageData[dataIdx + 0] = channelData[pixelIdx];
            }

            // Copy G channel
            if (gIdx >= 0 && gIdx < image.num_channels)
            {
                float* channelData = reinterpret_cast<float*>(image.images[gIdx]);
                imageData[dataIdx + 1] = channelData[pixelIdx];
            }

            // Copy B channel
            if (bIdx >= 0 && bIdx < image.num_channels)
            {
                float* channelData = reinterpret_cast<float*>(image.images[bIdx]);
                imageData[dataIdx + 2] = channelData[pixelIdx];
            }

            // Copy A channel (default to 1.0 if not present)
            if (aIdx >= 0 && aIdx < image.num_channels)
            {
                float* channelData = reinterpret_cast<float*>(image.images[aIdx]);
                imageData[dataIdx + 3] = channelData[pixelIdx];
            }
            else
            {
                imageData[dataIdx + 3] = 1.0f;
            }
        }
    }

    FreeEXRImage(&image);
    FreeEXRHeader(&header);
    LOG_INFO("Successfully loaded EXR file: {}", filePath);
    return texture;
}

void ExrUtils::copyTextureDataToCPU(
    ref<Device> device,
    ID3D12Resource* textureResource,
    uint32_t width,
    uint32_t height,
    nvrhi::Format format,
    std::vector<float>& outData
)
{
    // Create staging texture for readback
    auto stagingDesc = nvrhi::TextureDesc()
                           .setDimension(nvrhi::TextureDimension::Texture2D)
                           .setFormat(format)
                           .setWidth(width)
                           .setHeight(height)
                           .setMipLevels(1)
                           .setArraySize(1)
                           .setDebugName("EXR Staging Texture");

    nvrhi::StagingTextureHandle stagingTexture = device->getDevice()->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
    if (!stagingTexture)
        LOG_ERROR_RETURN("Failed to create staging texture"); // Create wrapper for the native D3D12 resource
    // Use UnorderedAccess as the initial state based on the error message
    auto textureDesc = nvrhi::TextureDesc()
                           .setDimension(nvrhi::TextureDimension::Texture2D)
                           .setFormat(format)
                           .setWidth(width)
                           .setHeight(height)
                           .setMipLevels(1)
                           .setArraySize(1)
                           .setInitialState(nvrhi::ResourceStates::UnorderedAccess) // Actual current state
                           .setKeepInitialState(true)                               // Keep the initial state to avoid unnecessary transitions
                           .setDebugName("Source Texture for EXR Export");

    nvrhi::TextureHandle sourceTexture =
        device->getDevice()->createHandleForNativeTexture(nvrhi::ObjectTypes::D3D12_Resource, nvrhi::Object(textureResource), textureDesc);

    if (!sourceTexture)
        LOG_ERROR_RETURN("Failed to create NVRHI handle for native texture");

    // Copy from GPU texture to staging texture
    nvrhi::DeviceHandle nvrhiDevice = device->getDevice();
    nvrhi::CommandListHandle commandList = device->getCommandList();
    commandList->open();

    nvrhi::TextureSlice slice;
    slice.mipLevel = 0;
    slice.arraySlice = 0;
    slice.x = 0;
    slice.y = 0;
    slice.z = 0;
    slice.width = width;
    slice.height = height;
    slice.depth = 1;

    commandList->beginTrackingTextureState(sourceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
    commandList->setTextureState(sourceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
    commandList->copyTexture(stagingTexture, slice, sourceTexture, slice);
    commandList->setTextureState(sourceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::UnorderedAccess);
    commandList->close();

    // Execute using the correct API - single command list
    uint64_t fenceValue = nvrhiDevice->executeCommandList(commandList);
    nvrhiDevice->waitForIdle();

    // Map staging texture and copy data
    size_t rowPitch;
    void* mappedData = device->getDevice()->mapStagingTexture(stagingTexture, slice, nvrhi::CpuAccessMode::Read, &rowPitch);
    if (!mappedData)
        LOG_ERROR_RETURN("Failed to map staging texture");

    uint32_t channelCount = getChannelCount(format);
    outData.resize(width * height * channelCount);

    // Copy row by row to handle potential padding
    for (uint32_t row = 0; row < height; ++row)
    {
        const float* srcRow = reinterpret_cast<const float*>(static_cast<const uint8_t*>(mappedData) + row * rowPitch);
        float* dstRow = outData.data() + row * width * channelCount;
        memcpy(dstRow, srcRow, width * channelCount * sizeof(float));
    }

    device->getDevice()->unmapStagingTexture(stagingTexture);
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
