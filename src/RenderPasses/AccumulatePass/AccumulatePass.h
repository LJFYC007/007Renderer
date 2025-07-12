#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/ComputePass.h"
#include "Core/Buffer.h"

class AccumulatePass : public RenderPass
{
public:
    AccumulatePass(ref<Device> device);

    RenderData execute(const RenderData& input) override;

    void renderUI() override;

private:
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t frameCount = 0;

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
        uint32_t frameCount;
    } perFrameData;

    Buffer cbPerFrame;
    nvrhi::TextureHandle textureOut;
    nvrhi::TextureHandle accumulateTexture;
    ref<ComputePass> pass;
};
