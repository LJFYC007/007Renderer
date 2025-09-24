#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/ComputePass.h"

class ErrorMeasure : public RenderPass
{
public:
    ErrorMeasure(ref<Device> pDevice);

    enum class OutputId
    {
        Source,
        Reference,
        Difference,
    };

    RenderData execute(const RenderData& input) override;

    void renderUI() override;

    // RenderGraph interface
    std::string getName() const override { return "ErrorMeasure"; }
    std::vector<RenderPassInput> getInputs() const override;
    std::vector<RenderPassOutput> getOutputs() const override;

private:
    void prepareResources();

    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    OutputId mSelectedOutput = OutputId::Difference;

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
    } mPerFrameData;

    nvrhi::BufferHandle mCbPerFrame;
    size_t mCbPerFrameSize = 0;
    nvrhi::TextureHandle mpSourceTexture;
    nvrhi::TextureHandle mpReferenceTexture;
    nvrhi::TextureHandle mpDifferenceTexture;
    ref<ComputePass> mpPass;
};
