#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/RayTracingPass.h"
#include "Core/Buffer.h"

class PathTracingPass : public RenderPass
{
public:
    PathTracingPass(ref<Device> device);

    RenderData execute(const RenderData& input) override;

    void renderUI() override;

private:
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t frameCount = 0;
    uint32_t maxDepth = 5;
    float gColorSlider = 1.f; // UI slider value

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
        uint32_t maxDepth;
        uint32_t frameCount;
        float gColor;
    } perFrameData;

    Buffer cbPerFrame;
    nvrhi::TextureHandle textureOut;
    ref<RayTracingPass> pass;
};