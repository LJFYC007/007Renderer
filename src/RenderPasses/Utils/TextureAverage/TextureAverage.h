#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/ComputePass.h"
#include "Core/Buffer.h"
#include "Utils/Math/Math.h"

class TextureAverage : public RenderPass
{
public:
    TextureAverage(ref<Device> pDevice);

    RenderData execute(const RenderData& input) override;

    void renderUI() override;

    // RenderGraph interface
    std::string getName() const override { return "TextureAverage"; }
    std::vector<RenderPassInput> getInputs() const override;
    std::vector<RenderPassOutput> getOutputs() const override;

private:
    RENDERPASS_FRIEND_TEST(PathTracerTest, Basic);
    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
    } mPerFrameData;

    float4 mAverageResult;

    Buffer mCbPerFrame;
    Buffer mResultBuffer;
    ref<ComputePass> mpPass;
    nvrhi::TextureHandle mpInputTexture;
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
};
