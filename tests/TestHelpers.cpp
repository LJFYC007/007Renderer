#include "TestHelpers.h"

#include <filesystem>

#include "Utils/ResourceIO.h"

namespace TestHelpers
{
static std::string sanitizeForPath(std::string s)
{
    for (char& c : s)
        if (c == '/' || c == '\\' || c == ':')
            c = '_';
    return s;
}

std::string artifactDir()
{
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string suite = info ? info->test_suite_name() : "Unknown";
    std::string name = info ? info->name() : "Unknown";
    std::string dir = std::string(PROJECT_DIR) + "/tests/artifacts/" + sanitizeForPath(suite) + "." + sanitizeForPath(name);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string artifactPath(const std::string& filename)
{
    return artifactDir() + "/" + filename;
}

std::vector<uint8_t> readbackBuffer(ref<Device> device, nvrhi::BufferHandle buffer, size_t byteSize)
{
    std::vector<uint8_t> result(byteSize);
    if (byteSize == 0)
        return result;

    if (!ResourceIO::readbackBuffer(device, buffer, result.data(), byteSize))
        result.clear();

    return result;
}

nvrhi::BufferHandle createStructuredBufferSRV(ref<Device> device, const void* data, size_t byteSize, size_t stride, const char* name)
{
    nvrhi::BufferDesc desc;
    desc.byteSize = byteSize;
    desc.structStride = static_cast<uint32_t>(stride);
    desc.initialState = nvrhi::ResourceStates::ShaderResource;
    desc.keepInitialState = true;
    desc.cpuAccess = nvrhi::CpuAccessMode::None;
    desc.debugName = name;
    nvrhi::BufferHandle buffer = device->getDevice()->createBuffer(desc);
    if (buffer && data)
        ResourceIO::uploadBuffer(device, buffer, data, byteSize);
    return buffer;
}

nvrhi::BufferHandle createStructuredBufferUAV(ref<Device> device, size_t byteSize, size_t stride, const char* name)
{
    nvrhi::BufferDesc desc;
    desc.byteSize = byteSize;
    desc.structStride = static_cast<uint32_t>(stride);
    desc.canHaveUAVs = true;
    desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    desc.keepInitialState = true;
    desc.cpuAccess = nvrhi::CpuAccessMode::None;
    desc.debugName = name;
    return device->getDevice()->createBuffer(desc);
}

nvrhi::TextureHandle createFloat4Texture1D(ref<Device> device, const float4* texels, uint32_t width, const char* name)
{
    nvrhi::TextureDesc desc = nvrhi::TextureDesc()
                                  .setDimension(nvrhi::TextureDimension::Texture2D)
                                  .setWidth(width)
                                  .setHeight(1)
                                  .setMipLevels(1)
                                  .setArraySize(1)
                                  .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                  .setInitialState(nvrhi::ResourceStates::ShaderResource)
                                  .setKeepInitialState(true)
                                  .setDebugName(name);
    nvrhi::TextureHandle texture = device->getDevice()->createTexture(desc);
    if (!texture)
        return nullptr;

    if (!ResourceIO::uploadTexture(device, texture, texels, width * sizeof(float4)))
        return nullptr;
    return texture;
}
} // namespace TestHelpers
