#include "RayTracingPass.h"
#include "Core/Program/Program.h"
#include "Utils/Logger.h"

RayTracingPass::RayTracingPass(
    ref<Device> pDevice,
    const std::string& shaderPath,
    const std::vector<std::pair<std::string, nvrhi::ShaderType>>& entryPoints,
    const std::vector<std::pair<std::string, std::string>>& defines
)
    : Pass(pDevice)
{
    auto pNvrhiDevice = pDevice->getDevice();
    std::string shaderVersion = getLatestLibVersion();

    std::unordered_map<std::string, nvrhi::ShaderType> entryPointMap;
    for (const auto& [name, type] : entryPoints)
        entryPointMap[name] = type;

    Program program(pNvrhiDevice, std::string(PROJECT_DIR) + shaderPath, entryPointMap, shaderVersion, defines);

    if (!program.generateBindingLayout())
        LOG_ERROR_RETURN("[RayTracingPass] Failed to generate binding layout from program");
    mpBindingSetManager = make_ref<BindingSetManager>(pDevice, program.getReflectionInfo());

    // Create ray tracing pipeline with proper configuration
    nvrhi::rt::PipelineDesc pipelineDesc;
    std::vector<nvrhi::BindingLayoutHandle> bindingLayouts = mpBindingSetManager->getBindingLayouts();
    for (const auto& pLayout : bindingLayouts)
        if (pLayout)
            pipelineDesc.addBindingLayout(pLayout);

    // Must match sizeof(ScatterRayData) in PathTracing.slang.
    pipelineDesc.maxPayloadSize = 88;
    pipelineDesc.maxAttributeSize = 8;
    pipelineDesc.maxRecursionDepth = 2;

    std::vector<std::string> missNames;

    for (const auto& [name, type] : entryPoints)
    {
        nvrhi::ShaderHandle shader = program.getShader(name);
        if (type == nvrhi::ShaderType::RayGeneration)
        {
            mRayGenShader = shader;
            pipelineDesc.addShader(nvrhi::rt::PipelineShaderDesc().setShader(shader).setExportName(name.c_str()));
        }
        else if (type == nvrhi::ShaderType::Miss)
        {
            mMissShaders.push_back(shader);
            missNames.push_back(name);
            pipelineDesc.addShader(nvrhi::rt::PipelineShaderDesc().setShader(shader).setExportName(name.c_str()));
        }
        else if (type == nvrhi::ShaderType::ClosestHit)
        {
            mClosestHitShader = shader;
        }
    }

    // Shadow rays reuse hit group 0 with RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
    // so no separate shadow hit group is needed.
    pipelineDesc.addHitGroup(
        nvrhi::rt::PipelineHitGroupDesc().setClosestHitShader(mClosestHitShader).setExportName("closestHitMain").setIsProceduralPrimitive(false)
    );

    LOG_DEBUG(
        "[RayTracingPass] Creating ray tracing pipeline with {} payload, {} attributes, {} recursion depth, {} miss shaders",
        pipelineDesc.maxPayloadSize,
        pipelineDesc.maxAttributeSize,
        pipelineDesc.maxRecursionDepth,
        mMissShaders.size()
    );
    mPipeline = pNvrhiDevice->createRayTracingPipeline(pipelineDesc);
    if (!mPipeline)
        LOG_ERROR_RETURN("[RayTracingPass] Failed to create ray tracing pipeline");
    LOG_DEBUG("[RayTracingPass] Ray tracing pipeline created successfully");

    // Create shader table with matching export names
    mShaderTable = mPipeline->createShaderTable();
    mShaderTable->setRayGenerationShader("rayGenMain");
    for (const auto& name : missNames)
        mShaderTable->addMissShader(name.c_str());
    mShaderTable->addHitGroup("closestHitMain");
}

void RayTracingPass::execute(uint32_t width, uint32_t height, uint32_t depth)
{
    nvrhi::rt::State rtState;
    rtState.setShaderTable(mShaderTable);
    std::vector<nvrhi::BindingSetHandle> bindingSets = mpBindingSetManager->getBindingSets();
    for (const auto& pBindingSet : bindingSets)
        if (pBindingSet)
            rtState.addBindingSet(pBindingSet);

    auto pCommandList = mpDevice->getCommandList();
    auto pNvrhiDevice = mpDevice->getDevice();
    pCommandList->open();

    // Push the latest CPU-side constant buffer contents immediately before dispatch.
    // This is how per-frame camera jitter reaches the shader without any explicit
    // "camera upload" call in the main loop.
    for (const auto& cbTex : mConstantBuffers)
        if (cbTex.buffer && cbTex.pData && cbTex.sizeBytes > 0)
            pCommandList->writeBuffer(cbTex.buffer, cbTex.pData, cbTex.sizeBytes);

    pCommandList->setRayTracingState(rtState);
    nvrhi::rt::DispatchRaysArguments args;
    args.setDimensions(width, height, depth);
    pCommandList->dispatchRays(args);
    pCommandList->close();
    pNvrhiDevice->executeCommandList(pCommandList);
}

std::string RayTracingPass::getLatestLibVersion()
{
    auto pD3D12Device = mpDevice->getD3D12Device();

    // First check if ray tracing is supported at all
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    HRESULT hr = pD3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));

    if (FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
        LOG_ERROR("[RayTracingPass] Ray tracing is not supported on this device");
        return "lib_6_3";
    }

    // Try shader models in descending order to find the highest supported one
    // Ray tracing library shaders start from lib_6_3 and go up
    const std::pair<D3D_SHADER_MODEL, std::string> shaderModels[] = {
        // { D3D_SHADER_MODEL_6_9, "lib_6_9" },
        // { D3D_SHADER_MODEL_6_8, "lib_6_8" },
        // { D3D_SHADER_MODEL_6_7, "lib_6_7" },
        {D3D_SHADER_MODEL_6_6, "lib_6_6"},
        {D3D_SHADER_MODEL_6_5, "lib_6_5"},
        {D3D_SHADER_MODEL_6_4, "lib_6_4"},
        {D3D_SHADER_MODEL_6_3, "lib_6_3"} // Minimum for ray tracing
    };

    for (const auto& [model, version] : shaderModels)
    {
        D3D12_FEATURE_DATA_SHADER_MODEL shaderModelData = {};
        shaderModelData.HighestShaderModel = model;

        hr = pD3D12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModelData, sizeof(shaderModelData));

        if (SUCCEEDED(hr) && shaderModelData.HighestShaderModel >= model)
            return version;
    }

    // Fallback to minimum ray tracing version
    LOG_WARN("[RayTracingPass] No compatible shader model detected, falling back to lib_6_3");
    return "lib_6_3";
}
