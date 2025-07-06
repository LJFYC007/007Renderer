#include "RayTracingPass.h"
#include "Core/Program/Program.h"
#include "Utils/Logger.h"

RayTracingPass::RayTracingPass(
    ref<Device> device,
    const std::string& shaderPath,
    const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints
)
    : Pass(device)
{
    auto nvrhiDevice = device->getDevice();
    Program program(nvrhiDevice, std::string(PROJECT_DIR) + shaderPath, entryPoints, "lib_6_3");
    // program.printReflectionInfo();

    if (!program.generateBindingLayout())
        LOG_ERROR_RETURN("[RayTracingPass] Failed to generate binding layout from program");
    m_BindingSetManager = make_ref<BindingSetManager>(device, program.getReflectionInfo());

    // Create ray tracing pipeline with proper configuration
    nvrhi::rt::PipelineDesc pipelineDesc;
    std::vector<nvrhi::BindingLayoutHandle> bindingLayouts = m_BindingSetManager->getBindingLayouts();
    for (const auto& layout : bindingLayouts)
        if (layout)
            pipelineDesc.addBindingLayout(layout);

    // Configure pipeline parameters - these are critical for D3D12
    pipelineDesc.maxPayloadSize = 64;   // Size in bytes for ray payload
    pipelineDesc.maxAttributeSize = 8;  // Size in bytes for hit attributes (typically 2 floats for barycentric coords)
    pipelineDesc.maxRecursionDepth = 5; // Maximum trace recursion depth

    // Add shaders with correct export names matching the shader
    m_RayGenShader = program.getShader("rayGenMain");
    m_MissShader = program.getShader("missMain");
    m_ClosestHitShader = program.getShader("closestHitMain");
    pipelineDesc.addShader(nvrhi::rt::PipelineShaderDesc().setShader(m_RayGenShader).setExportName("rayGenMain"));
    pipelineDesc.addShader(nvrhi::rt::PipelineShaderDesc().setShader(m_MissShader).setExportName("missMain"));
    pipelineDesc.addHitGroup(
        nvrhi::rt::PipelineHitGroupDesc().setClosestHitShader(m_ClosestHitShader).setExportName("closestHitMain").setIsProceduralPrimitive(false)
    ); // Set to true if using intersection shaders

    LOG_DEBUG(
        "[RayTracingPass] Creating ray tracing pipeline with {} payload, {} attributes, {} recursion depth",
        pipelineDesc.maxPayloadSize,
        pipelineDesc.maxAttributeSize,
        pipelineDesc.maxRecursionDepth
    );
    m_Pipeline = nvrhiDevice->createRayTracingPipeline(pipelineDesc);
    if (!m_Pipeline)
        LOG_ERROR_RETURN("[RayTracingPass] Failed to create ray tracing pipeline");
    LOG_DEBUG("[RayTracingPass] Ray tracing pipeline created successfully");

    // Create shader table with matching export names
    m_ShaderTable = m_Pipeline->createShaderTable();
    m_ShaderTable->setRayGenerationShader("rayGenMain");
    m_ShaderTable->addMissShader("missMain");
    m_ShaderTable->addHitGroup("closestHitMain");
}

void RayTracingPass::execute(uint32_t width, uint32_t height, uint32_t depth)
{
    nvrhi::rt::State m_rtState;
    m_rtState.setShaderTable(m_ShaderTable);
    std::vector<nvrhi::BindingSetHandle> bindingSets = m_BindingSetManager->getBindingSets();
    for (const auto& bindingSet : bindingSets)
        if (bindingSet)
            m_rtState.addBindingSet(bindingSet);

    auto commandList = m_Device->getCommandList();
    auto nvrhiDevice = m_Device->getDevice();
    commandList->open();
    trackingResourceState(commandList);

    commandList->setRayTracingState(m_rtState);
    nvrhi::rt::DispatchRaysArguments args;
    args.setDimensions(width, height, depth);
    commandList->dispatchRays(args);
    commandList->close();
    nvrhiDevice->executeCommandList(commandList);
}
