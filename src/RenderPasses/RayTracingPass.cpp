#include "RayTracingPass.h"
#include "Utils/Logger.h"

bool RayTracingPass::initialize(
    Device& device,
    const std::string& shaderPath,
    const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints,
    const std::unordered_map<std::string, nvrhi::ResourceHandle>& resourceMap,
    const std::unordered_map<std::string, nvrhi::rt::AccelStructHandle>& accelStructMap
)
{
    auto nvrhiDevice = device.getDevice();
    ShaderProgram program(nvrhiDevice, std::string(PROJECT_DIR) + shaderPath, entryPoints, "lib_6_3");
    // program.printReflectionInfo();

    if (!program.generateBindingLayout(resourceMap, accelStructMap))
        return false;
    std::vector<nvrhi::BindingLayoutItem> layoutItems = program.getBindingLayoutItems();
    std::vector<nvrhi::BindingSetItem> bindings = program.getBindingSetItems();

    // Create binding layout
    nvrhi::BindingLayoutDesc layoutDesc;
    layoutDesc.visibility = nvrhi::ShaderType::AllRayTracing;
    layoutDesc.bindings = layoutItems;
    m_BindingLayout = nvrhiDevice->createBindingLayout(layoutDesc);

    // Create binding set
    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings = bindings;
    m_BindingSet = nvrhiDevice->createBindingSet(bindingSetDesc, m_BindingLayout);

    // Create ray tracing pipeline with proper configuration
    nvrhi::rt::PipelineDesc pipelineDesc;
    pipelineDesc.addBindingLayout(m_BindingLayout);

    // Configure pipeline parameters - these are critical for D3D12
    pipelineDesc.maxPayloadSize = 32;   // Size in bytes for ray payload
    pipelineDesc.maxAttributeSize = 8;  // Size in bytes for hit attributes (typically 2 floats for barycentric coords)
    pipelineDesc.maxRecursionDepth = 1; // Maximum trace recursion depth

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

void RayTracingPass::dispatch(Device& device, uint32_t width, uint32_t height, uint32_t depth)
{
    auto commandList = device.getCommandList();
    auto nvrhiDevice = device.getDevice();
    commandList->open();

    // Set up resource states for all bound resources
    const nvrhi::BindingSetDesc& desc = *m_BindingSet->getDesc();
    for (const nvrhi::BindingSetItem& item : desc.bindings)
    {
        if (item.type == nvrhi::ResourceType::StructuredBuffer_SRV || item.type == nvrhi::ResourceType::StructuredBuffer_UAV)
        {
            nvrhi::IBuffer* buffer = dynamic_cast<nvrhi::IBuffer*>(item.resourceHandle);
            if (buffer)
                commandList->beginTrackingBufferState(
                    buffer,
                    item.type == nvrhi::ResourceType::StructuredBuffer_SRV ? nvrhi::ResourceStates::ShaderResource
                                                                           : nvrhi::ResourceStates::UnorderedAccess
                );
            else
                LOG_WARN("[RayTracingPass] WARNING: Resource handle for slot {} is not a buffer", item.slot);
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
                    item.type == nvrhi::ResourceType::Texture_SRV ? nvrhi::ResourceStates::ShaderResource : nvrhi::ResourceStates::UnorderedAccess
                );
            }
            else
                LOG_WARN("[RayTracingPass] WARNING: Resource handle for slot {} is not a texture", item.slot);
        }
        else if (item.type == nvrhi::ResourceType::ConstantBuffer)
        {
            nvrhi::IBuffer* buffer = dynamic_cast<nvrhi::IBuffer*>(item.resourceHandle);
            if (buffer)
                commandList->beginTrackingBufferState(buffer, nvrhi::ResourceStates::ConstantBuffer);
            else
                LOG_WARN("[RayTracingPass] WARNING: Resource handle for slot {} is not a constant buffer", item.slot);
        }
        else if (item.type == nvrhi::ResourceType::RayTracingAccelStruct)
        {
            // Ray tracing acceleration structures are automatically managed by NVRHI
            // No explicit state tracking needed for acceleration structures
            LOG_TRACE("[RayTracingPass] Acceleration structure resource found for slot {}", item.slot);
        }
        else
            LOG_WARN("[RayTracingPass] WARNING: Unsupported resource type for slot {}", item.slot);
    }

    // Commit all resource state transitions
    commandList->commitBarriers();

    commandList->setRayTracingState(m_rtState);
    nvrhi::rt::DispatchRaysArguments args;
    args.setDimensions(width, height, depth);
    commandList->dispatchRays(args);
    commandList->close();
    nvrhiDevice->executeCommandList(commandList);
}
