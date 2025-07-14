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

    void setScene(ref<Scene> scene) override
    {
        m_Scene = scene;
        reset = true;
    }

private:
    void prepareResources();

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frameCount = 0;
    bool reset = false;

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
        uint32_t frameCount;
        uint32_t reset;
    } perFrameData;

    Buffer cbPerFrame;
    nvrhi::TextureHandle textureOut;
    nvrhi::TextureHandle accumulateTexture;
    ref<ComputePass> pass;
};
