#include "Program.h"

void Program::initializeSession(const std::string& profile)
{
    slang::createGlobalSession(m_GlobalSession.writeRef());

#ifdef _DEBUG
    LOG_INFO("[Program] Using DEBUG compilation options with profile: {}", profile);
    slang::CompilerOptionEntry debugOptions[] = {
        {slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_MAXIMAL, 0, nullptr, nullptr}},
        {slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE, 0, nullptr, nullptr}},
    };
#else
    LOG_INFO("[Program] Using RELEASE compilation options with profile: {}", profile);
    slang::CompilerOptionEntry debugOptions[] = {
        {slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_MINIMAL, 0, nullptr, nullptr}},
        {slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_HIGH, 0, nullptr, nullptr}},
    };
#endif
    const int optionCount = 2;

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = m_GlobalSession->findProfile(profile.c_str());
    targetDesc.compilerOptionEntries = debugOptions;
    targetDesc.compilerOptionEntryCount = optionCount;

    const char* searchPaths[] = {PROJECT_SHADER_DIR, PROJECT_SRC_DIR};

    slang::SessionDesc sessionDesc = {};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.searchPaths = searchPaths;
    sessionDesc.searchPathCount = 2;

    auto sessionResult = m_GlobalSession->createSession(sessionDesc, m_Session.writeRef());
    if (SLANG_FAILED(sessionResult))
        LOG_ERROR("[Slang] Failed to create session with result: {}", sessionResult);
}

Program::Program(
    nvrhi::IDevice* device,
    const std::string& filePath,
    const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints,
    const std::string& profile
)
{
    if (entryPoints.empty())
        LOG_ERROR_RETURN("[Program] No entry points provided");
    initializeSession(profile);

    // Load module
    Slang::ComPtr<slang::IModule> module;
    module = m_Session->loadModule(filePath.c_str());
    if (!module)
        LOG_ERROR_RETURN("[Slang] Failed to load module: {}", filePath);

    // Create entry points and collect them
    std::vector<Slang::ComPtr<slang::IEntryPoint>> slangEntryPoints;
    slangEntryPoints.reserve(entryPoints.size());

    for (const std::pair<std::string, nvrhi::ShaderType>& entryPoint : entryPoints)
    {
        Slang::ComPtr<slang::IEntryPoint> slangEntryPoint;
        if (SLANG_FAILED(module->findEntryPointByName(entryPoint.first.c_str(), slangEntryPoint.writeRef())))
            LOG_ERROR_RETURN("[Slang] Failed to find entry point: {}", entryPoint.first);
        slangEntryPoints.push_back(slangEntryPoint);
    }

    // Create composite component type with module and all entry points
    std::vector<slang::IComponentType*> components;
    components.reserve(1 + slangEntryPoints.size());
    components.push_back(module);
    for (auto& entryPoint : slangEntryPoints)
        components.push_back(entryPoint);

    Slang::ComPtr<slang::IComponentType> program;
    if (SLANG_FAILED(m_Session->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()), program.writeRef())))
        LOG_ERROR_RETURN("[Slang] Failed to create composite component type");

    // Link program
    if (SLANG_FAILED(program->link(m_LinkedProgram.writeRef())))
        LOG_ERROR_RETURN("[Slang] Failed to link program");

    // Get program layout with diagnostics
    Slang::ComPtr<slang::IBlob> diagnostics;
    m_ProgramLayout = m_LinkedProgram->getLayout(0, diagnostics.writeRef());
    if (!m_ProgramLayout)
    {
        if (diagnostics && diagnostics->getBufferSize() > 0)
            LOG_ERROR("[Slang] Program layout diagnostics: {}", (const char*)diagnostics->getBufferPointer());
        LOG_ERROR_RETURN("[Slang] Failed to get program layout");
    }

    // Create shaders for each entry point
    m_Shaders.clear();
    m_Shaders.reserve(entryPoints.size());
    m_EntryPointToShaderIndex.clear();

    uint32_t entryPointIndex = 0;
    for (const auto& entryPoint : entryPoints)
    {
        const auto& entryPointName = entryPoint.first;
        const auto& entryPointType = entryPoint.second;

        Slang::ComPtr<slang::IBlob> kernelBlob;
        Slang::ComPtr<slang::IBlob> diagnosticBlob;

        if (SLANG_FAILED(m_LinkedProgram->getEntryPointCode(entryPointIndex, 0, kernelBlob.writeRef(), diagnosticBlob.writeRef())))
        {
            if (diagnosticBlob && diagnosticBlob->getBufferSize() > 0)
                LOG_ERROR("[Slang] Entry point diagnostics for {}: {}", entryPointName, (const char*)diagnosticBlob->getBufferPointer());
            LOG_ERROR_RETURN("[Slang] Failed to get entry point code for {}", entryPointName);
        }

        LOG_DEBUG("[Program] Compiled entry point {}: {} bytes", entryPointName, kernelBlob->getBufferSize());

        nvrhi::ShaderDesc desc;
        desc.entryName = entryPointName.c_str(); // Determine shader type based on entry point name
        desc.shaderType = entryPointType;
        auto shader = device->createShader(desc, kernelBlob->getBufferPointer(), kernelBlob->getBufferSize());
        if (!shader)
            LOG_ERROR_RETURN("[Program] Failed to create shader for entry point: {}", entryPointName);

        m_Shaders.push_back(shader);
        m_EntryPointToShaderIndex[entryPointName] = entryPointIndex;
        entryPointIndex++;
    }

    LOG_DEBUG("[Program] Successfully loaded shader with {} entry points from: {}", entryPoints.size(), filePath);
}

