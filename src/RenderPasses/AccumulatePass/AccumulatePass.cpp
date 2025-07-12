#include "AccumulatePass.h"

AccumulatePass::AccumulatePass(ref<Device> device) : RenderPass(device)
{
    cbPerFrame.initialize(device->getDevice(), &perFrameData, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB");

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

    pass = make_ref<ComputePass>(device, "/src/RenderPasses/AccumulatePass/AccumulatePass.slang", "main");
}

RenderData AccumulatePass::execute(const RenderData& input)
{
    perFrameData.gWidth = width;
    perFrameData.gHeight = height;
    perFrameData.frameCount = frameCount++;

    RenderData output;
    output.setResource("output", textureOut);
    cbPerFrame.updateData(m_Device->getDevice(), &perFrameData, sizeof(PerFrameCB));
    (*pass)["PerFrameCB"] = cbPerFrame.getHandle();
    (*pass)["input"] = input["output"];
    (*pass)["accumulateTexture"] = accumulateTexture;
    (*pass)["output"] = textureOut;
    pass->execute(width, height, 1);
    return output;
}

void AccumulatePass::renderUI() {}
