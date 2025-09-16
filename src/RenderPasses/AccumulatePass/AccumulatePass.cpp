#include "AccumulatePass.h"
#include "Utils/Math/Math.h"

AccumulatePass::AccumulatePass(ref<Device> pDevice) : RenderPass(pDevice)
{
    mCbPerFrame.initialize(pDevice, &mPerFrameData, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB");
    mpPass = make_ref<ComputePass>(pDevice, "/src/RenderPasses/AccumulatePass/AccumulatePass.slang", "main");
}

RenderData AccumulatePass::execute(const RenderData& input)
{
    nvrhi::TextureHandle pInputTexture = dynamic_cast<nvrhi::ITexture*>(input["output"].Get());
    uint2 resolution = uint2(pInputTexture->getDesc().width, pInputTexture->getDesc().height);
    if (resolution.x != mWidth || resolution.y != mHeight)
    {
        mWidth = resolution.x;
        mHeight = resolution.y;
        prepareResources();
    }

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
    output.setResource("output", mTextureOut);
    mCbPerFrame.updateData(mpDevice, &mPerFrameData, sizeof(PerFrameCB));
    (*mpPass)["PerFrameCB"] = mCbPerFrame.getHandle();
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
                                         .setDebugName("output")
                                         .setIsUAV(true)
                                         .setKeepInitialState(true);
    mTextureOut = mpDevice->getDevice()->createTexture(textureDesc);
    textureDesc.setDebugName("accumulateTexture");
    mAccumulateTexture = mpDevice->getDevice()->createTexture(textureDesc);
}
