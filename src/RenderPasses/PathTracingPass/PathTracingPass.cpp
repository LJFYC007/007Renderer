#include "PathTracingPass.h"
#include "Utils/ResourceIO.h"

namespace
{
struct PathTracingPassRegistration
{
    PathTracingPassRegistration()
    {
        RenderPassRegistry::registerPass(
            RenderPassDescriptor{
                "PathTracing",
                "Physically-based path tracing integrator that produces the primary color output.",
                [](ref<Device> pDevice) { return make_ref<PathTracingPass>(pDevice); }
            }
        );
    }
};

[[maybe_unused]] static PathTracingPassRegistration gPathTracingPassRegistration;
} // namespace

PathTracingPass::PathTracingPass(ref<Device> pDevice) : RenderPass(pDevice)
{
    nvrhi::BufferDesc cbDesc;
    cbDesc.byteSize = sizeof(PerFrameCB);
    cbDesc.isConstantBuffer = true;
    cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    cbDesc.keepInitialState = true;
    cbDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    cbDesc.debugName = "PathTracingPass/PerFrameCB";
    mCbPerFrame = mpDevice->getDevice()->createBuffer(cbDesc);
    mCbPerFrameSize = mCbPerFrame ? cbDesc.byteSize : 0;

    cbDesc.byteSize = sizeof(CameraData);
    cbDesc.debugName = "PathTracingPass/Camera";
    mCbCamera = mpDevice->getDevice()->createBuffer(cbDesc);
    mCbCameraSize = mCbCamera ? cbDesc.byteSize : 0;

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
    ResourceIO::uploadBuffer(mpDevice, mCbPerFrame, &mPerFrameData, sizeof(PerFrameCB));
    ResourceIO::uploadBuffer(mpDevice, mCbCamera, &mpScene->camera->getCameraData(), sizeof(CameraData));

    (*mpPass)["PerFrameCB"] = mCbPerFrame;
    (*mpPass)["gCamera"] = mCbCamera;
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
                                         .setDebugName("PathTracingPass/output")
                                         .setIsUAV(true)
                                         .setKeepInitialState(true);
    mTextureOut = mpDevice->getDevice()->createTexture(textureDesc);
}