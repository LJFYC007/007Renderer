#include "RayTracingPass.h"
#include "Core/Program/Program.h"
#include "Utils/Logger.h"

RayTracingPass::RayTracingPass(
    ref<Device> pDevice,
    const std::string& shaderPath,
    const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints
)
    : Pass(pDevice)
{
    auto pNvrhiDevice = pDevice->getDevice();
    Program program(pNvrhiDevice, std::string(PROJECT_DIR) + shaderPath, entryPoints, "lib_6_3");
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

    // Configure pipeline parameters - these are critical for D3D12
    pipelineDesc.maxPayloadSize = 64;    // Size in bytes for ray payload
    pipelineDesc.maxAttributeSize = 8;   // Size in bytes for hit attributes (typically 2 floats for barycentric coords)
    pipelineDesc.maxRecursionDepth = 10; // Maximum trace recursion depth    // Add shaders with correct export names matching the shader
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
    pCommandList->setRayTracingState(rtState);
    nvrhi::rt::DispatchRaysArguments args;
    args.setDimensions(width, height, depth);
    pCommandList->dispatchRays(args);
    pCommandList->close();
    pNvrhiDevice->executeCommandList(pCommandList);
}