void Program::printReflectionInfo() const
{
    if (!m_ProgramLayout)
        LOG_DEBUG_RETURN("[Program] No program layout available for reflection");
    LOG_DEBUG("[Program] Printing shader reflection information");

    auto globalScopeLayout = m_ProgramLayout->getGlobalParamsVarLayout();
    if (globalScopeLayout)
    {
        LOG_DEBUG("[Program] Global Parameters:");
        printVariableLayout(globalScopeLayout, 1);
    }

    auto entryPointCount = m_ProgramLayout->getEntryPointCount();
    for (unsigned int i = 0; i < entryPointCount; i++)
    {
        auto entryPoint = m_ProgramLayout->getEntryPointByIndex(i);
        LOG_DEBUG("[Program] Entry Point {}: {}", i, entryPoint->getName());

        auto entryPointLayout = entryPoint->getVarLayout();
        if (entryPointLayout)
            printVariableLayout(entryPointLayout, 1);
    }
}

void Program::printVariableLayout(slang::VariableLayoutReflection* varLayout, int indent) const
{
    if (!varLayout)
        return;
    std::string indentStr(indent * 2, ' ');
    auto typeLayout = varLayout->getTypeLayout();

    if (varLayout->getName() != nullptr)
        LOG_DEBUG("{}Variable: {}", indentStr, varLayout->getName());

    const char* typeName = nullptr;
    if (typeLayout)
        typeName = typeLayout->getName();
    else
        LOG_WARN_RETURN("{}No type layout available for variable '{}'", indentStr, varLayout->getName() ? varLayout->getName() : "Unknown");
    LOG_DEBUG("{}Type: {}", indentStr, typeName ? typeName : "Unknown");

    switch (typeLayout->getKind())
    {
    case slang::TypeReflection::Kind::Struct:
        LOG_DEBUG("{}Kind: Struct", indentStr);
        {
            auto fieldCount = typeLayout->getFieldCount();
            for (unsigned int i = 0; i < fieldCount; i++)
            {
                auto fieldLayout = typeLayout->getFieldByIndex(i);
                printVariableLayout(fieldLayout, indent + 1);
            }
        }
        break;

    case slang::TypeReflection::Kind::Array:
        LOG_DEBUG("{}Kind: Array[{}]", indentStr, typeLayout->getElementCount());
        break;

    case slang::TypeReflection::Kind::Vector:
        LOG_DEBUG("{}Kind: Vector[{}]", indentStr, typeLayout->getElementCount());
        break;

    case slang::TypeReflection::Kind::Matrix:
        LOG_DEBUG("{}Kind: Matrix[{}x{}]", indentStr, typeLayout->getRowCount(), typeLayout->getColumnCount());
        break;

    case slang::TypeReflection::Kind::Scalar:
        LOG_DEBUG("{}Kind: Scalar", indentStr);
        break;

    case slang::TypeReflection::Kind::Resource:
    {
        LOG_DEBUG("{}Kind: Resource", indentStr);
        auto resourceShape = typeLayout->getResourceShape();
        auto resourceAccess = typeLayout->getResourceAccess();
        LOG_DEBUG("{}  Resource Shape: {}", indentStr, static_cast<int>(resourceShape));
        LOG_DEBUG("{}  Resource Access: {}", indentStr, static_cast<int>(resourceAccess));
    }
    break;

    case slang::TypeReflection::Kind::ConstantBuffer:
        LOG_DEBUG("{}Kind: ConstantBuffer", indentStr);
        break;

    case slang::TypeReflection::Kind::ParameterBlock:
        LOG_DEBUG("{}Kind: ParameterBlock", indentStr);
        break;

    default:
        LOG_WARN("{}Kind: Unknown type kind ({})", indentStr, static_cast<int>(typeLayout->getKind()));
        break;
    }

    auto offset = varLayout->getOffset(slang::ParameterCategory::Uniform);
    if (offset != ~0u)
        LOG_DEBUG("{}  Uniform Offset: {}", indentStr, offset);

    auto bindingOffset = varLayout->getOffset(slang::ParameterCategory::DescriptorTableSlot);
    if (bindingOffset != ~0u)
        LOG_DEBUG("{}  Binding Offset: {}", indentStr, bindingOffset);
}

