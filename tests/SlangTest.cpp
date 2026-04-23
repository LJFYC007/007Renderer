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

constexpr uint32_t kElementCount = 4;
constexpr uint32_t kDispatchWidth = 8;
constexpr uint32_t kDispatchHeight = 8;
constexpr uint32_t kBindlessSlotCount = 8; // must match kBindlessSlotCount in SlangTest.slang
constexpr uint32_t kBindlessWrittenCount = 4;

const float4 kDefaultBindlessColor{0.77f, 0.88f, 0.99f, 1.0f};
} // namespace

// One shader (`everythingMain`) exercises constant buffers, RWTexture2D writes,
// ParameterBlocks, and bindless descriptor tables in a single dispatch. Each TEST_F
// re-runs SetUp() (so resources + dispatch recur per subtest) and verifies *one*
// output, attributing failures to a specific feature. The Slang session cache keeps
// the per-TEST_F compile cost near-zero after the first.
class Slang : public DeviceTest
{
protected:
    std::vector<float> inputBufferData;
    std::vector<float4> inputTextureData;
    ConstantBufferData constants;

    std::vector<float> blockValues;
    std::vector<float4> blockTexels;

    std::vector<float4> bindlessWrittenColors;

    nvrhi::BufferHandle inputBuffer;
    nvrhi::BufferHandle constantBuffer;
    nvrhi::TextureHandle inputTexture;
    nvrhi::BufferHandle basicOutput;
    nvrhi::TextureHandle rwTexture;
    nvrhi::BufferHandle blockValueBuffer;
    nvrhi::TextureHandle blockTexture;
    nvrhi::BufferHandle blockOutput;
    nvrhi::BufferHandle bindlessOutput;

    ref<Pass> pass;

