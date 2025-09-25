#include "TextureAverage.h"
#include "Utils/ResourceIO.h"
#include "Utils/Math/Math.h"
#include "Utils/Logger.h"

#include <cstring>
#include <vector>

namespace
{
struct TextureAveragePassRegistration
{
    TextureAveragePassRegistration()
    {
        RenderPassRegistry::registerPass(
            RenderPassDescriptor{
                "TextureAverage",
                "Computes an average texture over time for debugging and statistics collection.",
                [](ref<Device> pDevice) { return make_ref<TextureAverage>(pDevice); }
            }
        );
    }
};

[[maybe_unused]] static TextureAveragePassRegistration gTextureAveragePassRegistration;

const std::string kInputName = "input";
constexpr uint32_t kTileWidth = 16;
constexpr uint32_t kTileHeight = 16;
} // namespace

TextureAverage::TextureAverage(ref<Device> pDevice) : RenderPass(pDevice)
{
    nvrhi::BufferDesc cbDesc;
    cbDesc.byteSize = sizeof(PerFrameCB);
    cbDesc.isConstantBuffer = true;
    cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    cbDesc.keepInitialState = true;
    cbDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    cbDesc.isVolatile = true;
    cbDesc.debugName = "Utils/TextureAverage/PerFrameCB";
    mCbPerFrame = mpDevice->getDevice()->createBuffer(cbDesc);

    mpPass = make_ref<ComputePass>(pDevice, "/src/RenderPasses/Utils/TextureAverage/TextureAverage.slang", "main");
    mpPass->addConstantBuffer(mCbPerFrame, &mPerFrameData, sizeof(PerFrameCB));
    mAverageResult = float4(0.0f);
}

std::vector<RenderPassInput> TextureAverage::getInputs() const
{
    return {RenderPassInput(kInputName, RenderDataType::Texture2D)};
}

std::vector<RenderPassOutput> TextureAverage::getOutputs() const
{
    // This pass has no outputs, it only displays UI
    return {};
}

RenderData TextureAverage::execute(const RenderData& renderData)
{
    mpInputTexture = dynamic_cast<nvrhi::ITexture*>(renderData[kInputName].Get());

    if (!mpInputTexture)
    {
        LOG_WARN("TextureAverage: No input texture provided");
        mAverageResult = float4(0.0f);
        return RenderData();
    }

    // Get texture dimensions
    const auto& textureDesc = mpInputTexture->getDesc();
    mWidth = textureDesc.width;
    mHeight = textureDesc.height;

    if (mWidth == 0 || mHeight == 0)
    {
        mAverageResult = float4(0.0f);
        return RenderData();
    }

    // Update constant buffer with texture dimensions
    mPerFrameData.gWidth = mWidth;
    mPerFrameData.gHeight = mHeight;

    const uint32_t tilesX = (mWidth + kTileWidth - 1) / kTileWidth;
    const uint32_t tilesY = (mHeight + kTileHeight - 1) / kTileHeight;
    const size_t tileCount = static_cast<size_t>(tilesX) * static_cast<size_t>(tilesY);
    const size_t requiredBytes = tileCount * sizeof(float4);

    if (!mResultBuffer || mResultBufferSize != requiredBytes)
    {
        nvrhi::BufferDesc resultDesc;
        resultDesc.byteSize = requiredBytes;
        resultDesc.structStride = sizeof(float4);
        resultDesc.canHaveUAVs = true;
        resultDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
        resultDesc.keepInitialState = true;
        resultDesc.cpuAccess = nvrhi::CpuAccessMode::None;
        resultDesc.debugName = "Utils/TextureAverage/AverageResult";
        mResultBuffer = mpDevice->getDevice()->createBuffer(resultDesc);
        mResultBufferSize = mResultBuffer ? requiredBytes : 0;
    }

    // Bind resources to compute pass
    (*mpPass)["PerFrameCB"] = mCbPerFrame;
    (*mpPass)["inputTexture"] = mpInputTexture;
    (*mpPass)["resultBuffer"] = mResultBuffer;

    // Execute compute pass with one thread per tile
    mpPass->execute(tilesX, tilesY, 1);

    // Read back the result from GPU
    std::vector<uint8_t> resultData(requiredBytes);
    if (!mResultBuffer || requiredBytes == 0 || !ResourceIO::readbackBuffer(mpDevice, mResultBuffer, resultData.data(), requiredBytes))
    {
        mAverageResult = float4(0.0f);
        return RenderData();
    }

    float4 totalSum(0.0f);
    const float* partialSums = reinterpret_cast<const float*>(resultData.data());
    for (size_t tileIndex = 0; tileIndex < tileCount; ++tileIndex)
    {
        const size_t base = tileIndex * 4;
        totalSum += float4(partialSums[base + 0], partialSums[base + 1], partialSums[base + 2], partialSums[base + 3]);
    }

    const float totalPixels = static_cast<float>(static_cast<uint64_t>(mWidth) * static_cast<uint64_t>(mHeight));
    mAverageResult = totalPixels > 0.0f ? totalSum / totalPixels : float4(0.0f);
    return RenderData();
}

void TextureAverage::renderUI()
{
    GUI::Text("Averages:");
    GUI::Text("(%.5f, %.5f, %.5f, %.5f)", mAverageResult.r, mAverageResult.g, mAverageResult.b, mAverageResult.a);
}
