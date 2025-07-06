#include "ComputePass.h"
#include "Core/Program/Program.h"

ComputePass::ComputePass(ref<Device> device, const std::string& shaderPath, const std::string& entryPoint) : Pass(device)
{
    auto nvrhiDevice = device->getDevice();
    std::unordered_map<std::string, nvrhi::ShaderType> entryPoints;
    entryPoints[entryPoint] = nvrhi::ShaderType::Compute;

    Program program(nvrhiDevice, std::string(PROJECT_DIR) + shaderPath, entryPoints, "cs_6_2");
    m_Shader = program.getShader(entryPoint);
    // program.printReflectionInfo();
    if (!program.generateBindingLayout())
        LOG_ERROR_RETURN("[ComputePass] Failed to generate binding layout from program");
    m_BindingSetManager = make_ref<BindingSetManager>(device, program.getReflectionInfo());

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

    // Create compute pipeline
    nvrhi::ComputePipelineDesc pipelineDesc;
    std::vector<nvrhi::BindingLayoutHandle> bindingLayouts = m_BindingSetManager->getBindingLayouts();
    for (const auto& layout : bindingLayouts)
        if (layout)
            pipelineDesc.addBindingLayout(layout);

    pipelineDesc.setComputeShader(m_Shader);
    m_Pipeline = nvrhiDevice->createComputePipeline(pipelineDesc);
    if (!m_Pipeline)
        LOG_ERROR_RETURN("[ComputePass] Failed to create compute pipeline");
    LOG_DEBUG("[ComputePass] Compute pipeline created successfully");
}

void ComputePass::dispatch(uint32_t width, uint32_t height, uint32_t depth)
{
    auto nvrhiDevice = m_Device->getDevice();
    auto commandList = m_Device->getCommandList();
    commandList->open();
    trackingResourceState(commandList);

    nvrhi::ComputeState state;
    state.pipeline = m_Pipeline;
    std::vector<nvrhi::BindingSetHandle> bindingSets = m_BindingSetManager->getBindingSets();
    for (const auto& bindingSet : bindingSets)
        if (bindingSet)
            state.addBindingSet(bindingSet);

    commandList->setComputeState(state);
    commandList->dispatch(width, height, depth);
    commandList->close();
    nvrhiDevice->executeCommandList(commandList);
}

void ComputePass::execute(uint32_t width, uint32_t height, uint32_t depth)
{
    uint32_t threadGroupX = (width + m_WorkGroupSizeX - 1) / m_WorkGroupSizeX;
    uint32_t threadGroupY = (height + m_WorkGroupSizeY - 1) / m_WorkGroupSizeY;
    uint32_t threadGroupZ = (depth + m_WorkGroupSizeZ - 1) / m_WorkGroupSizeZ;

    LOG_TRACE("[ComputePass] Total threads: {}x{}x{}, Thread groups: {}x{}x{}", width, height, depth, threadGroupX, threadGroupY, threadGroupZ);
    dispatch(threadGroupX, threadGroupY, threadGroupZ);
}
