#include "ComputePass.h"

bool ComputePass::initialize(
    nvrhi::IDevice* device,
    const std::string& shaderPath,
    const std::string& entryPoint,
    const std::vector<nvrhi::BindingLayoutItem>& layoutItems,
    const std::vector<nvrhi::BindingSetItem>& bindings
)
{
    ShaderProgram program;
    if (!program.loadFromFile(device, shaderPath, entryPoint, nvrhi::ShaderType::Compute))
    {
        return false;
    }

    m_Shader = program.getShader();

    // Create binding layout
    nvrhi::BindingLayoutDesc layoutDesc;
    layoutDesc.visibility = nvrhi::ShaderType::Compute;
    layoutDesc.bindings = layoutItems;

    m_BindingLayout = device->createBindingLayout(layoutDesc);

    // Create binding set
    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings = bindings;

    m_BindingSet = device->createBindingSet(bindingSetDesc, m_BindingLayout);

    // Create compute pipeline
    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = {m_BindingLayout};
    pipelineDesc.setComputeShader(m_Shader);

    m_Pipeline = device->createComputePipeline(pipelineDesc);
    return m_Pipeline;
}

void ComputePass::dispatch(nvrhi::IDevice* device, uint32_t threadX, uint32_t threadY, uint32_t threadZ)
{
    auto commandList = device->createCommandList();
    commandList->open();

    const nvrhi::BindingSetDesc& desc = *m_BindingSet->getDesc();
    for (const nvrhi::BindingSetItem& item : desc.bindings)
    {
        if (item.type == nvrhi::ResourceType::StructuredBuffer_SRV || item.type == nvrhi::ResourceType::StructuredBuffer_UAV)
        {
            nvrhi::IBuffer* buffer = dynamic_cast<nvrhi::IBuffer*>(item.resourceHandle);
            if (buffer)
            {
                commandList->beginTrackingBufferState(
                    buffer,
                    item.type == nvrhi::ResourceType::StructuredBuffer_SRV ? nvrhi::ResourceStates::ShaderResource
                                                                           : nvrhi::ResourceStates::UnorderedAccess
                );
            }
        }
    }

    nvrhi::ComputeState state;
    state.pipeline = m_Pipeline;
    state.bindings = {m_BindingSet};

    commandList->setComputeState(state);
    commandList->dispatch(threadX, threadY, threadZ);

    commandList->close();
    device->executeCommandList(commandList);
}