#include "ToneMapping.h"

namespace
{
struct ToneMappingPassRegistration
{
    ToneMappingPassRegistration()
    {
        RenderPassRegistry::registerPass(
            RenderPassDescriptor{
                "ToneMapping",
                "Converts linear radiance to display-encoded output.",
                [](ref<Device> pDevice) { return make_ref<ToneMappingPass>(pDevice); }
            }
        );
    }
};

[[maybe_unused]] static ToneMappingPassRegistration gToneMappingPassRegistration;

const std::string kInputName = "input";
const std::string kOutputName = "output";
} // namespace

ToneMappingPass::ToneMappingPass(ref<Device> pDevice) : RenderPass(pDevice)
{
    mpPass = make_ref<ComputePass>(pDevice, "/src/RenderPasses/ToneMappingPass/ToneMapping.slang", "main");
}

std::vector<RenderPassInput> ToneMappingPass::getInputs() const
{
    return {RenderPassInput(kInputName, RenderDataType::Texture2D)};
}

std::vector<RenderPassOutput> ToneMappingPass::getOutputs() const
{
    return {RenderPassOutput(kOutputName, RenderDataType::Texture2D)};
}

RenderData ToneMappingPass::execute(const RenderData& renderData)
{
    nvrhi::TextureHandle pInputTexture = dynamic_cast<nvrhi::ITexture*>(renderData[kInputName].Get());
    uint2 resolution = uint2(pInputTexture->getDesc().width, pInputTexture->getDesc().height);
    if (resolution.x != mWidth || resolution.y != mHeight)
    {
        mWidth = resolution.x;
        mHeight = resolution.y;
        prepareResources();
    }

    RenderData output;
    output.setResource(kOutputName, mTextureOut);
    (*mpPass)[kInputName] = pInputTexture;
    (*mpPass)[kOutputName] = mTextureOut;
    mpPass->execute(mWidth, mHeight, 1);
    return output;
}

void ToneMappingPass::prepareResources()
{
    nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
                                         .setWidth(mWidth)
                                         .setHeight(mHeight)
                                         .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                         .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                         .setDebugName("ToneMappingPass/output")
                                         .setIsUAV(true)
                                         .setKeepInitialState(true);
    mTextureOut = mpDevice->getDevice()->createTexture(textureDesc);
}
