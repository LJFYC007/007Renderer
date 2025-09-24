#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>

#include "Core/Device.h"
#include "ShaderPasses/ComputePass.h"
#include "Utils/ResourceIO.h"
#include "Environment.h"

#include <nvrhi/nvrhi.h>

namespace
{
struct alignas(16) ConstantBufferData
{
    float scalarValue = 0.f;
    int32_t integerValue = 0;
    int32_t flagValue = 0; // Stored as 32-bit to match Slang constant buffer layout
    float padding = 0.f;   // Keeps vectorValue aligned to the next 16-byte boundary
    float vectorValue[3] = {0.f, 0.f, 0.f};

    void setFlag(bool value) { flagValue = value ? 1 : 0; }
    bool getFlag() const { return flagValue != 0; }
};

static_assert(sizeof(ConstantBufferData) % 16 == 0, "Constant buffer data must stay 16-byte aligned.");

struct Float4
{
    float x;
    float y;
    float z;
    float w;
};

std::vector<uint8_t> readbackBuffer(ref<Device> device, nvrhi::BufferHandle buffer, size_t byteSize)
{
    std::vector<uint8_t> result(byteSize);
    if (byteSize == 0)
        return result;

    if (!ResourceIO::readbackBuffer(device, buffer, result.data(), byteSize))
        result.clear();

    return result;
}

} // namespace

class SlangTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mpDevice = BasicTestEnvironment::getDevice();
        ASSERT_NE(mpDevice, nullptr);
        ASSERT_TRUE(mpDevice->isValid());
    }

    ref<Device> mpDevice;
};

