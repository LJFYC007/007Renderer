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
