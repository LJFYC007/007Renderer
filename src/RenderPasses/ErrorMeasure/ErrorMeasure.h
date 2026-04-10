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

    enum class ReferenceMode
    {
        Texture,
        Constant,
    };

    RenderData execute(const RenderData& input) override;

    void renderUI() override;

    void setConstantReference(float3 color)
    {
        mReferenceMode = ReferenceMode::Constant;
        mConstantReferenceColor = color;
    }

    void setSelectedOutput(OutputId id) { mSelectedOutput = id; }

    // RenderGraph interface
    std::string getName() const override { return "ErrorMeasure"; }
    std::vector<RenderPassInput> getInputs() const override;
    std::vector<RenderPassOutput> getOutputs() const override;

private:
    void prepareResources(uint32_t width, uint32_t height);

    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    OutputId mSelectedOutput = OutputId::Difference;
    ReferenceMode mReferenceMode = ReferenceMode::Texture;
    float3 mConstantReferenceColor = {1.f, 1.f, 1.f};

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
        uint32_t gSelectedOutput;
        uint32_t gReferenceMode;
        float3 gConstantColor;
        float _padding;
    } mPerFrameData;

    nvrhi::BufferHandle mCbPerFrame;
    nvrhi::TextureHandle mpSourceTexture;
    nvrhi::TextureHandle mpReferenceTexture;
    nvrhi::TextureHandle mpOutputTexture;
    ref<ComputePass> mpPass;
};
