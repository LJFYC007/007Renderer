#include "ShaderProgram.h"

ShaderProgram::ShaderProgram() : m_ProgramLayout(nullptr) {}

void ShaderProgram::initializeForShaderType(nvrhi::ShaderType primaryShaderType)
{
    const char* profile;
    switch (primaryShaderType)
    {
    case nvrhi::ShaderType::Compute:
        profile = "cs_6_2";
        break;
    case nvrhi::ShaderType::AllRayTracing:
        profile = "lib_6_3";
        break;
    default:
        LOG_WARN("[ShaderProgram] Unknown shader type, using library profile");
        break;
    }
    slang::createGlobalSession(m_GlobalSession.writeRef());

#ifdef _DEBUG
    LOG_INFO("[ShaderProgram] Using DEBUG compilation options with profile: {}", profile);
    slang::CompilerOptionEntry debugOptions[] = {
        {slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_MAXIMAL, 0, nullptr, nullptr}},
        {slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE, 0, nullptr, nullptr}},
    };
#else
    LOG_INFO("[ShaderProgram] Using RELEASE compilation options with profile: {}", profile);
    slang::CompilerOptionEntry debugOptions[] = {
        {slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_MINIMAL, 0, nullptr, nullptr}},
        {slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_HIGH, 0, nullptr, nullptr}},
    };
#endif
    const int optionCount = 2;

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = m_GlobalSession->findProfile(profile);
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

