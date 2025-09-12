#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/RayTracingPass.h"
#include "Core/Buffer.h"

class PathTracingPass : public RenderPass
{
public:
    PathTracingPass(ref<Device> device);

    RenderData execute(const RenderData& input = RenderData()) override;

    void renderUI() override;

private:
    void prepareResources();

    uint32_t width;
    uint32_t height;
    uint32_t frameCount = 0;
    uint32_t maxDepth = 10;
    float gColorSlider = 1.f; // UI slider value

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
        uint32_t maxDepth;
        uint32_t frameCount;
        float gColor;
    } perFrameData;

    Buffer cbPerFrame, cbCamera;
    nvrhi::TextureHandle textureOut;
    ref<RayTracingPass> pass;
};