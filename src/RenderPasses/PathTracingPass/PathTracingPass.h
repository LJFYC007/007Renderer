#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/RayTracingPass.h"

class PathTracingPass : public RenderPass
{
public:
    PathTracingPass(ref<Device> pDevice);

    RenderData execute(const RenderData& input = RenderData()) override;

    void renderUI() override;

    // RenderGraph interface
    std::string getName() const override { return "PathTracing"; }
    std::vector<RenderPassInput> getInputs() const override { return {}; } // No inputs, generates from scene
    std::vector<RenderPassOutput> getOutputs() const override { return {RenderPassOutput("output", RenderDataType::Texture2D)}; }

private:
    void prepareResources();

    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mFrameCount = 0;
    uint32_t mMaxDepth = 5;
    float mGColorSlider = 1.f; // UI slider value

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
    size_t mCbPerFrameSize = 0;
    size_t mCbCameraSize = 0;
    nvrhi::TextureHandle mTextureOut;
    ref<RayTracingPass> mpPass;
};