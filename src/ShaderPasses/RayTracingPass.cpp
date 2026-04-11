#include "RayTracingPass.h"
#include "Core/Program/Program.h"
#include "Utils/Logger.h"

RayTracingPass::RayTracingPass(
    ref<Device> pDevice,
    const std::string& shaderPath,
    const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints,
    const std::vector<std::pair<std::string, std::string>>& defines
)
    : Pass(pDevice)
{
    auto pNvrhiDevice = pDevice->getDevice();
    std::string shaderVersion = getLatestLibVersion();
    Program program(pNvrhiDevice, std::string(PROJECT_DIR) + shaderPath, entryPoints, shaderVersion, defines);
    // program.printReflectionInfo();

    if (!program.generateBindingLayout())
        LOG_ERROR_RETURN("[RayTracingPass] Failed to generate binding layout from program");
    mpBindingSetManager = make_ref<BindingSetManager>(pDevice, program.getReflectionInfo());

    // Create ray tracing pipeline with proper configuration
    nvrhi::rt::PipelineDesc pipelineDesc;
    std::vector<nvrhi::BindingLayoutHandle> bindingLayouts = mpBindingSetManager->getBindingLayouts();
    for (const auto& pLayout : bindingLayouts)
        if (pLayout)
            pipelineDesc.addBindingLayout(pLayout);

    // ScatterRayData layout: float3 radiance(12) + bool terminated(4) + float3 thp(12) + uint pathLength(4)
    //                        + float3 origin(12) + float3 direction(12) + TinyUniformSampleGenerator(16) = 72 bytes
    // Bounce loop lives in rayGenMain, so recursion depth is always 1.
    pipelineDesc.maxPayloadSize = 72;
    pipelineDesc.maxAttributeSize = 8;
    pipelineDesc.maxRecursionDepth = 1;

    // Add shaders with correct export names matching the shader
    mRayGenShader = program.getShader("rayGenMain");
    mMissShader = program.getShader("missMain");
    mClosestHitShader = program.getShader("closestHitMain");
    pipelineDesc.addShader(nvrhi::rt::PipelineShaderDesc().setShader(mRayGenShader).setExportName("rayGenMain"));
    pipelineDesc.addShader(nvrhi::rt::PipelineShaderDesc().setShader(mMissShader).setExportName("missMain"));
    pipelineDesc.addHitGroup(
        nvrhi::rt::PipelineHitGroupDesc().setClosestHitShader(mClosestHitShader).setExportName("closestHitMain").setIsProceduralPrimitive(false)
    ); // Set to true if using intersection shaders

    LOG_DEBUG(
        "[RayTracingPass] Creating ray tracing pipeline with {} payload, {} attributes, {} recursion depth",
        pipelineDesc.maxPayloadSize,
        pipelineDesc.maxAttributeSize,
        pipelineDesc.maxRecursionDepth
    );
    mPipeline = pNvrhiDevice->createRayTracingPipeline(pipelineDesc);
    if (!mPipeline)
        LOG_ERROR_RETURN("[RayTracingPass] Failed to create ray tracing pipeline");
    LOG_DEBUG("[RayTracingPass] Ray tracing pipeline created successfully");

    // Create shader table with matching export names
    mShaderTable = mPipeline->createShaderTable();
    mShaderTable->setRayGenerationShader("rayGenMain");
    mShaderTable->addMissShader("missMain");
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
