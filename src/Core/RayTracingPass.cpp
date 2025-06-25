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

    m_RayGenShader = program.getShader("raygeneration");
    m_MissShader = program.getShader("miss");
    m_ClosestHitShader = program.getShader("closesthit");
    program.printReflectionInfo();

    std::vector<nvrhi::BindingLayoutItem> layoutItems;
    std::vector<nvrhi::BindingSetItem> bindings;
    if (!program.generateBindingLayout(layoutItems, bindings, resourceMap))
        return false;

    // Create binding layout
    nvrhi::BindingLayoutDesc layoutDesc;
    layoutDesc.visibility = nvrhi::ShaderType::AllRayTracing;
    layoutDesc.bindings = layoutItems;
    m_BindingLayout = device->createBindingLayout(layoutDesc);

    // Create binding set
    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings = bindings;
    m_BindingSet = device->createBindingSet(bindingSetDesc, m_BindingLayout);

    // Create ray tracing pipeline
    nvrhi::rt::PipelineDesc pipelineDesc;
    pipelineDesc.addBindingLayout(m_BindingLayout);
    // Add shaders to pipeline
    pipelineDesc.addShader(nvrhi::rt::PipelineShaderDesc().setShader(m_RayGenShader).setExportName("raygeneration"));
    pipelineDesc.addShader(nvrhi::rt::PipelineShaderDesc().setShader(m_MissShader).setExportName("miss"));
    pipelineDesc.addHitGroup(nvrhi::rt::PipelineHitGroupDesc().setClosestHitShader(m_ClosestHitShader).setExportName("closesthit"));

    m_Pipeline = device->createRayTracingPipeline(pipelineDesc);
    return m_Pipeline != nullptr;
}

void RayTracingPass::dispatch(nvrhi::ICommandList* commandList, uint32_t width, uint32_t height, uint32_t depth)
{
    if (!m_Pipeline || !m_BindingSet)
        return;

    // Create shader table
    auto shaderTable = m_Pipeline->createShaderTable();
    shaderTable->setRayGenerationShader("raygeneration");
    shaderTable->addMissShader("miss");
    shaderTable->addHitGroup("closesthit");

    nvrhi::rt::State rtState;
    rtState.setShaderTable(shaderTable);
    rtState.addBindingSet(m_BindingSet);

    commandList->setRayTracingState(rtState);

    nvrhi::rt::DispatchRaysArguments args;
    args.setDimensions(width, height, depth);

    commandList->dispatchRays(args);
}