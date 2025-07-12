#include "PathTracingPass.h"
#include "Utils/GUI.h"

PathTracingPass::PathTracingPass(ref<Device> device) : RenderPass(device)
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

    std::unordered_map<std::string, nvrhi::ShaderType> entryPoints = {
        {"rayGenMain", nvrhi::ShaderType::RayGeneration}, {"missMain", nvrhi::ShaderType::Miss}, {"closestHitMain", nvrhi::ShaderType::ClosestHit}
    };
    pass = make_ref<RayTracingPass>(device, "/src/RenderPasses/PathTracingPass/PathTracing.slang", entryPoints);
}

RenderData PathTracingPass::execute(const RenderData& input)
{
    perFrameData.gWidth = width;
    perFrameData.gHeight = height;
    perFrameData.maxDepth = maxDepth;
    perFrameData.frameCount = frameCount++;
    perFrameData.gColor = gColorSlider;

    RenderData output;
    output.setResource("output", textureOut);
    cbPerFrame.updateData(m_Device->getDevice(), &perFrameData, sizeof(PerFrameCB));

    (*pass)["PerFrameCB"] = cbPerFrame.getHandle();
    (*pass)["gCamera"] = input["gCamera"];
    (*pass)["result"] = textureOut;
    (*pass)["gScene.vertices"] = input["gScene.vertices"];
    (*pass)["gScene.indices"] = input["gScene.indices"];
    (*pass)["gScene.rtAccel"] = input["gScene.rtAccel"];
    pass->execute(width, height, 1);
    return output;
}

void PathTracingPass::renderUI()
{
    GUI::SliderFloat("gColor", &gColorSlider, 0.0f, 1.0f);
}
