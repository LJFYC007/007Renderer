#include "AccumulatePass.h"
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
} // namespace

AccumulatePass::AccumulatePass(ref<Device> pDevice) : RenderPass(pDevice)
{
    nvrhi::BufferDesc cbDesc;
    cbDesc.byteSize = sizeof(PerFrameCB);
    cbDesc.isConstantBuffer = true;
    cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    cbDesc.keepInitialState = true;
    cbDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    cbDesc.debugName = "AccumulatePass/PerFrameCB";
    mCbPerFrame = mpDevice->getDevice()->createBuffer(cbDesc);
    mCbPerFrameSize = mCbPerFrame ? cbDesc.byteSize : 0;
    if (mCbPerFrame && mCbPerFrameSize > 0)
        ResourceIO::uploadBuffer(mpDevice, mCbPerFrame, &mPerFrameData, mCbPerFrameSize);

    mpPass = make_ref<ComputePass>(pDevice, "/src/RenderPasses/AccumulatePass/AccumulatePass.slang", "main");
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
    }

    // Check for GUI refresh flags
    if (GUI::getAndClearRefreshFlags() != RenderPassRefreshFlags::None)
        mReset = true;

    mPerFrameData.gWidth = mWidth;
    mPerFrameData.gHeight = mHeight;
    mPerFrameData.reset = mReset;
    if (mReset)
    {
        mFrameCount = 0;
        mReset = false;
    }
    mPerFrameData.frameCount = ++mFrameCount;

    RenderData output;
    output.setResource(kOutputName, mTextureOut);
    ResourceIO::uploadBuffer(mpDevice, mCbPerFrame, &mPerFrameData, sizeof(PerFrameCB));

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
