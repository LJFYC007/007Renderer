#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/ComputePass.h"

class ToneMappingPass : public RenderPass
{
public:
    ToneMappingPass(ref<Device> pDevice);

    RenderData execute(const RenderData& input) override;

    void renderUI() override {}

    // RenderGraph interface
    std::string getName() const override { return "ToneMapping"; }
    std::vector<RenderPassInput> getInputs() const override;
    std::vector<RenderPassOutput> getOutputs() const override;

private:
    void prepareResources();

    uint32_t mWidth = 0;
    uint32_t mHeight = 0;

    nvrhi::TextureHandle mTextureOut;
    ref<ComputePass> mpPass;
};
