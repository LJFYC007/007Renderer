#pragma once
#include <vector>
#include <string>
#include <nvrhi/nvrhi.h>

#include "Core/Pointer.h"

class Device;

static const uint32_t kInvalidTextureId = 0xFFFFFFFF;

// Manages texture resources and handles CPU-to-GPU texture uploads
class TextureManager
{
public:
    TextureManager(ref<Device> device);
    ~TextureManager() = default;

    // Load texture from CPU memory to GPU
    uint32_t loadTexture(const float* data, uint32_t width, uint32_t height, uint32_t channels, const std::string& debugName = "");

    // Get texture by ID
    nvrhi::TextureHandle getTexture(uint32_t textureId) const;

    // Get all textures
    const std::vector<nvrhi::TextureHandle>& getAllTextures() const { return mTextures; }
    size_t getTextureCount() const { return mTextures.size(); }

    // Get default 1x1 white texture for unused slots
    nvrhi::TextureHandle getDefaultTexture() const { return mDefaultTexture; }

    // Create default textures (call after construction)
    void initialize();

private:
    ref<Device> mpDevice;
    std::vector<nvrhi::TextureHandle> mTextures;
    nvrhi::TextureHandle mDefaultTexture;

    // Helper: determine GPU format based on channel count
    nvrhi::Format determineFormat(uint32_t channels, uint32_t& outGpuChannels) const;

    // Helper: prepare upload data (handle RGB->RGBA conversion)
    std::vector<float> prepareUploadData(const float* data, uint32_t width, uint32_t height, uint32_t channels, uint32_t gpuChannels) const;
};
