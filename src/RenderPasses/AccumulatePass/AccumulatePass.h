#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/ComputePass.h"

class AccumulatePass : public RenderPass
{
public:
    AccumulatePass(ref<Device> pDevice);

    RenderData execute(const RenderData& input) override;

    void renderUI() override;

    void setScene(ref<Scene> pScene) override
    {
        mpScene = pScene;
        mReset = true;
    }

    // RenderGraph interface
    std::string getName() const override { return "Accumulate"; }
    std::vector<RenderPassInput> getInputs() const override;
    std::vector<RenderPassOutput> getOutputs() const override;

private:
    void prepareResources();

    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    uint32_t mFrameCount = 0;
    bool mReset = false;

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
        uint32_t frameCount;
        uint32_t reset;
    } mPerFrameData;

    nvrhi::BufferHandle mCbPerFrame;
    size_t mCbPerFrameSize = 0;
    nvrhi::TextureHandle mTextureOut;
    nvrhi::TextureHandle mAccumulateTexture;
    ref<ComputePass> mpPass;
};
