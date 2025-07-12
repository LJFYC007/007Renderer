#include "AccumulatePass.h"
#include "Utils/Math/Math.h"

AccumulatePass::AccumulatePass(ref<Device> device) : RenderPass(device)
{
    cbPerFrame.initialize(device->getDevice(), &perFrameData, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB");
    pass = make_ref<ComputePass>(device, "/src/RenderPasses/AccumulatePass/AccumulatePass.slang", "main");
}

RenderData AccumulatePass::execute(const RenderData& input)
{
    nvrhi::ITexture* texture = dynamic_cast<nvrhi::ITexture*>(input["output"].Get());
    nvrhi::TextureHandle inputTexture = texture;
    uint2 resolution = uint2(inputTexture->getDesc().width, inputTexture->getDesc().height);
    if (resolution.x != width || resolution.y != height)
    {
        width = resolution.x;
        height = resolution.y;
        prepareResources();
    }

    perFrameData.gWidth = width;
    perFrameData.gHeight = height;
    perFrameData.frameCount = ++frameCount;
    perFrameData.reset = false;
    if (reset)
    {
        frameCount = 0;
        reset = false;
        perFrameData.reset = true;
    }

    RenderData output;
    output.setResource("output", textureOut);
    cbPerFrame.updateData(m_Device->getDevice(), &perFrameData, sizeof(PerFrameCB));
    (*pass)["PerFrameCB"] = cbPerFrame.getHandle();
    (*pass)["input"] = inputTexture;
    (*pass)["accumulateTexture"] = accumulateTexture;
    (*pass)["output"] = textureOut;
    pass->execute(width, height, 1);
    return output;
}

void AccumulatePass::renderUI()
{
    GUI::Checkbox("Reset Accumulation", &reset);
}

void AccumulatePass::prepareResources()
{
    nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
                                         .setWidth(width)
                                         .setHeight(height)
                                         .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                         .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                         .setDebugName("output")
                                         .setIsUAV(true);
    textureOut = m_Device->getDevice()->createTexture(textureDesc);
    textureDesc.setDebugName("accumulateTexture");
    accumulateTexture = m_Device->getDevice()->createTexture(textureDesc);
}
