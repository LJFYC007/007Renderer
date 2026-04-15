#include "PathTracing.h"
#include "Utils/Logger.h"
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
    cbDesc.isVolatile = true;
    cbDesc.debugName = "PathTracingPass/PerFrameCB";
    mCbPerFrame = mpDevice->getDevice()->createBuffer(cbDesc);

    cbDesc.byteSize = sizeof(CameraData);
    cbDesc.debugName = "PathTracingPass/Camera";
    mCbCamera = mpDevice->getDevice()->createBuffer(cbDesc);

    nvrhi::SamplerDesc samplerDesc;
    samplerDesc.setAllFilters(true);
    samplerDesc.setMaxAnisotropy(16.f);
    samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Repeat);
    mTextureSampler = mpDevice->getDevice()->createSampler(samplerDesc);

    buildRayTracingPass();
}

void PathTracingPass::buildRayTracingPass()
{
    LOG_DEBUG("[PathTracingPass] Recompiling path tracing shader program (furnaceMode={})", static_cast<uint32_t>(mFurnaceMode));
    std::vector<std::pair<std::string, nvrhi::ShaderType>> entryPoints = {
        {"rayGenMain", nvrhi::ShaderType::RayGeneration},
        {"missMain", nvrhi::ShaderType::Miss},
        {"shadowMissMain", nvrhi::ShaderType::Miss},
        {"closestHitMain", nvrhi::ShaderType::ClosestHit}
    };
    std::vector<std::pair<std::string, std::string>> defines;
    if (mFurnaceMode == FurnaceMode::WeakWhiteFurnace)
        defines.emplace_back("WEAK_WHITE_FURNACE", "1");

    mpPass.reset();
    mpPass = make_ref<RayTracingPass>(mpDevice, "/src/RenderPasses/PathTracingPass/PathTracing.slang", entryPoints, defines);
    mpPass->addConstantBuffer(mCbPerFrame, &mPerFrameData, sizeof(PerFrameCB));
    if (mpScene)
        mpPass->addConstantBuffer(mCbCamera, &mpScene->camera->getCameraData(), sizeof(CameraData));
}

void PathTracingPass::setFurnaceMode(FurnaceMode mode)
{
    if (mode == mFurnaceMode)
        return;
    mFurnaceMode = mode;
    buildRayTracingPass();
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
    mPerFrameData.emissiveTriangleCount = mpScene->getEmissiveTriangleCount();
    mPerFrameData.totalEmissiveArea = mpScene->totalEmissiveArea;

    RenderData output;
    output.setResource("output", mTextureOut);
    (*mpPass)["PerFrameCB"] = mCbPerFrame;
    (*mpPass)["gCamera"] = mCbCamera;
    (*mpPass)["gScene.vertices"] = mpScene->getVertexBuffer();
    (*mpPass)["gScene.indices"] = mpScene->getIndexBuffer();
    (*mpPass)["gScene.meshes"] = mpScene->getMeshBuffer();
    (*mpPass)["gScene.triangleToMesh"] = mpScene->getTriangleToMeshBuffer();
    (*mpPass)["gScene.materials"] = mpScene->getMaterialBuffer();
    (*mpPass)["gScene.rtAccel"] = mpScene->getTLAS();
    (*mpPass)["gScene.emissiveTriangles"] = mpScene->getEmissiveTriangleBuffer();

    // Bind all textures to descriptor table for bindless access
    // Pass default texture to fill unused slots
    mpPass->setDescriptorTable("gMaterialTextures.textures", mpScene->getTextures(), mpScene->getDefaultTexture());

    // Bind sampler separately (in different register space)
    (*mpPass)["gMaterialSampler.sampler"] = mTextureSampler;

    (*mpPass)["result"] = mTextureOut;
    mpPass->execute(mWidth, mHeight, 1);
    return output;
}

void PathTracingPass::renderUI()
{
    GUI::SliderFloat("gColor", &mGColorSlider, 0.0f, 5.0f);

    static const char* furnaceModeLabels[] = {"Off", "Weak White Furnace"};
    int furnaceIdx = static_cast<int>(mFurnaceMode);
    if (GUI::Combo("Furnace Mode", &furnaceIdx, furnaceModeLabels, 2))
        setFurnaceMode(static_cast<FurnaceMode>(furnaceIdx));
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