    void SetUp() override
    {
        DeviceTest::SetUp();
        if (IsSkipped() || HasFatalFailure())
            return;

        // --- Basic scalar/buffer/texture path (constant buffer + SRVs) ---
        inputBufferData = {1.25f, 2.5f, 3.75f, 5.0f};
        inputBuffer = TestHelpers::createStructuredBufferSRV(
            mpDevice, inputBufferData.data(), inputBufferData.size() * sizeof(float), sizeof(float), "BasicInputBuffer"
        );
        ASSERT_TRUE(inputBuffer);

        constants.scalarValue = 0.5f;
        constants.integerValue = 3;
        constants.setFlag(true);
        constants.vectorValue[0] = 0.125f;
        constants.vectorValue[1] = 0.25f;
        constants.vectorValue[2] = 0.75f;

        nvrhi::BufferDesc cbDesc;
        cbDesc.byteSize = sizeof(constants);
        cbDesc.isConstantBuffer = true;
        cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        cbDesc.keepInitialState = true;
        cbDesc.cpuAccess = nvrhi::CpuAccessMode::None;
        cbDesc.debugName = "BasicConstants";
        constantBuffer = mpDevice->getDevice()->createBuffer(cbDesc);
        ASSERT_TRUE(constantBuffer);
        ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, constantBuffer, &constants, sizeof(constants)));

        inputTextureData = {
            {0.1f, 0.6f, 0.2f, 0.7f},
            {0.2f, 0.5f, 0.3f, 0.8f},
            {0.3f, 0.4f, 0.4f, 0.9f},
            {0.4f, 0.3f, 0.5f, 1.0f},
        };
        inputTexture = TestHelpers::createFloat4Texture1D(mpDevice, inputTextureData.data(), kElementCount, "BasicTexture");
        ASSERT_TRUE(inputTexture);

        basicOutput = TestHelpers::createStructuredBufferUAV(mpDevice, kElementCount * sizeof(float4), sizeof(float4), "BasicOutput");
        ASSERT_TRUE(basicOutput);

        // --- RWTexture2D path ---
        nvrhi::TextureDesc rwDesc = nvrhi::TextureDesc()
                                        .setDimension(nvrhi::TextureDimension::Texture2D)
                                        .setWidth(kDispatchWidth)
                                        .setHeight(kDispatchHeight)
                                        .setMipLevels(1)
                                        .setArraySize(1)
                                        .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                        .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                        .setKeepInitialState(true)
                                        .setIsUAV(true)
                                        .setDebugName("RWTextureOutput");
        rwTexture = mpDevice->getDevice()->createTexture(rwDesc);
        ASSERT_TRUE(rwTexture);

        // --- ParameterBlock path ---
        blockValues = {0.25f, 0.5f, 0.75f, 1.0f};
        blockValueBuffer =
            TestHelpers::createStructuredBufferSRV(mpDevice, blockValues.data(), blockValues.size() * sizeof(float), sizeof(float), "BlockValues");
        ASSERT_TRUE(blockValueBuffer);

        blockTexels = {
            {0.10f, 0.20f, 0.30f, 1.0f},
            {0.11f, 0.22f, 0.33f, 1.0f},
            {0.12f, 0.24f, 0.36f, 1.0f},
            {0.13f, 0.26f, 0.39f, 1.0f},
        };
        blockTexture = TestHelpers::createFloat4Texture1D(mpDevice, blockTexels.data(), kElementCount, "BlockTex");
        ASSERT_TRUE(blockTexture);

        blockOutput = TestHelpers::createStructuredBufferUAV(mpDevice, kElementCount * sizeof(float4), sizeof(float4), "BlockOutput");
        ASSERT_TRUE(blockOutput);

        // --- Bindless descriptor-table path ---
        std::vector<nvrhi::TextureHandle> writtenTextures;
        bindlessWrittenColors.reserve(kBindlessWrittenCount);
        writtenTextures.reserve(kBindlessWrittenCount);
        for (uint32_t i = 0; i < kBindlessWrittenCount; ++i)
        {
            float4 color{0.1f * (i + 1), 0.2f * (i + 1), 0.3f * (i + 1), 1.0f};
            bindlessWrittenColors.push_back(color);
            auto t = TestHelpers::createFloat4Texture1D(mpDevice, &color, 1, "BindlessSlot");
            ASSERT_TRUE(t);
            writtenTextures.push_back(t);
        }
        nvrhi::TextureHandle defaultTexture = TestHelpers::createFloat4Texture1D(mpDevice, &kDefaultBindlessColor, 1, "BindlessDefault");
        ASSERT_TRUE(defaultTexture);

        bindlessOutput = TestHelpers::createStructuredBufferUAV(mpDevice, kBindlessSlotCount * sizeof(float4), sizeof(float4), "BindlessOutput");
        ASSERT_TRUE(bindlessOutput);

        // --- Bind everything and dispatch once ---
        pass = make_ref<ComputePass>(mpDevice, "/tests/SlangTest.slang", "everythingMain");
        (*pass)["gInputBuffer"] = inputBuffer;
        (*pass)["gInputTexture"] = inputTexture;
        (*pass)["gConstants"] = constantBuffer;
        (*pass)["gOutputBuffer"] = basicOutput;
        (*pass)["gRWTextureOutput"] = rwTexture;
        (*pass)["gBlockParams.values"] = blockValueBuffer;
        (*pass)["gBlockParams.tex"] = blockTexture;
        (*pass)["gBlockOutput"] = blockOutput;
        pass->setDescriptorTable("gBindless.textures", writtenTextures, defaultTexture);
        (*pass)["gBindlessOutput"] = bindlessOutput;

        pass->execute(kDispatchWidth, kDispatchHeight, 1);
    }

    // Shared helper: verify the basic output matches a given constant-buffer state.
    void verifyBasicOutput(const ConstantBufferData& c, const char* label)
    {
        auto bytes = TestHelpers::readbackBuffer(mpDevice, basicOutput, kElementCount * sizeof(float4));
        ASSERT_EQ(bytes.size(), kElementCount * sizeof(float4));
        const float4* results = reinterpret_cast<const float4*>(bytes.data());
        const float acc = c.scalarValue + static_cast<float>(c.integerValue) + (c.getFlag() ? 1.0f : 0.0f) + c.vectorValue[2];
        for (uint32_t i = 0; i < kElementCount; ++i)
        {
            SCOPED_TRACE(std::string(label) + " i=" + std::to_string(i));
            EXPECT_FLOAT_EQ(results[i].x, inputBufferData[i]);
            EXPECT_FLOAT_EQ(results[i].y, inputTextureData[i].y);
            EXPECT_FLOAT_EQ(results[i].z, acc);
            EXPECT_FLOAT_EQ(results[i].w, c.vectorValue[1]);
        }
    }
};

