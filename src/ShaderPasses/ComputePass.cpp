#include "ComputePass.h"
#include "Core/Program/Program.h"

ComputePass::ComputePass(ref<Device> pDevice, const std::string& shaderPath, const std::string& entryPoint) : Pass(pDevice)
{
    auto pNvrhiDevice = pDevice->getDevice();
    std::unordered_map<std::string, nvrhi::ShaderType> entryPoints;
    entryPoints[entryPoint] = nvrhi::ShaderType::Compute;

    Program program(pNvrhiDevice, std::string(PROJECT_DIR) + shaderPath, entryPoints, "cs_6_2");
    mShader = program.getShader(entryPoint);
    // program.printReflectionInfo();
    if (!program.generateBindingLayout())
        LOG_ERROR_RETURN("[ComputePass] Failed to generate binding layout from program");
    mpBindingSetManager = make_ref<BindingSetManager>(pDevice, program.getReflectionInfo());
    auto pProgramLayout = program.getProgramLayout();
    if (pProgramLayout && pProgramLayout->getEntryPointCount() > 0)
    {
        auto pEntryPointReflection = pProgramLayout->getEntryPointByIndex(0);
        if (pEntryPointReflection)
        {
            SlangUInt workGroupSize[3] = {1, 1, 1};
            pEntryPointReflection->getComputeThreadGroupSize(3, workGroupSize);
            mWorkGroupSizeX = static_cast<uint32_t>(workGroupSize[0]);
            mWorkGroupSizeY = static_cast<uint32_t>(workGroupSize[1]);
            mWorkGroupSizeZ = static_cast<uint32_t>(workGroupSize[2]);

            LOG_DEBUG("[ComputePass] Work group size: {}x{}x{}", mWorkGroupSizeX, mWorkGroupSizeY, mWorkGroupSizeZ);
        }
    } // Create compute pipeline
    nvrhi::ComputePipelineDesc pipelineDesc;
    std::vector<nvrhi::BindingLayoutHandle> bindingLayouts = mpBindingSetManager->getBindingLayouts();
    for (const auto& pLayout : bindingLayouts)
        if (pLayout)
            pipelineDesc.addBindingLayout(pLayout);

    pipelineDesc.setComputeShader(mShader);
    mPipeline = pNvrhiDevice->createComputePipeline(pipelineDesc);
    if (!mPipeline)
        LOG_ERROR_RETURN("[ComputePass] Failed to create compute pipeline");
    LOG_DEBUG("[ComputePass] Compute pipeline created successfully");
}

void ComputePass::dispatch(uint32_t width, uint32_t height, uint32_t depth)
{
    auto pNvrhiDevice = mpDevice->getDevice();
    auto pCommandList = mpDevice->getCommandList();
    pCommandList->open();

    nvrhi::ComputeState state;
    state.pipeline = mPipeline;
    std::vector<nvrhi::BindingSetHandle> bindingSets = mpBindingSetManager->getBindingSets();
    for (const auto& pBindingSet : bindingSets)
        if (pBindingSet)
            state.addBindingSet(pBindingSet);

    pCommandList->setComputeState(state);
    pCommandList->dispatch(width, height, depth);
    pCommandList->close();
    pNvrhiDevice->executeCommandList(pCommandList);
}

void ComputePass::execute(uint32_t width, uint32_t height, uint32_t depth)
{
    uint32_t threadGroupX = (width + mWorkGroupSizeX - 1) / mWorkGroupSizeX;
    uint32_t threadGroupY = (height + mWorkGroupSizeY - 1) / mWorkGroupSizeY;
    uint32_t threadGroupZ = (depth + mWorkGroupSizeZ - 1) / mWorkGroupSizeZ;

    LOG_TRACE("[ComputePass] Total threads: {}x{}x{}, Thread groups: {}x{}x{}", width, height, depth, threadGroupX, threadGroupY, threadGroupZ);
    dispatch(threadGroupX, threadGroupY, threadGroupZ);
}