bool ShaderProgram::loadFromFile(
    nvrhi::IDevice* device,
    const std::string& filePath,
    const std::vector<std::string>& entryPoints,
    nvrhi::ShaderType shaderType
)
{
    if (entryPoints.empty())
    {
        LOG_ERROR("[ShaderProgram] No entry points provided");
        return false;
    }

    // Initialize session for the specific shader type
    initializeForShaderType(shaderType);

    // Load module
    Slang::ComPtr<slang::IModule> module;
    module = m_Session->loadModule(filePath.c_str());
    if (!module)
    {
        LOG_ERROR("[Slang] Failed to load module: {}", filePath);
        return false;
    }

    // Create entry points and collect them
    std::vector<Slang::ComPtr<slang::IEntryPoint>> slangEntryPoints;
    slangEntryPoints.reserve(entryPoints.size());

    for (const std::string& entryPoint : entryPoints)
    {
        Slang::ComPtr<slang::IEntryPoint> slangEntryPoint;
        if (SLANG_FAILED(module->findEntryPointByName(entryPoint.c_str(), slangEntryPoint.writeRef())))
        {
            LOG_ERROR("[Slang] Failed to find entry point: {}", entryPoint);
            return false;
        }
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
    {
        LOG_ERROR("[Slang] Failed to create composite component type");
        return false;
    }

    // Link program
    if (SLANG_FAILED(program->link(m_LinkedProgram.writeRef())))
    {
        LOG_ERROR("[Slang] Failed to link program");
        return false;
    }

    // Get program layout with diagnostics
    Slang::ComPtr<slang::IBlob> diagnostics;
    m_ProgramLayout = m_LinkedProgram->getLayout(0, diagnostics.writeRef());
    if (diagnostics && diagnostics->getBufferSize() > 0)
        LOG_ERROR("[Slang] Program layout diagnostics: {}", (const char*)diagnostics->getBufferPointer());
    if (!m_ProgramLayout)
    {
        LOG_ERROR("[Slang] Failed to get program layout");
        return false;
    }

    // Create shaders for each entry point
    m_Shaders.clear();
    m_Shaders.reserve(entryPoints.size());
    m_EntryPointToShaderIndex.clear();

    for (size_t i = 0; i < entryPoints.size(); ++i)
    {
        Slang::ComPtr<slang::IBlob> kernelBlob;
        Slang::ComPtr<slang::IBlob> diagnosticBlob;

        if (SLANG_FAILED(m_LinkedProgram->getEntryPointCode(i, 0, kernelBlob.writeRef(), diagnosticBlob.writeRef())))
        {
            if (diagnosticBlob && diagnosticBlob->getBufferSize() > 0)
                LOG_ERROR("[Slang] Entry point diagnostics for {}: {}", entryPoints[i], (const char*)diagnosticBlob->getBufferPointer());
            LOG_ERROR("[Slang] Failed to get entry point code for {}", entryPoints[i]);
            return false;
        }

        LOG_DEBUG("[ShaderProgram] Compiled entry point {}: {} bytes", entryPoints[i], kernelBlob->getBufferSize());

        nvrhi::ShaderDesc desc;
        desc.entryName = entryPoints[i].c_str(); // Determine shader type based on entry point name
        nvrhi::ShaderType shaderType;
        if (entryPoints[i] == "rayGenMain")
            shaderType = nvrhi::ShaderType::RayGeneration;
        else if (entryPoints[i] == "missMain")
            shaderType = nvrhi::ShaderType::Miss;
        else if (entryPoints[i] == "closestHitMain")
            shaderType = nvrhi::ShaderType::ClosestHit;
        else if (entryPoints[i] == "anyHitMain")
            shaderType = nvrhi::ShaderType::AnyHit;
        else if (entryPoints[i] == "intersectionMain")
            shaderType = nvrhi::ShaderType::Intersection;
        else
        {
            LOG_WARN("[ShaderProgram] Unknown ray tracing entry point: {}, defaulting to RayGeneration", entryPoints[i]);
            shaderType = nvrhi::ShaderType::RayGeneration;
        }

        desc.shaderType = shaderType;

        auto shader = device->createShader(desc, kernelBlob->getBufferPointer(), kernelBlob->getBufferSize());
        if (!shader)
        {
            LOG_ERROR("[ShaderProgram] Failed to create shader for entry point: {}", entryPoints[i]);
            return false;
        }

        m_Shaders.push_back(shader);
        m_EntryPointToShaderIndex[entryPoints[i]] = i;
    }

    LOG_DEBUG("[ShaderProgram] Successfully loaded shader with {} entry points from: {}", entryPoints.size(), filePath);
    return true;
}

void ShaderProgram::printReflectionInfo() const
{
    if (!m_ProgramLayout)
    {
        LOG_DEBUG("[ShaderProgram] No program layout available for reflection");
        return;
    }
    LOG_DEBUG("[ShaderProgram] Printing shader reflection information");

    auto globalScopeLayout = m_ProgramLayout->getGlobalParamsVarLayout();
    if (globalScopeLayout)
    {
        LOG_DEBUG("[ShaderProgram] Global Parameters:");
        printVariableLayout(globalScopeLayout, 1);
    }

    auto entryPointCount = m_ProgramLayout->getEntryPointCount();
    for (unsigned int i = 0; i < entryPointCount; i++)
    {
        auto entryPoint = m_ProgramLayout->getEntryPointByIndex(i);
        LOG_DEBUG("[ShaderProgram] Entry Point {}: {}", i, entryPoint->getName());

        auto entryPointLayout = entryPoint->getVarLayout();
        if (entryPointLayout)
            printVariableLayout(entryPointLayout, 1);
    }
}

void ShaderProgram::printVariableLayout(slang::VariableLayoutReflection* varLayout, int indent) const
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
    LOG_DEBUG("{}Type: {}", indentStr, typeName ? typeName : "Unknown");

    if (!typeLayout)
    {
        LOG_WARN("{}No type layout available", indentStr);
        return;
    }
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

bool ShaderProgram::generateBindingLayout(
    std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
    std::vector<nvrhi::BindingSetItem>& outBindings,
    const std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>>& resourceMap
)
{
    if (!m_ProgramLayout)
    {
        LOG_ERROR("[ShaderProgram] No program layout available for binding generation");
        return false;
    }

    outLayoutItems.clear();
    outBindings.clear();

    // Process global parameters
    auto globalScopeLayout = m_ProgramLayout->getGlobalParamsVarLayout();
    if (globalScopeLayout)
    {
        if (!processParameterGroup(globalScopeLayout, outLayoutItems, outBindings, resourceMap))
            return false;
    }

    // Process entry point parameters
    auto entryPointCount = m_ProgramLayout->getEntryPointCount();
    for (unsigned int i = 0; i < entryPointCount; i++)
    {
        auto entryPoint = m_ProgramLayout->getEntryPointByIndex(i);
        auto entryPointLayout = entryPoint->getVarLayout();
        if (entryPointLayout)
        {
            if (!processParameterGroup(entryPointLayout, outLayoutItems, outBindings, resourceMap))
                return false;
        }
    }

    return true;
}

bool ShaderProgram::processParameterGroup(
    slang::VariableLayoutReflection* varLayout,
    std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
    std::vector<nvrhi::BindingSetItem>& outBindings,
    const std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>>& resourceMap
)
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
            if (!processParameter(fieldLayout, outLayoutItems, outBindings, resourceMap))
                return false;
        }
    }
    else
        return processParameter(varLayout, outLayoutItems, outBindings, resourceMap);

    return true;
}

