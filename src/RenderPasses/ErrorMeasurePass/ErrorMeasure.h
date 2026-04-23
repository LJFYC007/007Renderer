#pragma once
#include "RenderPasses/RenderPass.h"
#include "ShaderPasses/ComputePass.h"

class ErrorMeasurePass : public RenderPass
{
public:
    ErrorMeasurePass(ref<Device> pDevice);

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

    enum class ErrorMetric : uint32_t
    {
        MAE = 0,    // |src - ref| per channel
        RelMSE = 1, // (src - ref)^2 / (ref^2 + eps) per channel
    };
    // Values are mirrored by kMetricMAE/kMetricRelMSE in ErrorMeasure.slang; keep in sync.

    RenderData execute(const RenderData& input) override;

    void renderUI() override;

    void setConstantReference(float3 color)
    {
        mReferenceMode = ReferenceMode::Constant;
        mConstantReferenceColor = color;
    }

    void setTextureReference(const std::string& path);

    void setSelectedOutput(OutputId id) { mSelectedOutput = id; }

    void setMetric(ErrorMetric m) { mMetric = m; }

    void setScene(ref<Scene> pScene) override;

    // RenderGraph interface
    std::string getName() const override { return "ErrorMeasure"; }
    std::vector<RenderPassInput> getInputs() const override;
    std::vector<RenderPassOutput> getOutputs() const override;

private:
    void prepareResources(uint32_t width, uint32_t height);

    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    OutputId mSelectedOutput = OutputId::Difference;
    ReferenceMode mReferenceMode = ReferenceMode::Constant;
    ErrorMetric mMetric = ErrorMetric::MAE;
    float3 mConstantReferenceColor = {1.f, 1.f, 1.f};

    struct PerFrameCB
    {
        uint32_t gWidth;
        uint32_t gHeight;
        uint32_t gSelectedOutput;
        uint32_t gReferenceMode;
        float3 gConstantColor;
        uint32_t gMetric;
    } mPerFrameData;
    static_assert(sizeof(PerFrameCB) % 16 == 0, "ErrorMeasure PerFrameCB must stay 16-byte aligned.");

    nvrhi::BufferHandle mCbPerFrame;
    nvrhi::TextureHandle mpSourceTexture;
    nvrhi::TextureHandle mpReferenceTexture;
    nvrhi::TextureHandle mpOutputTexture;
    ref<ComputePass> mpPass;
};
