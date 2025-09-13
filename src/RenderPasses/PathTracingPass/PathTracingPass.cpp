#include "PathTracingPass.h"

PathTracingPass::PathTracingPass(ref<Device> device) : RenderPass(device)
{
    cbPerFrame.initialize(device, nullptr, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB");
    cbCamera.initialize(device, nullptr, sizeof(CameraData), nvrhi::ResourceStates::ConstantBuffer, false, true, "Camera");

    std::unordered_map<std::string, nvrhi::ShaderType> entryPoints = {
        {"rayGenMain", nvrhi::ShaderType::RayGeneration}, {"missMain", nvrhi::ShaderType::Miss}, {"closestHitMain", nvrhi::ShaderType::ClosestHit}
    };
    pass = make_ref<RayTracingPass>(device, "/src/RenderPasses/PathTracingPass/PathTracing.slang", entryPoints);
}

RenderData PathTracingPass::execute(const RenderData& input)
{
    uint2 resolution = uint2(m_Scene->camera->getCameraData().frameWidth, m_Scene->camera->getCameraData().frameHeight);
    if (resolution.x != width || resolution.y != height)
    {
        width = resolution.x;
        height = resolution.y;
        prepareResources();
    }

    perFrameData.gWidth = width;
    perFrameData.gHeight = height;
    perFrameData.maxDepth = maxDepth;
    perFrameData.frameCount = ++frameCount;
    perFrameData.gColor = gColorSlider;

    RenderData output;
    output.setResource("output", textureOut);
    cbPerFrame.updateData(m_Device, &perFrameData, sizeof(PerFrameCB));
    cbCamera.updateData(m_Device, &m_Scene->camera->getCameraData(), sizeof(CameraData));

    (*pass)["PerFrameCB"] = cbPerFrame.getHandle();
    (*pass)["gCamera"] = cbCamera.getHandle();
    (*pass)["gScene.vertices"] = m_Scene->getVertexBuffer();
    (*pass)["gScene.indices"] = m_Scene->getIndexBuffer();
    (*pass)["gScene.meshes"] = m_Scene->getMeshBuffer();
    (*pass)["gScene.triangleToMesh"] = m_Scene->getTriangleToMeshBuffer();
    (*pass)["gScene.materials"] = m_Scene->getMaterialBuffer();
    (*pass)["gScene.rtAccel"] = m_Scene->getTLAS();
    (*pass)["result"] = textureOut;
    pass->execute(width, height, 1);
    return output;
}

void PathTracingPass::renderUI()
{
    GUI::SliderFloat("gColor", &gColorSlider, 0.0f, 5.0f);
}

void PathTracingPass::prepareResources()
{
    nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
                                         .setWidth(width)
                                         .setHeight(height)
                                         .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                         .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                         .setDebugName("output")
                                         .setIsUAV(true);
    textureOut = m_Device->getDevice()->createTexture(textureDesc);
}