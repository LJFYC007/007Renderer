#include "Accumulate.h"
#include "Utils/ResourceIO.h"
#include "Utils/Math/Math.h"

namespace
{
struct AccumulatePassRegistration
{
    AccumulatePassRegistration()
    {
        RenderPassRegistry::registerPass(
            RenderPassDescriptor{
                "Accumulate",
                "Accumulates successive frames to smooth noise and handles reset logic when parameters change.",
                [](ref<Device> pDevice) { return make_ref<AccumulatePass>(pDevice); }
            }
        );
    }
};

[[maybe_unused]] static AccumulatePassRegistration gAccumulatePassRegistration;

const std::string kInputName = "input";
const std::string kOutputName = "output";
constexpr int kMaxSppSliderMax = 8192;
} // namespace

AccumulatePass::AccumulatePass(ref<Device> pDevice) : RenderPass(pDevice)
{
    nvrhi::BufferDesc cbDesc;
    cbDesc.byteSize = sizeof(PerFrameCB);
    cbDesc.isConstantBuffer = true;
    cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    cbDesc.keepInitialState = true;
    cbDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    cbDesc.isVolatile = true;
    cbDesc.debugName = "AccumulatePass/PerFrameCB";
    mCbPerFrame = mpDevice->getDevice()->createBuffer(cbDesc);

    mpPass = make_ref<ComputePass>(pDevice, "/src/RenderPasses/AccumulatePass/Accumulate.slang", "main");
    mpPass->addConstantBuffer(mCbPerFrame, &mPerFrameData, sizeof(PerFrameCB));
}

std::vector<RenderPassInput> AccumulatePass::getInputs() const
{
    return {RenderPassInput(kInputName, RenderDataType::Texture2D)};
}

std::vector<RenderPassOutput> AccumulatePass::getOutputs() const
{
    return {RenderPassOutput(kOutputName, RenderDataType::Texture2D)};
}

RenderData AccumulatePass::execute(const RenderData& renderData)
{
    nvrhi::TextureHandle pInputTexture = dynamic_cast<nvrhi::ITexture*>(renderData[kInputName].Get());
    uint2 resolution = uint2(pInputTexture->getDesc().width, pInputTexture->getDesc().height);
    if (resolution.x != mWidth || resolution.y != mHeight)
    {
        mWidth = resolution.x;
        mHeight = resolution.y;
        prepareResources();
        // Fresh accumulateTexture contents are undefined; the shader's reset branch zero-clears it.
        mReset = true;
    }

    if (hasFlag(GUI::getRefreshFlags(), RenderPassRefreshFlags::ResetAccumulation))
        mReset = true;

    RenderData output;
    output.setResource(kOutputName, mTextureOut);

    if (mMaxSpp > 0 && mFrameCount >= mMaxSpp && !mReset)
        return output;

    mPerFrameData.gWidth = mWidth;
    mPerFrameData.gHeight = mHeight;
    mPerFrameData.reset = mReset;
    if (mReset)
    {
        mFrameCount = 0;
        mReset = false;
    }
    mPerFrameData.frameCount = ++mFrameCount;

    (*mpPass)["PerFrameCB"] = mCbPerFrame;
    (*mpPass)["input"] = pInputTexture;
    (*mpPass)["accumulateTexture"] = mAccumulateTexture;
    (*mpPass)["output"] = mTextureOut;
    mpPass->execute(mWidth, mHeight, 1);
    return output;
}

void AccumulatePass::renderUI()
{
    if (GUI::Button("Reset Accumulation"))
        mReset = true;
    int maxSpp = static_cast<int>(mMaxSpp);
    if (GUI::SliderInt("Max SPP (0 = unlimited)", &maxSpp, 0, kMaxSppSliderMax))
        mMaxSpp = static_cast<uint32_t>(maxSpp);
}

void AccumulatePass::prepareResources()
{
    nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
                                         .setWidth(mWidth)
                                         .setHeight(mHeight)
                                         .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                         .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                         .setDebugName("AccumulatePass/output")
                                         .setIsUAV(true)
                                         .setKeepInitialState(true);
    mTextureOut = mpDevice->getDevice()->createTexture(textureDesc);
    textureDesc.setDebugName("AccumulatePass/accumulateTexture");
    mAccumulateTexture = mpDevice->getDevice()->createTexture(textureDesc);
}
