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
        LOG_DEBUG("[Program] Global Scope:");
        printScope(globalScopeLayout, 1);
    }

    auto entryPointCount = m_ProgramLayout->getEntryPointCount();
    for (unsigned int i = 0; i < entryPointCount; i++)
    {
        auto entryPoint = m_ProgramLayout->getEntryPointByIndex(i);
        LOG_DEBUG("[Program] Entry Point {}: {}", i, entryPoint->getName());

        auto entryPointLayout = entryPoint->getVarLayout();
        if (entryPointLayout)
            printScope(entryPointLayout, 1);
    }
}

void Program::printScope(slang::VariableLayoutReflection* scopeVarLayout, int indent) const
{
    std::string indentStr(indent * 2, ' ');
    auto scopeTypeLayout = scopeVarLayout->getTypeLayout();
    switch (scopeTypeLayout->getKind())
    {
    case slang::TypeReflection::Kind::Struct:
    {
        LOG_DEBUG("{}parameters:", indentStr);
        int paramCount = scopeTypeLayout->getFieldCount();
        for (int i = 0; i < paramCount; i++)
        {
            auto param = scopeTypeLayout->getFieldByIndex(i);
            printVarLayout(param, indent + 1);
        }
    }
    break;

    case slang::TypeReflection::Kind::ConstantBuffer:
    {
        LOG_DEBUG("{}automatically-introduced constant buffer:", indentStr);
        printOffsets(scopeTypeLayout->getContainerVarLayout(), indent + 1);
        printScope(scopeTypeLayout->getElementVarLayout(), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::ParameterBlock:
    {
        LOG_DEBUG("automatically-introduced parameter block:", indentStr);
        printOffsets(scopeTypeLayout->getContainerVarLayout(), indent + 1);
        printScope(scopeTypeLayout->getElementVarLayout(), indent + 1);
    }
    break;

    default:
        LOG_WARN("[Program] Unsupported scope type kind for printing: {}", static_cast<int>(scopeTypeLayout->getKind()));
        break;
    }
}

void Program::printVarLayout(slang::VariableLayoutReflection* varLayout, int indent) const
{
    std::string indentStr(indent * 2, ' ');
    LOG_DEBUG("{}name: {}", indentStr, varLayout->getName());
    printRelativeOffsets(varLayout, indent + 1);
    LOG_DEBUG("{}type layout:", indentStr);
    printTypeLayout(varLayout->getTypeLayout(), indent + 1);
}

void Program::printTypeLayout(slang::TypeLayoutReflection* typeLayout, int indent) const
{
    std::string indentStr(indent * 2, ' ');
    LOG_DEBUG("{}name: {}", indentStr, typeLayout->getName() ? typeLayout->getName() : "None");
    LOG_DEBUG("{}kind: {}", indentStr, printKind(typeLayout->getKind()));
    printSizes(typeLayout, indent + 1);

    switch (typeLayout->getKind())
    {
    case slang::TypeReflection::Kind::Struct:
    {
        LOG_DEBUG("{}fields:", indentStr);
        int fieldCount = typeLayout->getFieldCount();
        for (int i = 0; i < fieldCount; i++)
        {
            auto field = typeLayout->getFieldByIndex(i);
            printVarLayout(field, indent + 1);
        }
    }
    break;

    case slang::TypeReflection::Kind::Array:
    {
        LOG_DEBUG("{}element count: {}", indentStr, typeLayout->getElementCount());
        LOG_DEBUG("{}element type layout: ", indentStr);
        printTypeLayout(typeLayout->getElementTypeLayout(), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::Vector:
    {
        LOG_DEBUG("{}element type layout: ", indentStr);
        printTypeLayout(typeLayout->getElementTypeLayout(), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::ConstantBuffer:
    case slang::TypeReflection::Kind::ParameterBlock:
    case slang::TypeReflection::Kind::TextureBuffer:
    case slang::TypeReflection::Kind::ShaderStorageBuffer:
    {
        auto containerVarLayout = typeLayout->getContainerVarLayout();
        auto elementVarLayout = typeLayout->getElementVarLayout();

        LOG_DEBUG("{}container", indentStr);
        printOffsets(containerVarLayout, indent + 1);

        LOG_DEBUG("{}element: ", indentStr);
        printOffsets(elementVarLayout, indent + 1);

        LOG_DEBUG("{}type layout: ", indentStr);
        printTypeLayout(elementVarLayout->getTypeLayout(), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::Resource:
    {
        if ((typeLayout->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER)
        {
            LOG_DEBUG("{}element type layout: ", indentStr);
            printTypeLayout(typeLayout->getElementTypeLayout(), indent + 1);
        }
        else
        {
            LOG_DEBUG("{}result type: todo", indentStr);
        }
    }
    break;

    default:
        break;
    }
}

/*
CumulativeOffset Program::calculateCumulativeOffset(slang::ParameterCategory layoutUnit, AccessPath accessPath) const
{
    CumulativeOffset result;
    switch (layoutUnit)
    {
    case slang::ParameterCategory::Uniform:
        for (auto node = accessPath.leaf; node != accessPath.deepestConstantBuffer; node = node->outer)
            result.offset += node->varLayout->getOffset(layoutUnit);
        break;

    case slang::ParameterCategory::ConstantBuffer:
    case slang::ParameterCategory::ShaderResource:
    case slang::ParameterCategory::UnorderedAccess:
    case slang::ParameterCategory::SamplerState:
    case slang::ParameterCategory::DescriptorTableSlot:
        {
            for (auto node = accessPath.leaf; node != accessPath.deepestParameterBlock; node = node->outer)
            {
                result.offset += node->varLayout->getOffset(layoutUnit);
                result.space += node->varLayout->getBindingSpace(layoutUnit);
            }
            for (auto node = accessPath.deepestParameterBlock; node != nullptr; node = node->outer)
                result.space += node->varLayout->getOffset(slang::ParameterCategory::SubElementRegisterSpace);
        }
        break;

    default:
        for (auto node = accessPath.leaf; node != nullptr; node = node->outer)
            result.offset += node->varLayout->getOffset(layoutUnit);
        break;
    }
    return result;
}
*/

void Program::printRelativeOffsets(slang::VariableLayoutReflection* varLayout, int indent) const
{
    std::string indentStr(indent * 2, ' ');
    LOG_DEBUG("{}relative offsets: ", indentStr);
    int usedLayoutUnitCount = varLayout->getCategoryCount();
    for (int i = 0; i < usedLayoutUnitCount; i++)
    {
        auto layoutUnit = varLayout->getCategoryByIndex(i);
        printOffset(varLayout, layoutUnit, indent + 1);
    }
}

void Program::printOffset(slang::VariableLayoutReflection* varLayout, slang::ParameterCategory layoutUnit, int indent) const
{
    std::string indentStr(indent * 2, ' ');
    size_t offset = varLayout->getOffset(layoutUnit);
    LOG_DEBUG("{}value: {}", indentStr, offset);
    LOG_DEBUG("{}unit: {}", indentStr, printLayoutUnit(layoutUnit));

    size_t spaceOffset = varLayout->getBindingSpace(layoutUnit);
    switch (layoutUnit)
    {
    case slang::ParameterCategory::ConstantBuffer:
    case slang::ParameterCategory::ShaderResource:
    case slang::ParameterCategory::UnorderedAccess:
    case slang::ParameterCategory::SamplerState:
    case slang::ParameterCategory::DescriptorTableSlot:
        LOG_DEBUG("{}space: {}", indentStr, spaceOffset);
    default:
        break;
    }
}

void Program::printOffsets(slang::VariableLayoutReflection* varLayout, int indent) const
{
    std::string indentStr(indent * 2, ' ');
    LOG_DEBUG("{}offsets:", indentStr);
    printRelativeOffsets(varLayout, indent + 1);
}

void Program::printSize(slang::TypeLayoutReflection* typeLayout, slang::ParameterCategory layoutUnit, int indent) const
{
    std::string indentStr(indent * 2, ' ');
    size_t size = typeLayout->getSize(layoutUnit);
    LOG_DEBUG("{}value: {}", indentStr, size);
    LOG_DEBUG("{}unit: {}", indentStr, printLayoutUnit(layoutUnit));
}

void Program::printSizes(slang::TypeLayoutReflection* typeLayout, int indent) const
{
    std::string indentStr(indent * 2, ' ');
    int usedLayoutUnitCount = typeLayout->getCategoryCount();
    for (int i = 0; i < usedLayoutUnitCount; i++)
    {
        auto layoutUnit = typeLayout->getCategoryByIndex(i);
        printSize(typeLayout, layoutUnit, indent + 1);
    }

    // Alignment and stride
    if (typeLayout->getSize() != 0)
    {
        LOG_DEBUG("{}alignment in bytes: {}", indentStr, typeLayout->getAlignment());
        LOG_DEBUG("{}stride in bytes: {}", indentStr, typeLayout->getStride());
    }
}

std::string Program::printKind(slang::TypeReflection::Kind kind) const
{
    switch (kind)
    {
    case slang::TypeReflection::Kind::Struct:
        return "Struct";
    case slang::TypeReflection::Kind::ConstantBuffer:
        return "ConstantBuffer";
    case slang::TypeReflection::Kind::ParameterBlock:
        return "ParameterBlock";
    case slang::TypeReflection::Kind::TextureBuffer:
        return "TextureBuffer";
    case slang::TypeReflection::Kind::Vector:
        return "Vector";
    case slang::TypeReflection::Kind::Scalar:
        return "Scalar";
    case slang::TypeReflection::Kind::Resource:
        return "Resource";
    default:
        LOG_WARN("[Program] Unknown type kind: {}", static_cast<int>(kind));
        return "Unknown";
    }
}

std::string Program::printLayoutUnit(slang::ParameterCategory layoutUnit) const
{
    switch (layoutUnit)
    {
    case slang::ParameterCategory::ConstantBuffer:
        return "constant buffer slots";
    case slang::ParameterCategory::ShaderResource:
        return "texture slots";
    case slang::ParameterCategory::UnorderedAccess:
        return "uav slots";
    case slang::ParameterCategory::VaryingInput:
        return "varying input slots";
    case slang::ParameterCategory::VaryingOutput:
        return "varying output slots";
    case slang::ParameterCategory::SamplerState:
        return "sampler slots";
    case slang::ParameterCategory::Uniform:
        return "bytes";
    case slang::ParameterCategory::DescriptorTableSlot:
        return "bindings";
    case slang::ParameterCategory::SpecializationConstant:
        return "specialization constant ids";
    case slang::ParameterCategory::PushConstantBuffer:
        return "push-constant buffers";
    case slang::ParameterCategory::RegisterSpace:
        return "register space offset for a variable";
    case slang::ParameterCategory::GenericResource:
        return "generic resources";
    case slang::ParameterCategory::RayPayload:
        return "ray payloads";
    case slang::ParameterCategory::HitAttributes:
        return "hit attributes";
    case slang::ParameterCategory::CallablePayload:
        return "callable payloads";
    case slang::ParameterCategory::ShaderRecord:
        return "shader records";
    case slang::ParameterCategory::ExistentialTypeParam:
        return "existential type parameters";
    case slang::ParameterCategory::ExistentialObjectParam:
        return "existential object parameters";
    case slang::ParameterCategory::SubElementRegisterSpace:
        return "register spaces / descriptor sets";
    case slang::ParameterCategory::InputAttachmentIndex:
        return "subpass input attachments";
    case slang::ParameterCategory::MetalArgumentBufferElement:
        return "Metal argument buffer elements";
    case slang::ParameterCategory::MetalAttribute:
        return "Metal attributes";
    case slang::ParameterCategory::MetalPayload:
        return "Metal payloads";
    case slang::ParameterCategory::None:
        LOG_WARN("[Program] Layout unit is None, this should not happen");
        return "Unknown";
    default:
        LOG_WARN("[Program] Unknown layout unit: {}", static_cast<int>(layoutUnit));
        return "Unknown";
    }
}

bool Program::generateBindingLayout()
{
    if (!m_ProgramLayout)
    {
        LOG_ERROR("[Program] No program layout available for binding generation");
        return false;
    }
    m_ReflectionInfo.clear();

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

bool Program::processParameter(slang::VariableLayoutReflection* varLayout, int bindingSpaceOffset, std::string prefix)
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
                layoutItem = resourceAccess == SLANG_RESOURCE_ACCESS_READ ? nvrhi::BindingLayoutItem::Texture_SRV(bindingSlot)
                                                                          : nvrhi::BindingLayoutItem::Texture_UAV(bindingSlot);
                bindingItem = resourceAccess == SLANG_RESOURCE_ACCESS_READ ? nvrhi::BindingSetItem::Texture_SRV(bindingSlot, nullptr)
                                                                           : nvrhi::BindingSetItem::Texture_UAV(bindingSlot, nullptr);
                LOG_DEBUG(
                    "[ShaderBinding] Found {} texture: {} at slot {} in space {}",
                    resourceAccess == SLANG_RESOURCE_ACCESS_READ ? "SRV" : "UAV",
                    paramNameStr,
                    bindingSlot,
                    bindingSpaceOffset
                );
            }
            break;

        case SLANG_STRUCTURED_BUFFER:
            if (resourceAccess == SLANG_RESOURCE_ACCESS_READ || resourceAccess == SLANG_RESOURCE_ACCESS_READ_WRITE)
            {
                layoutItem = resourceAccess == SLANG_RESOURCE_ACCESS_READ ? nvrhi::BindingLayoutItem::StructuredBuffer_SRV(bindingSlot)
                                                                          : nvrhi::BindingLayoutItem::StructuredBuffer_UAV(bindingSlot);
                bindingItem = resourceAccess == SLANG_RESOURCE_ACCESS_READ ? nvrhi::BindingSetItem::StructuredBuffer_SRV(bindingSlot, nullptr)
                                                                           : nvrhi::BindingSetItem::StructuredBuffer_UAV(bindingSlot, nullptr);
                LOG_DEBUG(
                    "[ShaderBinding] Found {} structured buffer: {} at slot {} in space {}",
                    resourceAccess == SLANG_RESOURCE_ACCESS_READ ? "SRV" : "UAV",
                    paramNameStr,
                    bindingSlot,
                    bindingSpaceOffset
                );
            }
            break;
        case SLANG_ACCELERATION_STRUCTURE:
            if (resourceAccess == SLANG_RESOURCE_ACCESS_READ || resourceAccess == SLANG_RESOURCE_ACCESS_READ_WRITE)
            {
                layoutItem = nvrhi::BindingLayoutItem::RayTracingAccelStruct(bindingSlot);
                bindingItem = nvrhi::BindingSetItem::RayTracingAccelStruct(bindingSlot, nullptr);
                LOG_DEBUG(
                    "[ShaderBinding] Found ray tracing acceleration structure: {} at slot {} in space {}",
                    paramNameStr,
                    bindingSlot,
                    bindingSpaceOffset
                );
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
        layoutItem = nvrhi::BindingLayoutItem::ConstantBuffer(bindingSlot);
        bindingItem = nvrhi::BindingSetItem::ConstantBuffer(bindingSlot, nullptr);
        LOG_DEBUG("[ShaderBinding] Found constant buffer: {} at slot {}", paramNameStr, bindingSlot);
        break;
    }
    case slang::TypeReflection::Kind::ParameterBlock:
    {
        auto bindingSpace = varLayout->getOffset(slang::ParameterCategory::SubElementRegisterSpace) + bindingSpaceOffset;
        LOG_DEBUG("[ShaderBinding] Processing parameter block: {} at slot {} in space {}", paramNameStr, bindingSlot, bindingSpace);

        // ParameterBlock creates its own binding space - process its contents recursively
        auto elementTypeLayout = typeLayout->getElementTypeLayout();
        if (elementTypeLayout)
        {
            auto fieldCount = elementTypeLayout->getFieldCount();
            LOG_DEBUG("[ShaderBinding] ParameterBlock {} contains {} fields: ", paramNameStr, fieldCount);

            for (unsigned int i = 0; i < fieldCount; i++)
            {
                auto fieldLayout = elementTypeLayout->getFieldByIndex(i);
                if (!processParameter(fieldLayout, bindingSpace, paramNameStr + "."))
                    return false;
            }
        }
        else
        {
            LOG_WARN("[ShaderBinding] ParameterBlock {} has no element type layout", paramNameStr);
        }

        // ParameterBlock doesn't create a binding itself, only its contents do
        return true;
    }

    default:
        return true;
    }

    ReflectionInfo reflectionInfo;
    reflectionInfo.name = prefix + paramNameStr;
    reflectionInfo.bindingLayoutItem = layoutItem;
    reflectionInfo.bindingSetItem = bindingItem;
    reflectionInfo.bindingSpace = bindingSpaceOffset;
    m_ReflectionInfo.push_back(reflectionInfo);

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
