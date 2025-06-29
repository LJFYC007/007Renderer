#include "RayTracingPass.h"

bool RayTracingPass::initialize(
    nvrhi::IDevice* device,
    const std::string& shaderPath,
    const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints,
    const std::unordered_map<std::string, nvrhi::ResourceHandle>& resourceMap,
    const std::unordered_map<std::string, nvrhi::rt::AccelStructHandle>& accelStructMap
)
{
    ShaderProgram program(device, std::string(PROJECT_DIR) + shaderPath, entryPoints, "lib_6_3");

    m_RayGenShader = program.getShader("rayGenMain");
    m_MissShader = program.getShader("missMain");
    m_ClosestHitShader = program.getShader("closestHitMain");
    // program.printReflectionInfo();

    if (!program.generateBindingLayout(resourceMap, accelStructMap))
        return false;
    std::vector<nvrhi::BindingLayoutItem> layoutItems = program.getBindingLayoutItems();
    std::vector<nvrhi::BindingSetItem> bindings = program.getBindingSetItems();

    // Create binding layout
    nvrhi::BindingLayoutDesc layoutDesc;
    layoutDesc.visibility = nvrhi::ShaderType::AllRayTracing;
    layoutDesc.bindings = layoutItems;
    m_BindingLayout = device->createBindingLayout(layoutDesc);

    // Create binding set
    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings = bindings;
    m_BindingSet = device->createBindingSet(bindingSetDesc, m_BindingLayout);

    // Create ray tracing pipeline with proper configuration
    nvrhi::rt::PipelineDesc pipelineDesc;
    pipelineDesc.addBindingLayout(m_BindingLayout);

    // Configure pipeline parameters - these are critical for D3D12
    pipelineDesc.maxPayloadSize = 32;   // Size in bytes for ray payload
    pipelineDesc.maxAttributeSize = 8;  // Size in bytes for hit attributes (typically 2 floats for barycentric coords)
    pipelineDesc.maxRecursionDepth = 1; // Maximum trace recursion depth

    // Add shaders with correct export names matching the shader
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
    m_Pipeline = device->createRayTracingPipeline(pipelineDesc);
    if (!m_Pipeline)
    {
        LOG_ERROR("[RayTracingPass] Failed to create ray tracing pipeline");
        return false;
    }
    LOG_DEBUG("[RayTracingPass] Ray tracing pipeline created successfully");

    // Create shader table with matching export names
    m_ShaderTable = m_Pipeline->createShaderTable();
    m_ShaderTable->setRayGenerationShader("rayGenMain");
    m_ShaderTable->addMissShader("missMain");
    m_ShaderTable->addHitGroup("closestHitMain");

    m_rtState.setShaderTable(m_ShaderTable);
    m_rtState.addBindingSet(m_BindingSet);

    return true;
}

void RayTracingPass::dispatch(nvrhi::ICommandList* commandList, uint32_t width, uint32_t height, uint32_t depth)
{
    commandList->setRayTracingState(m_rtState);
    nvrhi::rt::DispatchRaysArguments args;
    args.setDimensions(width, height, depth);
    commandList->dispatchRays(args);
}