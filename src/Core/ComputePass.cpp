#include "ComputePass.h"

bool ComputePass::initialize(
    nvrhi::IDevice* device,
    const std::string& shaderPath,
    const std::string& entryPoint,
    const std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>>& resourceMap
)
{
    ShaderProgram program;
    if (!program.loadFromFile(device, shaderPath, entryPoint, nvrhi::ShaderType::Compute))
        return false;
    m_Shader = program.getShader();
    program.printReflectionInfo();

    auto programLayout = program.getProgramLayout();
    if (programLayout && programLayout->getEntryPointCount() > 0)
    {
        auto entryPointReflection = programLayout->getEntryPointByIndex(0);
        if (entryPointReflection)
        {
            SlangUInt workGroupSize[3] = {1, 1, 1};
            entryPointReflection->getComputeThreadGroupSize(3, workGroupSize);
            m_WorkGroupSizeX = static_cast<uint32_t>(workGroupSize[0]);
            m_WorkGroupSizeY = static_cast<uint32_t>(workGroupSize[1]);
            m_WorkGroupSizeZ = static_cast<uint32_t>(workGroupSize[2]);

            LOG_DEBUG("[ComputePass] Work group size: {}x{}x{}", m_WorkGroupSizeX, m_WorkGroupSizeY, m_WorkGroupSizeZ);
        }
    }

    std::vector<nvrhi::BindingLayoutItem> layoutItems;
    std::vector<nvrhi::BindingSetItem> bindings;
    if (!program.generateBindingLayout(layoutItems, bindings, resourceMap))
        return false;

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

void ComputePass::dispatch(nvrhi::IDevice* device, uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ)
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
            else
                LOG_WARN("[ComputePass] WARNING: Resource handle for slot {} is not a buffer", item.slot);
        }
        else if (item.type == nvrhi::ResourceType::Texture_SRV || item.type == nvrhi::ResourceType::Texture_UAV)
        {
            nvrhi::ITexture* texture = dynamic_cast<nvrhi::ITexture*>(item.resourceHandle);
            if (texture)
            {
                nvrhi::TextureSubresourceSet allSubresources;
                commandList->beginTrackingTextureState(
                    texture,
                    allSubresources,
                    item.type == nvrhi::ResourceType::Texture_SRV ? nvrhi::ResourceStates::ShaderResource
                                                                  : nvrhi::ResourceStates::UnorderedAccess
                );
            }
            else
                LOG_WARN("[ComputePass] WARNING: Resource handle for slot {} is not a texture", item.slot);
        }
        else
            LOG_WARN("[ComputePass] WARNING: Unsupported resource type for slot {}", item.slot);
    }

    nvrhi::ComputeState state;
    state.pipeline = m_Pipeline;
    state.bindings = {m_BindingSet};
    commandList->setComputeState(state);
    commandList->dispatch(threadGroupX, threadGroupY, threadGroupZ);
    commandList->close();
    device->executeCommandList(commandList);
}

void ComputePass::dispatchThreads(nvrhi::IDevice* device, uint32_t totalThreadsX, uint32_t totalThreadsY, uint32_t totalThreadsZ)
{
    uint32_t threadGroupX = (totalThreadsX + m_WorkGroupSizeX - 1) / m_WorkGroupSizeX;
    uint32_t threadGroupY = (totalThreadsY + m_WorkGroupSizeY - 1) / m_WorkGroupSizeY;
    uint32_t threadGroupZ = (totalThreadsZ + m_WorkGroupSizeZ - 1) / m_WorkGroupSizeZ;

    LOG_DEBUG(
        "[ComputePass] Total threads: {}x{}x{}, Thread groups: {}x{}x{}",
        totalThreadsX,
        totalThreadsY,
        totalThreadsZ,
        threadGroupX,
        threadGroupY,
        threadGroupZ
    );
    dispatch(device, threadGroupX, threadGroupY, threadGroupZ);
}