bool ShaderProgram::processParameter(
    slang::VariableLayoutReflection* varLayout,
    std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
    std::vector<nvrhi::BindingSetItem>& outBindings,
    const std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>>& resourceMap
)
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
    // Get binding slot information - use appropriate parameter category based on resource type
    auto bindingSlot = ~0u;
    auto bindingSpace = varLayout->getOffset(slang::ParameterCategory::RegisterSpace);

    // Determine the correct parameter category based on resource type
    if (typeLayout->getKind() == slang::TypeReflection::Kind::Resource)
    {
        auto resourceAccess = typeLayout->getResourceAccess();
        if (resourceAccess == SLANG_RESOURCE_ACCESS_READ)
            bindingSlot = varLayout->getOffset(slang::ParameterCategory::ShaderResource);
        else if (resourceAccess == SLANG_RESOURCE_ACCESS_READ_WRITE)
            bindingSlot = varLayout->getOffset(slang::ParameterCategory::UnorderedAccess);
    }
    else if (typeLayout->getKind() == slang::TypeReflection::Kind::ConstantBuffer)
        bindingSlot = varLayout->getOffset(slang::ParameterCategory::ConstantBuffer);
    else if (typeLayout->getKind() == slang::TypeReflection::Kind::SamplerState)
        bindingSlot = varLayout->getOffset(slang::ParameterCategory::SamplerState);
    else
        bindingSlot = varLayout->getOffset(slang::ParameterCategory::DescriptorTableSlot);

    // If no binding slot is available, skip this parameter
    if (bindingSlot == ~0u)
    {
        LOG_WARN("[ShaderBinding] No binding slot for parameter: {}", paramNameStr);
        return true;
    }

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
                auto resourceIt = resourceMap.find(paramNameStr);
                if (resourceIt != resourceMap.end())
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
                auto resourceIt = resourceMap.find(paramNameStr);
                if (resourceIt != resourceMap.end())
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
                auto resourceIt = resourceMap.find(paramNameStr);
                if (resourceIt != resourceMap.end())
                {
                    nvrhi::rt::IAccelStruct* accelStruct = dynamic_cast<nvrhi::rt::IAccelStruct*>(resourceIt->second.Get());
                    if (accelStruct)
                    {
                        layoutItem = nvrhi::BindingLayoutItem::RayTracingAccelStruct(bindingSlot);
                        bindingItem = nvrhi::BindingSetItem::RayTracingAccelStruct(bindingSlot, accelStruct);
                    }
                    else
                    {
                        LOG_ERROR("[ShaderBinding] Failed to cast resource to IAccelStruct: {}", paramNameStr);
                        return false;
                    }
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
        auto resourceIt = resourceMap.find(paramNameStr);
        if (resourceIt != resourceMap.end())
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

    outLayoutItems.push_back(layoutItem);
    outBindings.push_back(bindingItem);
    return true;
}

nvrhi::ShaderHandle ShaderProgram::getShader(const std::string& entryPoint) const
{
    auto it = m_EntryPointToShaderIndex.find(entryPoint);
    if (it != m_EntryPointToShaderIndex.end())
        return m_Shaders[it->second];

    LOG_WARN("[ShaderProgram] Entry point '{}' not found", entryPoint);
    return nullptr;
}
