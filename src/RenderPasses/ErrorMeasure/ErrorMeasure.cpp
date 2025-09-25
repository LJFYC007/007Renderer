#include "ErrorMeasure.h"
#include "Utils/ResourceIO.h"
#include "Utils/ExrUtils.h"

namespace
{
struct ErrorMeasurePassRegistration
{
    ErrorMeasurePassRegistration()
    {
        RenderPassRegistry::registerPass(
            RenderPassDescriptor{
                "ErrorMeasure",
                "Generates an error visualization comparing a source texture against an optional reference input.",
                [](ref<Device> pDevice) { return make_ref<ErrorMeasure>(pDevice); }
            }
        );
    }
};

[[maybe_unused]] static ErrorMeasurePassRegistration gErrorMeasurePassRegistration;

const std::string kSourceName = "source";
const std::string kReferenceName = "reference";
const std::string kOutputName = "output";
} // namespace

ErrorMeasure::ErrorMeasure(ref<Device> pDevice) : RenderPass(pDevice)
{
    nvrhi::BufferDesc cbDesc;
    cbDesc.byteSize = sizeof(PerFrameCB);
    cbDesc.isConstantBuffer = true;
    cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    cbDesc.keepInitialState = true;
    cbDesc.cpuAccess = nvrhi::CpuAccessMode::None;
    cbDesc.isVolatile = true;
    cbDesc.debugName = "ErrorMeasure/PerFrameCB";
    mCbPerFrame = mpDevice->getDevice()->createBuffer(cbDesc);
    mpPass = make_ref<ComputePass>(pDevice, "/src/RenderPasses/ErrorMeasure/ErrorMeasure.slang", "main");
    mpPass->addConstantBuffer(mCbPerFrame, &mPerFrameData, sizeof(PerFrameCB));

    // Load reference texture from EXR file
    mpReferenceTexture = ExrUtils::loadExrToTexture(pDevice, std::string(PROJECT_DIR) + "/media/reference.exr");
    mWidth = mpReferenceTexture->getDesc().width;
    mHeight = mpReferenceTexture->getDesc().height;

    nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
                                         .setWidth(mWidth)
                                         .setHeight(mHeight)
                                         .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                         .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                         .setDebugName("ErrorMeasure/differenceTexture")
                                         .setIsUAV(true)
                                         .setKeepInitialState(true);
    mpDifferenceTexture = mpDevice->getDevice()->createTexture(textureDesc);
}

std::vector<RenderPassInput> ErrorMeasure::getInputs() const
{
    return {RenderPassInput(kSourceName, RenderDataType::Texture2D), RenderPassInput(kReferenceName, RenderDataType::Texture2D, true)};
}

std::vector<RenderPassOutput> ErrorMeasure::getOutputs() const
{
    return {RenderPassOutput(kOutputName, RenderDataType::Texture2D)};
}

RenderData ErrorMeasure::execute(const RenderData& renderData)
{
    mpSourceTexture = dynamic_cast<nvrhi::ITexture*>(renderData[kSourceName].Get());
    uint2 resolution = uint2(mpSourceTexture->getDesc().width, mpSourceTexture->getDesc().height);
    if (resolution.x != mWidth || resolution.y != mHeight)
    {
        LOG_WARN("Resolution mismatch: source({}x{}) vs reference({}x{})", resolution.x, resolution.y, mWidth, mHeight);
        RenderData output;
        output.setResource("output", mpSourceTexture);
        mSelectedOutput = OutputId::Source; // Fallback to source
        return output;
    }

    mPerFrameData.gWidth = mWidth;
    mPerFrameData.gHeight = mHeight;

    (*mpPass)["PerFrameCB"] = mCbPerFrame;
    (*mpPass)["source"] = mpSourceTexture;
    (*mpPass)["reference"] = mpReferenceTexture;
    (*mpPass)["difference"] = mpDifferenceTexture;
    mpPass->execute(mWidth, mHeight, 1);

    // Set output based on user selection
    RenderData output;
    switch (mSelectedOutput)
    {
    case OutputId::Source:
        output.setResource(kOutputName, mpSourceTexture);
        break;
    case OutputId::Reference:
        output.setResource(kOutputName, mpReferenceTexture);
        break;
    case OutputId::Difference:
    default:
        output.setResource(kOutputName, mpDifferenceTexture);
        break;
    }
    return output;
}

void ErrorMeasure::renderUI()
{
    GUI::Text("Output Selection:");
    if (GUI::RadioButton("Source", mSelectedOutput == OutputId::Source))
        mSelectedOutput = OutputId::Source;
    if (GUI::RadioButton("Reference", mSelectedOutput == OutputId::Reference))
        mSelectedOutput = OutputId::Reference;
    if (GUI::RadioButton("Difference", mSelectedOutput == OutputId::Difference))
        mSelectedOutput = OutputId::Difference;
}