bool Program::generateBindingLayout(
    const std::unordered_map<std::string, nvrhi::ResourceHandle>& resourceMap,
    const std::unordered_map<std::string, nvrhi::rt::AccelStructHandle>& accelStructMap
)
{
    if (!m_ProgramLayout)
    {
        LOG_ERROR("[Program] No program layout available for binding generation");
        return false;
    }

    m_BindingLayoutItems.clear();
    m_BindingSetItems.clear();
    m_ResourceMap = resourceMap;
    m_AccelStructMap = accelStructMap;

    // Process global parameters
    auto globalScopeLayout = m_ProgramLayout->getGlobalParamsVarLayout();
    if (!processParameterGroup(globalScopeLayout))
        return false;

    // Process entry point parameters
    auto entryPointCount = m_ProgramLayout->getEntryPointCount();
    for (unsigned int i = 0; i < entryPointCount; i++)
    {
        auto entryPoint = m_ProgramLayout->getEntryPointByIndex(i);
        auto entryPointLayout = entryPoint->getVarLayout();
        if (!processParameterGroup(entryPointLayout))
            return false;
    }
    return true;
}

bool Program::processParameterGroup(slang::VariableLayoutReflection* varLayout)
{
    if (!varLayout)
        return true;
    auto typeLayout = varLayout->getTypeLayout();
    if (!typeLayout)
        return true;

    // Handle struct types (parameter groups)
    if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
    {
        auto fieldCount = typeLayout->getFieldCount();
        for (unsigned int i = 0; i < fieldCount; i++)
        {
            auto fieldLayout = typeLayout->getFieldByIndex(i);
            if (!processParameter(fieldLayout))
                return false;
        }
    }
    else
        return processParameter(varLayout);
    return true;
}

