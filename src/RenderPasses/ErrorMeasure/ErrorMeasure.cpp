#include "ErrorMeasure.h"
#include "Utils/ExrUtils.h"

ErrorMeasure::ErrorMeasure(ref<Device> device) : RenderPass(device)
{
    cbPerFrame.initialize(device, &perFrameData, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB");
    pass = make_ref<ComputePass>(device, "/src/RenderPasses/ErrorMeasure/ErrorMeasure.slang", "main");

    // Load reference texture from EXR file
    pReferenceTexture = ExrUtils::loadExrToTexture(device, std::string(PROJECT_DIR) + "/media/reference.exr");
    width = pReferenceTexture->getDesc().width;
    height = pReferenceTexture->getDesc().height;

    nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc()
                                         .setWidth(width)
                                         .setHeight(height)
                                         .setFormat(nvrhi::Format::RGBA32_FLOAT)
                                         .setInitialState(nvrhi::ResourceStates::UnorderedAccess)
                                         .setDebugName("differenceTexture")
                                         .setIsUAV(true);
    pDifferenceTexture = m_Device->getDevice()->createTexture(textureDesc);
}

RenderData ErrorMeasure::execute(const RenderData& input)
{
    pSourceTexture = dynamic_cast<nvrhi::ITexture*>(input["output"].Get());
    uint2 resolution = uint2(pSourceTexture->getDesc().width, pSourceTexture->getDesc().height);
    if (resolution.x != width || resolution.y != height)
    {
        LOG_WARN("Resolution mismatch: source({}x{}) vs reference({}x{})", resolution.x, resolution.y, width, height);
        RenderData output;
        output.setResource("output", pSourceTexture);
        selectedOutput = OutputId::Source; // Fallback to source
        return output;
    }

    perFrameData.gWidth = width;
    perFrameData.gHeight = height;

    cbPerFrame.updateData(m_Device, &perFrameData, sizeof(PerFrameCB));
    (*pass)["PerFrameCB"] = cbPerFrame.getHandle();
    (*pass)["source"] = pSourceTexture;
    (*pass)["reference"] = pReferenceTexture;
    (*pass)["difference"] = pDifferenceTexture;
    pass->execute(width, height, 1);

    // Set output based on user selection
    RenderData output;
    switch (selectedOutput)
    {
    case OutputId::Source:
        output.setResource("output", pSourceTexture);
        break;
    case OutputId::Reference:
        output.setResource("output", pReferenceTexture);
        break;
    case OutputId::Difference:
    default:
        output.setResource("output", pDifferenceTexture);
        break;
    }

    return output;
}

void ErrorMeasure::renderUI()
{
    GUI::Text("Error Measure Pass");

    GUI::Text("Output Selection:");
    if (GUI::RadioButton("Source", selectedOutput == OutputId::Source))
        selectedOutput = OutputId::Source;
    if (GUI::RadioButton("Reference", selectedOutput == OutputId::Reference))
        selectedOutput = OutputId::Reference;
    if (GUI::RadioButton("Difference", selectedOutput == OutputId::Difference))
        selectedOutput = OutputId::Difference;
}