TEST_F(SlangTest, Basic)
{
    const uint32_t elementCount = 4;

    // Structured buffer data
    std::vector<float> structuredBufferData = {1.25f, 2.5f, 3.75f, 5.0f};
    const size_t structuredBufferBytes = structuredBufferData.size() * sizeof(float);
    nvrhi::BufferDesc structuredBufferDesc;
    structuredBufferDesc.byteSize = structuredBufferBytes;
    structuredBufferDesc.structStride = sizeof(float);
    structuredBufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    structuredBufferDesc.keepInitialState = true;
    structuredBufferDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    structuredBufferDesc.debugName = "ReflectionInputBuffer";
    nvrhi::BufferHandle structuredBuffer = mpDevice->getDevice()->createBuffer(structuredBufferDesc);
    ASSERT_TRUE(structuredBuffer);
    ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, structuredBuffer, structuredBufferData.data(), structuredBufferBytes));

    // Constant buffer data (true flag)
    ConstantBufferData constantsTrue{};
    constantsTrue.scalarValue = 0.5f;
    constantsTrue.integerValue = 3;
    constantsTrue.setFlag(true);
    constantsTrue.vectorValue[0] = 0.125f;
    constantsTrue.vectorValue[1] = 0.25f;
    constantsTrue.vectorValue[2] = 0.75f;

    nvrhi::BufferDesc constantBufferDesc;
    constantBufferDesc.byteSize = sizeof(constantsTrue);
    constantBufferDesc.isConstantBuffer = true;
    constantBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    constantBufferDesc.keepInitialState = true;
    constantBufferDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    constantBufferDesc.debugName = "ReflectionConstants";
    nvrhi::BufferHandle constantBuffer = mpDevice->getDevice()->createBuffer(constantBufferDesc);
    ASSERT_TRUE(constantBuffer);
    ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, constantBuffer, &constantsTrue, sizeof(constantsTrue)));

    // Texture data (each thread reads a different texel)
    std::vector<Float4> textureData = {
        {0.1f, 0.6f, 0.2f, 0.7f},
        {0.2f, 0.5f, 0.3f, 0.8f},
        {0.3f, 0.4f, 0.4f, 0.9f},
        {0.4f, 0.3f, 0.5f, 1.0f},
    };

    nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
                                         .setDimension(nvrhi::TextureDimension::Texture2D)
                                         .setWidth(elementCount)
                                         .setHeight(1)
                                         .setMipLevels(1)
                                         .setArraySize(1)
                                         .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                         .setInitialState(nvrhi::ResourceStates::CopyDest)
                                         .setKeepInitialState(true)
                                         .setIsRenderTarget(false)
                                         .setIsUAV(false)
                                         .setDebugName("ReflectionInputTexture");

    nvrhi::TextureHandle texture = mpDevice->getDevice()->createTexture(textureDesc);
    ASSERT_TRUE(texture);

    auto commandList = mpDevice->getCommandList();
    commandList->open();
    commandList->writeTexture(texture, 0, 0, textureData.data(), elementCount * sizeof(Float4), 0);
    commandList->setTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
    commandList->close();
    mpDevice->getDevice()->executeCommandList(commandList);

    // Output UAV buffer (float4 per element)
    const size_t outputByteSize = elementCount * sizeof(Float4);
    nvrhi::BufferDesc outputDesc;
    outputDesc.byteSize = outputByteSize;
    outputDesc.structStride = sizeof(Float4);
    outputDesc.canHaveUAVs = true;
    outputDesc.keepInitialState = true;
    outputDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    outputDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    outputDesc.debugName = "ReflectionOutputBuffer";
    nvrhi::BufferHandle outputBuffer = mpDevice->getDevice()->createBuffer(outputDesc);
    ASSERT_TRUE(outputBuffer);

    // Dispatch compute shader using reflection-driven bindings
    ref<Pass> pass = make_ref<ComputePass>(mpDevice, "/tests/SlangTest.slang", "computeMain");
    (*pass)["gInputBuffer"] = structuredBuffer;
    (*pass)["gInputTexture"] = texture;
    (*pass)["gConstants"] = constantBuffer;
    (*pass)["gOutputBuffer"] = outputBuffer;

    pass->execute(elementCount, 1, 1);

    auto firstResultBytes = readbackBuffer(mpDevice, outputBuffer, outputByteSize);
    ASSERT_EQ(firstResultBytes.size(), outputByteSize);
    const Float4* firstResults = reinterpret_cast<const Float4*>(firstResultBytes.data());

    const float expectedFirstAccumulator = constantsTrue.scalarValue + static_cast<float>(constantsTrue.integerValue) +
                                           (constantsTrue.getFlag() ? 1.0f : 0.0f) + constantsTrue.vectorValue[2];
    for (uint32_t i = 0; i < elementCount; ++i)
    {
        EXPECT_FLOAT_EQ(firstResults[i].x, structuredBufferData[i]);
        EXPECT_FLOAT_EQ(firstResults[i].y, textureData[i].y);
        EXPECT_FLOAT_EQ(firstResults[i].z, expectedFirstAccumulator);
        EXPECT_FLOAT_EQ(firstResults[i].w, constantsTrue.vectorValue[1]);
    }

    // Update constants (false flag, different scalar) and re-dispatch to catch padding bugs
    ConstantBufferData constantsFalse = constantsTrue;
    constantsFalse.scalarValue = 1.25f;
    constantsFalse.setFlag(false);
    ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, constantBuffer, &constantsFalse, sizeof(constantsFalse)));

    pass->execute(elementCount, 1, 1);

    auto secondResultBytes = readbackBuffer(mpDevice, outputBuffer, outputByteSize);
    ASSERT_EQ(secondResultBytes.size(), outputByteSize);
    const Float4* secondResults = reinterpret_cast<const Float4*>(secondResultBytes.data());

    const float expectedSecondAccumulator = constantsFalse.scalarValue + static_cast<float>(constantsFalse.integerValue) +
                                            (constantsFalse.getFlag() ? 1.0f : 0.0f) + constantsFalse.vectorValue[2];
    for (uint32_t i = 0; i < elementCount; ++i)
    {
        EXPECT_FLOAT_EQ(secondResults[i].x, structuredBufferData[i]);
        EXPECT_FLOAT_EQ(secondResults[i].y, textureData[i].y);
        EXPECT_FLOAT_EQ(secondResults[i].z, expectedSecondAccumulator);
        EXPECT_FLOAT_EQ(secondResults[i].w, constantsFalse.vectorValue[1]);
    }
}
