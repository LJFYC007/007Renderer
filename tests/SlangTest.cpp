#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>

#include "Core/Device.h"
#include "ShaderPasses/ComputePass.h"
#include "Utils/ResourceIO.h"
#include "Environment.h"
#include "TestHelpers.h"

#include <nvrhi/nvrhi.h>

namespace
{
struct alignas(16) ConstantBufferData
{
    float scalarValue = 0.f;
    int32_t integerValue = 0;
    int32_t flagValue = 0; // Stored as 32-bit to match Slang constant buffer layout
    float _padding = 0.f;  // Keeps vectorValue aligned to the next 16-byte boundary
    float vectorValue[3] = {0.f, 0.f, 0.f};

    void setFlag(bool value) { flagValue = value ? 1 : 0; }
    bool getFlag() const { return flagValue != 0; }
};

static_assert(sizeof(ConstantBufferData) % 16 == 0, "Constant buffer data must stay 16-byte aligned.");
} // namespace

class SlangTest : public DeviceTest
{};

TEST_F(SlangTest, All)
{
    const uint32_t elementCount = 4;
    const uint32_t dispatchWidth = 8;
    const uint32_t dispatchHeight = 8;

    std::vector<float> structuredBufferData = {1.25f, 2.5f, 3.75f, 5.0f};
    nvrhi::BufferHandle structuredBuffer = TestHelpers::createStructuredBufferSRV(
        mpDevice, structuredBufferData.data(), structuredBufferData.size() * sizeof(float), sizeof(float), "BasicInputBuffer"
    );
    ASSERT_TRUE(structuredBuffer);

    ConstantBufferData constantsTrue{};
    constantsTrue.scalarValue = 0.5f;
    constantsTrue.integerValue = 3;
    constantsTrue.setFlag(true);
    constantsTrue.vectorValue[0] = 0.125f;
    constantsTrue.vectorValue[1] = 0.25f;
    constantsTrue.vectorValue[2] = 0.75f;

    nvrhi::BufferDesc cbDesc;
    cbDesc.byteSize = sizeof(constantsTrue);
    cbDesc.isConstantBuffer = true;
    cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    cbDesc.keepInitialState = true;
    cbDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    cbDesc.debugName = "BasicConstants";
    nvrhi::BufferHandle constantBuffer = mpDevice->getDevice()->createBuffer(cbDesc);
    ASSERT_TRUE(constantBuffer);
    ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, constantBuffer, &constantsTrue, sizeof(constantsTrue)));

    std::vector<float4> textureData = {
        {0.1f, 0.6f, 0.2f, 0.7f},
        {0.2f, 0.5f, 0.3f, 0.8f},
        {0.3f, 0.4f, 0.4f, 0.9f},
        {0.4f, 0.3f, 0.5f, 1.0f},
    };
    nvrhi::TextureHandle basicTex = TestHelpers::createFloat4Texture1D(mpDevice, textureData.data(), elementCount, "BasicTexture");
    ASSERT_TRUE(basicTex);

    nvrhi::BufferHandle basicOutput = TestHelpers::createStructuredBufferUAV(mpDevice, elementCount * sizeof(float4), sizeof(float4), "BasicOutput");
    ASSERT_TRUE(basicOutput);

    nvrhi::TextureDesc rwDesc = nvrhi::TextureDesc()
                                    .setDimension(nvrhi::TextureDimension::Texture2D)
                                    .setWidth(dispatchWidth)
                                    .setHeight(dispatchHeight)
                                    .setMipLevels(1)
                                    .setArraySize(1)
                                    .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                    .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                    .setKeepInitialState(true)
                                    .setIsUAV(true)
                                    .setDebugName("RWTextureOutput");
    nvrhi::TextureHandle rwTexture = mpDevice->getDevice()->createTexture(rwDesc);
    ASSERT_TRUE(rwTexture);

    std::vector<float> blockValues = {0.25f, 0.5f, 0.75f, 1.0f};
    nvrhi::BufferHandle blockValueBuffer =
        TestHelpers::createStructuredBufferSRV(mpDevice, blockValues.data(), blockValues.size() * sizeof(float), sizeof(float), "BlockValues");
    ASSERT_TRUE(blockValueBuffer);

    std::vector<float4> blockTexels = {
        {0.10f, 0.20f, 0.30f, 1.0f},
        {0.11f, 0.22f, 0.33f, 1.0f},
        {0.12f, 0.24f, 0.36f, 1.0f},
        {0.13f, 0.26f, 0.39f, 1.0f},
    };
    nvrhi::TextureHandle blockTex = TestHelpers::createFloat4Texture1D(mpDevice, blockTexels.data(), elementCount, "BlockTex");
    ASSERT_TRUE(blockTex);

    nvrhi::BufferHandle blockOutput = TestHelpers::createStructuredBufferUAV(mpDevice, elementCount * sizeof(float4), sizeof(float4), "BlockOutput");
    ASSERT_TRUE(blockOutput);

    constexpr uint32_t kBindlessSlotCount = 8; // must match kBindlessSlotCount in SlangTest.slang
    constexpr uint32_t kWrittenCount = 4;
    std::vector<nvrhi::TextureHandle> writtenTextures;
    std::vector<float4> writtenColors;
    writtenTextures.reserve(kWrittenCount);
    writtenColors.reserve(kWrittenCount);
    for (uint32_t i = 0; i < kWrittenCount; ++i)
    {
        float4 color{0.1f * static_cast<float>(i + 1), 0.2f * static_cast<float>(i + 1), 0.3f * static_cast<float>(i + 1), 1.0f};
        writtenColors.push_back(color);
        auto t = TestHelpers::createFloat4Texture1D(mpDevice, &color, 1, "BindlessSlot");
        ASSERT_TRUE(t);
        writtenTextures.push_back(t);
    }

    const float4 kDefaultColor{0.77f, 0.88f, 0.99f, 1.0f};
    nvrhi::TextureHandle defaultTexture = TestHelpers::createFloat4Texture1D(mpDevice, &kDefaultColor, 1, "BindlessDefault");
    ASSERT_TRUE(defaultTexture);

    nvrhi::BufferHandle bindlessOutput =
        TestHelpers::createStructuredBufferUAV(mpDevice, kBindlessSlotCount * sizeof(float4), sizeof(float4), "BindlessOutput");
    ASSERT_TRUE(bindlessOutput);

    ref<Pass> pass = make_ref<ComputePass>(mpDevice, "/tests/SlangTest.slang", "everythingMain");
    (*pass)["gInputBuffer"] = structuredBuffer;
    (*pass)["gInputTexture"] = basicTex;
    (*pass)["gConstants"] = constantBuffer;
    (*pass)["gOutputBuffer"] = basicOutput;
    (*pass)["gRWTextureOutput"] = rwTexture;
    (*pass)["gBlockParams.values"] = blockValueBuffer;
    (*pass)["gBlockParams.tex"] = blockTex;
    (*pass)["gBlockOutput"] = blockOutput;
    pass->setDescriptorTable("gBindless.textures", writtenTextures, defaultTexture);
    (*pass)["gBindlessOutput"] = bindlessOutput;

    auto checkBasicSubtest = [&](const ConstantBufferData& c, const char* label)
    {
        auto bytes = TestHelpers::readbackBuffer(mpDevice, basicOutput, elementCount * sizeof(float4));
        ASSERT_EQ(bytes.size(), elementCount * sizeof(float4));
        const float4* results = reinterpret_cast<const float4*>(bytes.data());
        const float acc = c.scalarValue + static_cast<float>(c.integerValue) + (c.getFlag() ? 1.0f : 0.0f) + c.vectorValue[2];
        for (uint32_t i = 0; i < elementCount; ++i)
        {
            SCOPED_TRACE(std::string("Subtest A (Basic, ") + label + ") i=" + std::to_string(i));
            EXPECT_FLOAT_EQ(results[i].x, structuredBufferData[i]);
            EXPECT_FLOAT_EQ(results[i].y, textureData[i].y);
            EXPECT_FLOAT_EQ(results[i].z, acc);
            EXPECT_FLOAT_EQ(results[i].w, c.vectorValue[1]);
        }
    };

    pass->execute(dispatchWidth, dispatchHeight, 1);
    checkBasicSubtest(constantsTrue, "constantsTrue");

    std::vector<float4> rwPixels(dispatchWidth * dispatchHeight);
    ASSERT_TRUE(ResourceIO::readbackTexture(mpDevice, rwTexture, rwPixels.data(), rwPixels.size() * sizeof(float4)));
    for (uint32_t y = 0; y < dispatchHeight; ++y)
    {
        for (uint32_t x = 0; x < dispatchWidth; ++x)
        {
            SCOPED_TRACE("Subtest B (RWTexture) (" + std::to_string(x) + "," + std::to_string(y) + ")");
            const float4& px = rwPixels[y * dispatchWidth + x];
            EXPECT_FLOAT_EQ(px.x, static_cast<float>(x) * 0.125f);
            EXPECT_FLOAT_EQ(px.y, static_cast<float>(y) * 0.125f);
            EXPECT_FLOAT_EQ(px.z, 0.25f);
            EXPECT_FLOAT_EQ(px.w, 1.0f);
        }
    }

    auto blockBytes = TestHelpers::readbackBuffer(mpDevice, blockOutput, elementCount * sizeof(float4));
    ASSERT_EQ(blockBytes.size(), elementCount * sizeof(float4));
    const float4* blockResults = reinterpret_cast<const float4*>(blockBytes.data());
    for (uint32_t i = 0; i < elementCount; ++i)
    {
        SCOPED_TRACE("Subtest C (ParameterBlock) i=" + std::to_string(i));
        EXPECT_FLOAT_EQ(blockResults[i].x, blockValues[i]);
        EXPECT_FLOAT_EQ(blockResults[i].y, blockTexels[i].x);
        EXPECT_FLOAT_EQ(blockResults[i].z, blockTexels[i].y);
        EXPECT_FLOAT_EQ(blockResults[i].w, blockTexels[i].z);
    }

    auto bindlessBytes = TestHelpers::readbackBuffer(mpDevice, bindlessOutput, kBindlessSlotCount * sizeof(float4));
    ASSERT_EQ(bindlessBytes.size(), kBindlessSlotCount * sizeof(float4));
    const float4* bindlessResults = reinterpret_cast<const float4*>(bindlessBytes.data());
    for (uint32_t i = 0; i < kWrittenCount; ++i)
    {
        SCOPED_TRACE("Subtest D (Bindless written) slot=" + std::to_string(i));
        EXPECT_FLOAT_EQ(bindlessResults[i].x, writtenColors[i].x);
        EXPECT_FLOAT_EQ(bindlessResults[i].y, writtenColors[i].y);
        EXPECT_FLOAT_EQ(bindlessResults[i].z, writtenColors[i].z);
        EXPECT_FLOAT_EQ(bindlessResults[i].w, writtenColors[i].w);
    }
    for (uint32_t i = kWrittenCount; i < kBindlessSlotCount; ++i)
    {
        SCOPED_TRACE("Subtest D (Bindless default) slot=" + std::to_string(i));
        EXPECT_FLOAT_EQ(bindlessResults[i].x, kDefaultColor.x);
        EXPECT_FLOAT_EQ(bindlessResults[i].y, kDefaultColor.y);
        EXPECT_FLOAT_EQ(bindlessResults[i].z, kDefaultColor.z);
        EXPECT_FLOAT_EQ(bindlessResults[i].w, kDefaultColor.w);
    }

    // Re-dispatch with a different flag/scalar to catch 16-byte padding bugs in the constant buffer.
    ConstantBufferData constantsFalse = constantsTrue;
    constantsFalse.scalarValue = 1.25f;
    constantsFalse.setFlag(false);
    ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, constantBuffer, &constantsFalse, sizeof(constantsFalse)));
    pass->execute(dispatchWidth, dispatchHeight, 1);
    checkBasicSubtest(constantsFalse, "constantsFalse");
}
