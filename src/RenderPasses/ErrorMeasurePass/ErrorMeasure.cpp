#include "ErrorMeasure.h"
#include "Utils/ResourceIO.h"
#include "Utils/ExrUtils.h"
#include "Utils/Widgets.h"

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
                [](ref<Device> pDevice) { return make_ref<ErrorMeasurePass>(pDevice); }
            }
        );
    }
};

[[maybe_unused]] static ErrorMeasurePassRegistration gErrorMeasurePassRegistration;

const std::string kSourceName = "source";
const std::string kReferenceName = "reference";
const std::string kOutputName = "output";
} // namespace

ErrorMeasurePass::ErrorMeasurePass(ref<Device> pDevice) : RenderPass(pDevice)
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
    mpPass = make_ref<ComputePass>(pDevice, "/src/RenderPasses/ErrorMeasurePass/ErrorMeasure.slang", "main");
    mpPass->addConstantBuffer(mCbPerFrame, &mPerFrameData, sizeof(PerFrameCB));
}

void ErrorMeasurePass::setTextureReference(const std::string& path)
{
    mpReferenceTexture = ExrUtils::loadExrToTexture(mpDevice, path);
    if (mpReferenceTexture)
    {
        mReferenceMode = ReferenceMode::Texture;
        LOG_INFO("ErrorMeasure: loaded reference texture from '{}'", path);
    }
    else
    {
        LOG_WARN("ErrorMeasure: failed to load '{}', staying in current mode", path);
    }
}

void ErrorMeasurePass::setScene(ref<Scene> pScene)
{
    RenderPass::setScene(pScene);
    if (pScene && pScene->camera)
    {
        uint32_t w = pScene->camera->getWidth();
        uint32_t h = pScene->camera->getHeight();
        if (w > 0 && h > 0 && (w != mWidth || h != mHeight))
        {
            mWidth = w;
            mHeight = h;
            prepareResources(mWidth, mHeight);
        }
    }
}

std::vector<RenderPassInput> ErrorMeasurePass::getInputs() const
{
    return {RenderPassInput(kSourceName, RenderDataType::Texture2D), RenderPassInput(kReferenceName, RenderDataType::Texture2D, true)};
}

std::vector<RenderPassOutput> ErrorMeasurePass::getOutputs() const
{
    return {RenderPassOutput(kOutputName, RenderDataType::Texture2D)};
}

RenderData ErrorMeasurePass::execute(const RenderData& renderData)
{
    mpSourceTexture = dynamic_cast<nvrhi::ITexture*>(renderData[kSourceName].Get());
    uint2 resolution = uint2(mpSourceTexture->getDesc().width, mpSourceTexture->getDesc().height);

    if (resolution.x != mWidth || resolution.y != mHeight)
    {
        mWidth = resolution.x;
        mHeight = resolution.y;
        prepareResources(mWidth, mHeight);
    }

    // Resolution mismatch check only applies in Texture mode
    if (mReferenceMode == ReferenceMode::Texture && mpReferenceTexture)
    {
        uint2 refRes = uint2(mpReferenceTexture->getDesc().width, mpReferenceTexture->getDesc().height);
        if (resolution.x != refRes.x || resolution.y != refRes.y)
        {
            LOG_WARN("Resolution mismatch: source({}x{}) vs reference({}x{})", resolution.x, resolution.y, refRes.x, refRes.y);
            RenderData output;
            output.setResource(kOutputName, mpSourceTexture);
            mSelectedOutput = OutputId::Source;
            return output;
        }
    }

    mPerFrameData.gWidth = mWidth;
    mPerFrameData.gHeight = mHeight;
    mPerFrameData.gSelectedOutput = static_cast<uint32_t>(mSelectedOutput);
    mPerFrameData.gReferenceMode = static_cast<uint32_t>(mReferenceMode);
    mPerFrameData.gConstantColor = mConstantReferenceColor;

    (*mpPass)["PerFrameCB"] = mCbPerFrame;
    (*mpPass)["source"] = mpSourceTexture;
    if (mReferenceMode == ReferenceMode::Texture && mpReferenceTexture)
        (*mpPass)["reference"] = mpReferenceTexture;
    else
        (*mpPass)["reference"] = mpSourceTexture; // Dummy bind; shader won't read it in Constant mode
    (*mpPass)["output"] = mpOutputTexture;
    mpPass->execute(mWidth, mHeight, 1);

    RenderData output;
    output.setResource(kOutputName, mpOutputTexture);
    return output;
}

void ErrorMeasurePass::renderUI()
{
    Widgets::subHeader("Output Selection");
    if (GUI::RadioButton("Source", mSelectedOutput == OutputId::Source))
        mSelectedOutput = OutputId::Source;
    if (GUI::RadioButton("Reference", mSelectedOutput == OutputId::Reference))
        mSelectedOutput = OutputId::Reference;
    if (GUI::RadioButton("Difference", mSelectedOutput == OutputId::Difference))
        mSelectedOutput = OutputId::Difference;
}

void ErrorMeasurePass::prepareResources(uint32_t width, uint32_t height)
{
    nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
                                         .setWidth(width)
                                         .setHeight(height)
                                         .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                         .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                         .setDebugName("ErrorMeasure/outputTexture")
                                         .setIsUAV(true)
                                         .setKeepInitialState(true);
    mpOutputTexture = mpDevice->getDevice()->createTexture(textureDesc);
}
