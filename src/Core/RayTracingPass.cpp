#include "RayTracingPass.h"

bool RayTracingPass::initialize(
    nvrhi::IDevice* device,
    const std::string& shaderPath,
    const std::vector<std::string>& entryPoints,
    const std::unordered_map<std::string, nvrhi::ResourceHandle>& resourceMap
)
{
    ShaderProgram program;
    if (!program.loadFromFile(device, std::string(PROJECT_DIR) + shaderPath, entryPoints, nvrhi::ShaderType::AllRayTracing))
        return false;

    m_RayGenShader = program.getShader("rayGenMain");
    m_MissShader = program.getShader("missMain");
    m_ClosestHitShader = program.getShader("closestHitMain");
    // program.printReflectionInfo();

    std::vector<nvrhi::BindingLayoutItem> layoutItems;
    std::vector<nvrhi::BindingSetItem> bindings;
    if (!program.generateBindingLayout(layoutItems, bindings, resourceMap))
        return false;

    // Create binding layout
    nvrhi::BindingLayoutDesc layoutDesc;
    layoutDesc.visibility = nvrhi::ShaderType::AllRayTracing;
    layoutDesc.bindings = layoutItems;
    m_BindingLayout = device->createBindingLayout(layoutDesc);

    // Create binding set    // Create binding set
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

    // Validate shaders before creating pipeline
    if (!m_RayGenShader || !m_MissShader || !m_ClosestHitShader)
    {
        LOG_ERROR("[RayTracingPass] One or more shaders failed to load");
        return false;
    }

    LOG_INFO(
        "[RayTracingPass] Creating ray tracing pipeline with {} payload, {} attributes, {} recursion depth",
        pipelineDesc.maxPayloadSize,
        pipelineDesc.maxAttributeSize,
        pipelineDesc.maxRecursionDepth
    );

    m_Pipeline = device->createRayTracingPipeline(pipelineDesc);

    if (!m_Pipeline)
    {
        LOG_ERROR("[RayTracingPass] Failed to create ray tracing pipeline - this may indicate:");
        LOG_ERROR("  1. Shader compilation errors");
        LOG_ERROR("  2. Incorrect pipeline configuration");
        LOG_ERROR("  3. Driver/hardware compatibility issues");
        LOG_ERROR("  4. Export name mismatches");
        return false;
    }

    LOG_INFO("[RayTracingPass] Ray tracing pipeline created successfully");
    return m_Pipeline != nullptr;
}

void RayTracingPass::dispatch(nvrhi::ICommandList* commandList, uint32_t width, uint32_t height, uint32_t depth)
{
    if (!m_Pipeline || !m_BindingSet)
        return;

    // Create shader table with matching export names
    auto shaderTable = m_Pipeline->createShaderTable();
    shaderTable->setRayGenerationShader("rayGenMain");
    shaderTable->addMissShader("missMain");
    shaderTable->addHitGroup("closestHitMain");

    nvrhi::rt::State rtState;
    rtState.setShaderTable(shaderTable);
    rtState.addBindingSet(m_BindingSet);

    commandList->setRayTracingState(rtState);

    nvrhi::rt::DispatchRaysArguments args;
    args.setDimensions(width, height, depth);

    commandList->dispatchRays(args);
}