bool Program::processParameter(slang::VariableLayoutReflection* varLayout)
{
    if (!varLayout)
        return true;
    auto typeLayout = varLayout->getTypeLayout();
    if (!typeLayout)
        return true;

    const char* paramName = varLayout->getName();
    if (!paramName)
        return true;
    std::string paramNameStr(paramName);
    auto bindingSlot = varLayout->getBindingIndex();

    // Create binding layout item based on resource type
    nvrhi::BindingLayoutItem layoutItem;
    nvrhi::BindingSetItem bindingItem;

    switch (typeLayout->getKind())
    {
    case slang::TypeReflection::Kind::Resource:
    {
        auto resourceShape = typeLayout->getResourceShape();
        auto resourceAccess = typeLayout->getResourceAccess();
        switch (resourceShape)
        {
        case SLANG_TEXTURE_1D:
        case SLANG_TEXTURE_2D:
        case SLANG_TEXTURE_3D:
        case SLANG_TEXTURE_CUBE:
        case SLANG_TEXTURE_BUFFER:
            if (resourceAccess == SLANG_RESOURCE_ACCESS_READ || resourceAccess == SLANG_RESOURCE_ACCESS_READ_WRITE)
            {
                auto resourceIt = m_ResourceMap.find(paramNameStr);
                if (resourceIt != m_ResourceMap.end())
                {
                    nvrhi::ITexture* texture = dynamic_cast<nvrhi::ITexture*>(resourceIt->second.Get());
                    nvrhi::TextureHandle textureHandle = texture;
                    layoutItem = resourceAccess == SLANG_RESOURCE_ACCESS_READ ? nvrhi::BindingLayoutItem::Texture_SRV(bindingSlot)
                                                                              : nvrhi::BindingLayoutItem::Texture_UAV(bindingSlot);
                    bindingItem = resourceAccess == SLANG_RESOURCE_ACCESS_READ ? nvrhi::BindingSetItem::Texture_SRV(bindingSlot, textureHandle)
                                                                               : nvrhi::BindingSetItem::Texture_UAV(bindingSlot, textureHandle);
                }
                else
                    LOG_WARN("[ShaderBinding] Texture resource not found in map: {}", paramNameStr);

                LOG_DEBUG(
                    "[ShaderBinding] Found {} texture: {} at slot {}",
                    resourceAccess == SLANG_RESOURCE_ACCESS_READ ? "SRV" : "UAV",
                    paramNameStr,
                    bindingSlot
                );
            }
            break;

        case SLANG_STRUCTURED_BUFFER:
            if (resourceAccess == SLANG_RESOURCE_ACCESS_READ || resourceAccess == SLANG_RESOURCE_ACCESS_READ_WRITE)
            {
                auto resourceIt = m_ResourceMap.find(paramNameStr);
                if (resourceIt != m_ResourceMap.end())
                {
                    nvrhi::IBuffer* buffer = dynamic_cast<nvrhi::IBuffer*>(resourceIt->second.Get());
                    nvrhi::BufferHandle bufferHandle = buffer;
                    layoutItem = resourceAccess == SLANG_RESOURCE_ACCESS_READ ? nvrhi::BindingLayoutItem::StructuredBuffer_SRV(bindingSlot)
                                                                              : nvrhi::BindingLayoutItem::StructuredBuffer_UAV(bindingSlot);
                    bindingItem = resourceAccess == SLANG_RESOURCE_ACCESS_READ
                                      ? nvrhi::BindingSetItem::StructuredBuffer_SRV(bindingSlot, bufferHandle)
                                      : nvrhi::BindingSetItem::StructuredBuffer_UAV(bindingSlot, bufferHandle);
                }
                else
                    LOG_WARN("[ShaderBinding] Buffer resource not found in map: {}", paramNameStr);

                LOG_DEBUG(
                    "[ShaderBinding] Found {} structured buffer: {} at slot {}",
                    resourceAccess == SLANG_RESOURCE_ACCESS_READ ? "SRV" : "UAV",
                    paramNameStr,
                    bindingSlot
                );
            }
            break;
        case SLANG_ACCELERATION_STRUCTURE:
            if (resourceAccess == SLANG_RESOURCE_ACCESS_READ || resourceAccess == SLANG_RESOURCE_ACCESS_READ_WRITE)
            {
                auto accelStructIt = m_AccelStructMap.find(paramNameStr);
                if (accelStructIt != m_AccelStructMap.end())
                {
                    layoutItem = nvrhi::BindingLayoutItem::RayTracingAccelStruct(bindingSlot);
                    bindingItem = nvrhi::BindingSetItem::RayTracingAccelStruct(bindingSlot, accelStructIt->second);
                }
                else
                    LOG_WARN("[ShaderBinding] Acceleration structure resource not found in map: {}", paramNameStr);

                LOG_DEBUG("[ShaderBinding] Found ray tracing acceleration structure: {} at slot {}", paramNameStr, bindingSlot);
            }
            break;

        default:
            LOG_WARN("[ShaderBinding] Unknown resource shape for: {} (shape: {})", paramNameStr, static_cast<int>(resourceShape));
            return true;
        }
        break;
    }

    case slang::TypeReflection::Kind::ConstantBuffer:
    {
        auto resourceIt = m_ResourceMap.find(paramNameStr);
        if (resourceIt != m_ResourceMap.end())
        {
            nvrhi::IBuffer* buffer = dynamic_cast<nvrhi::IBuffer*>(resourceIt->second.Get());
            nvrhi::BufferHandle bufferHandle = buffer;
            layoutItem = nvrhi::BindingLayoutItem::ConstantBuffer(bindingSlot);
            bindingItem = nvrhi::BindingSetItem::ConstantBuffer(bindingSlot, bufferHandle);
        }
        else
            LOG_WARN("[ShaderBinding] Constant buffer resource not found in map: {}", paramNameStr);
        LOG_DEBUG("[ShaderBinding] Found constant buffer: {} at slot {}", paramNameStr, bindingSlot);
        break;
    }

    default:
        return true;
    }

    m_BindingLayoutItems[paramNameStr] = layoutItem;
    m_BindingSetItems[paramNameStr] = bindingItem;
    return true;
}

nvrhi::ShaderHandle Program::getShader(const std::string& entryPoint) const
{
    auto it = m_EntryPointToShaderIndex.find(entryPoint);
    if (it != m_EntryPointToShaderIndex.end())
        return m_Shaders[it->second];

    LOG_WARN("[Program] Entry point '{}' not found", entryPoint);
    return nullptr;
}
