#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/RayTracingPass.h"

enum class FurnaceMode : uint32_t
{
    Off = 0,
    WeakWhiteFurnace = 1,
};

class PathTracingPass : public RenderPass
{
public:
    PathTracingPass(ref<Device> pDevice);

    RenderData execute(const RenderData& input = RenderData()) override;

    void renderUI() override;

    void setMissColor(float c) { mGColorSlider = c; }
    void setFurnaceMode(FurnaceMode mode);

    void setScene(ref<Scene> pScene) override
    {
        mpScene = pScene;
        // Store a pointer to the live CPU-side CameraData. RayTracingPass uploads all
        // registered constant buffers right before dispatch, so per-frame jitter updates
        // written by Camera::calculateCameraParameters() are visible on the GPU.
        mpPass->addConstantBuffer(mCbCamera, &mpScene->camera->getCameraData(), sizeof(CameraData));
    }

    // RenderGraph interface
    std::string getName() const override { return "PathTracing"; }
    std::vector<RenderPassInput> getInputs() const override { return {}; } // No inputs, generates from scene
    std::vector<RenderPassOutput> getOutputs() const override { return {RenderPassOutput("output", RenderDataType::Texture2D)}; }

private:
    void prepareResources();
    void buildRayTracingPass();

    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mFrameCount = 0;
    uint32_t mMaxDepth = 10;
    float mGColorSlider = 0.f; // UI slider value
    FurnaceMode mFurnaceMode = FurnaceMode::Off;

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
        uint32_t maxDepth;
        uint32_t frameCount;
        float gColor;
    } mPerFrameData;

    nvrhi::BufferHandle mCbPerFrame;
    nvrhi::BufferHandle mCbCamera;
    nvrhi::TextureHandle mTextureOut;
    nvrhi::SamplerHandle mTextureSampler;
    ref<RayTracingPass> mpPass;
};