// Constant buffer + SRV read-back round trip. Re-dispatches with a second constant-buffer
// payload to catch 16-byte padding bugs in the CB layout.
TEST_F(Slang, CBuffer)
{
    verifyBasicOutput(constants, "constantsTrue");

    ConstantBufferData constantsFalse = constants;
    constantsFalse.scalarValue = 1.25f;
    constantsFalse.setFlag(false);
    ASSERT_TRUE(ResourceIO::uploadBuffer(mpDevice, constantBuffer, &constantsFalse, sizeof(constantsFalse)));
    pass->execute(kDispatchWidth, kDispatchHeight, 1);
    verifyBasicOutput(constantsFalse, "constantsFalse");
}

// RWTexture2D write path.
TEST_F(Slang, RWTexture)
{
    std::vector<float4> pixels(kDispatchWidth * kDispatchHeight);
    ASSERT_TRUE(ResourceIO::readbackTexture(mpDevice, rwTexture, pixels.data(), pixels.size() * sizeof(float4)));
    for (uint32_t y = 0; y < kDispatchHeight; ++y)
    {
        for (uint32_t x = 0; x < kDispatchWidth; ++x)
        {
            SCOPED_TRACE("(" + std::to_string(x) + "," + std::to_string(y) + ")");
            const float4& px = pixels[y * kDispatchWidth + x];
            EXPECT_FLOAT_EQ(px.x, static_cast<float>(x) * 0.125f);
            EXPECT_FLOAT_EQ(px.y, static_cast<float>(y) * 0.125f);
            EXPECT_FLOAT_EQ(px.z, 0.25f);
            EXPECT_FLOAT_EQ(px.w, 1.0f);
        }
    }
}

// ParameterBlock nested-resource binding.
TEST_F(Slang, ParamBlock)
{
    auto bytes = TestHelpers::readbackBuffer(mpDevice, blockOutput, kElementCount * sizeof(float4));
    ASSERT_EQ(bytes.size(), kElementCount * sizeof(float4));
    const float4* results = reinterpret_cast<const float4*>(bytes.data());
    for (uint32_t i = 0; i < kElementCount; ++i)
    {
        SCOPED_TRACE("i=" + std::to_string(i));
        EXPECT_FLOAT_EQ(results[i].x, blockValues[i]);
        EXPECT_FLOAT_EQ(results[i].y, blockTexels[i].x);
        EXPECT_FLOAT_EQ(results[i].z, blockTexels[i].y);
        EXPECT_FLOAT_EQ(results[i].w, blockTexels[i].z);
    }
}

// Bindless descriptor-table read — written slots + default-fill for unused slots.
TEST_F(Slang, Bindless)
{
    auto bytes = TestHelpers::readbackBuffer(mpDevice, bindlessOutput, kBindlessSlotCount * sizeof(float4));
    ASSERT_EQ(bytes.size(), kBindlessSlotCount * sizeof(float4));
    const float4* results = reinterpret_cast<const float4*>(bytes.data());

    for (uint32_t i = 0; i < kBindlessWrittenCount; ++i)
    {
        SCOPED_TRACE("written slot=" + std::to_string(i));
        EXPECT_FLOAT_EQ(results[i].x, bindlessWrittenColors[i].x);
        EXPECT_FLOAT_EQ(results[i].y, bindlessWrittenColors[i].y);
        EXPECT_FLOAT_EQ(results[i].z, bindlessWrittenColors[i].z);
        EXPECT_FLOAT_EQ(results[i].w, bindlessWrittenColors[i].w);
    }
    for (uint32_t i = kBindlessWrittenCount; i < kBindlessSlotCount; ++i)
    {
        SCOPED_TRACE("default slot=" + std::to_string(i));
        EXPECT_FLOAT_EQ(results[i].x, kDefaultBindlessColor.x);
        EXPECT_FLOAT_EQ(results[i].y, kDefaultBindlessColor.y);
        EXPECT_FLOAT_EQ(results[i].z, kDefaultBindlessColor.z);
        EXPECT_FLOAT_EQ(results[i].w, kDefaultBindlessColor.w);
    }
}
