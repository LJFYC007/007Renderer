#pragma once
#include <nvrhi/nvrhi.h>
#include <d3d12.h>
#include <string>
#include <memory>

#include "Core/Pointer.h"

class Device;

class ExrUtils
{
    // Assumes linear color space and float4 format
public:
    // Save an NVRHI texture to EXR file
    static void saveTextureToExr(ref<Device> device, nvrhi::TextureHandle texture, const std::string& filePath);

    // Load EXR file and create NVRHI texture
    static nvrhi::TextureHandle ExrUtils::loadExrToTexture(ref<Device> device, const std::string& filePath);

private:
    // Internal helper to copy GPU texture data to CPU memory
    static void copyTextureDataToCPU(ref<Device> device, nvrhi::TextureHandle texture, std::vector<float>& outData);

    // Internal helper to copy CPU data to GPU texture
    static void copyCPUDataToTexture(ref<Device> device, nvrhi::TextureHandle& texture, const std::vector<float> inData);

    // Convert NVRHI format to number of channels
    static uint32_t getChannelCount(nvrhi::Format format);

    // Check if format is supported for EXR operations
    static bool isSupportedFormat(nvrhi::Format format);
};
