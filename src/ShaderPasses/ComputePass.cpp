#include "ComputePass.h"
#include "Core/Program/Program.h"

ComputePass::ComputePass(ref<Device> pDevice, const std::string& shaderPath, const std::string& entryPoint) : Pass(pDevice)
{
    auto pNvrhiDevice = pDevice->getDevice();
    std::unordered_map<std::string, nvrhi::ShaderType> entryPoints;
    entryPoints[entryPoint] = nvrhi::ShaderType::Compute;

    std::string shaderVersion = getLatestComputeShaderVersion();
    Program program(pNvrhiDevice, std::string(PROJECT_DIR) + shaderPath, entryPoints, shaderVersion);
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
    } 
    
    // Create compute pipeline
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

std::string ComputePass::getLatestComputeShaderVersion()
{
    auto pD3D12Device = mpDevice->getD3D12Device();
    // Try shader models in descending order to find the highest supported one
    const std::pair<D3D_SHADER_MODEL, std::string> shaderModels[] = {
        { D3D_SHADER_MODEL_6_9, "cs_6_9" },
        { D3D_SHADER_MODEL_6_8, "cs_6_8" },
        { D3D_SHADER_MODEL_6_7, "cs_6_7" },
        { D3D_SHADER_MODEL_6_6, "cs_6_6" },
        { D3D_SHADER_MODEL_6_5, "cs_6_5" },
        { D3D_SHADER_MODEL_6_4, "cs_6_4" },
        { D3D_SHADER_MODEL_6_3, "cs_6_3" },
        { D3D_SHADER_MODEL_6_2, "cs_6_2" },
        { D3D_SHADER_MODEL_6_1, "cs_6_1" },
        { D3D_SHADER_MODEL_6_0, "cs_6_0" },
        { D3D_SHADER_MODEL_5_1, "cs_5_1" }
    };

    for (const auto& [model, version] : shaderModels)
    {
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModelData = {};
        shaderModelData.HighestShaderModel = model;
        
        HRESULT hr = pD3D12Device->CheckFeatureSupport(
            D3D12_FEATURE_SHADER_MODEL, 
            &shaderModelData, 
            sizeof(shaderModelData)
        );
        
        if (SUCCEEDED(hr) && shaderModelData.HighestShaderModel >= model)
            return version;
    }

    // Fallback to 6.2 if nothing is supported (shouldn't happen on modern GPUs)
    LOG_WARN("[ComputePass] No shader model detected, falling back to cs_6_2");
    return "cs_6_2";
}