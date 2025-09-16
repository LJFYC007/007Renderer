#include "PathTracingPass.h"

PathTracingPass::PathTracingPass(ref<Device> pDevice) : RenderPass(pDevice)
{
    mCbPerFrame.initialize(pDevice, nullptr, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB");
    mCbCamera.initialize(pDevice, nullptr, sizeof(CameraData), nvrhi::ResourceStates::ConstantBuffer, false, true, "Camera");

    std::unordered_map<std::string, nvrhi::ShaderType> entryPoints = {
        {"rayGenMain", nvrhi::ShaderType::RayGeneration}, {"missMain", nvrhi::ShaderType::Miss}, {"closestHitMain", nvrhi::ShaderType::ClosestHit}
    };
    mpPass = make_ref<RayTracingPass>(pDevice, "/src/RenderPasses/PathTracingPass/PathTracing.slang", entryPoints);
}

RenderData PathTracingPass::execute(const RenderData& input)
{
    uint2 resolution = uint2(mpScene->camera->getCameraData().frameWidth, mpScene->camera->getCameraData().frameHeight);
    if (resolution.x != mWidth || resolution.y != mHeight)
    {
        mWidth = resolution.x;
        mHeight = resolution.y;
        prepareResources();
    }

    mPerFrameData.gWidth = mWidth;
    mPerFrameData.gHeight = mHeight;
    mPerFrameData.maxDepth = mMaxDepth;
    mPerFrameData.frameCount = ++mFrameCount;
    mPerFrameData.gColor = mGColorSlider;

    RenderData output;
    output.setResource("output", mTextureOut);
    mCbPerFrame.updateData(mpDevice, &mPerFrameData, sizeof(PerFrameCB));
    mCbCamera.updateData(mpDevice, &mpScene->camera->getCameraData(), sizeof(CameraData));

    (*mpPass)["PerFrameCB"] = mCbPerFrame.getHandle();
    (*mpPass)["gCamera"] = mCbCamera.getHandle();
    (*mpPass)["gScene.vertices"] = mpScene->getVertexBuffer();
    (*mpPass)["gScene.indices"] = mpScene->getIndexBuffer();
    (*mpPass)["gScene.meshes"] = mpScene->getMeshBuffer();
    (*mpPass)["gScene.triangleToMesh"] = mpScene->getTriangleToMeshBuffer();
    (*mpPass)["gScene.materials"] = mpScene->getMaterialBuffer();
    (*mpPass)["gScene.rtAccel"] = mpScene->getTLAS();
    (*mpPass)["result"] = mTextureOut;
    mpPass->execute(mWidth, mHeight, 1);
    return output;
}

void PathTracingPass::renderUI()
{
    GUI::SliderFloat("gColor", &mGColorSlider, 0.0f, 5.0f);
}

void PathTracingPass::prepareResources()
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
}