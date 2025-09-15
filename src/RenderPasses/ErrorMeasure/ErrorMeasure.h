#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/ComputePass.h"
#include "Core/Buffer.h"

class ErrorMeasure : public RenderPass
{
public:
    ErrorMeasure(ref<Device> device);

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
    std::vector<RenderPassInput> getInputs() const override { return {RenderPassInput("output", RenderDataType::Texture2D)}; }
    std::vector<RenderPassOutput> getOutputs() const override { return {RenderPassOutput("output", RenderDataType::Texture2D)}; }

private:
    void prepareResources();

    uint32_t width = 0;
    uint32_t height = 0;
    OutputId selectedOutput = OutputId::Difference;

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
    } perFrameData;

    Buffer cbPerFrame;
    nvrhi::TextureHandle pSourceTexture;
    nvrhi::TextureHandle pReferenceTexture;
    nvrhi::TextureHandle pDifferenceTexture;
    ref<ComputePass> pass;
};
