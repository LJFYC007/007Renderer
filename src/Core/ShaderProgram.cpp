#include "ShaderProgram.h"
#include <iostream>

ShaderProgram::ShaderProgram() : m_ProgramLayout(nullptr)
{
    slang::createGlobalSession(m_GlobalSession.writeRef());

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = m_GlobalSession->findProfile("cs_6_2");
    targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

    slang::SessionDesc sessionDesc = {};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    m_GlobalSession->createSession(sessionDesc, m_Session.writeRef());
}

bool ShaderProgram::loadFromFile(
    nvrhi::IDevice* device,
    const std::string& filePath,
    const std::string& entryPoint,
    nvrhi::ShaderType shaderType,
    const std::string& profile
)
{
    Slang::ComPtr<slang::IModule> module;
    module = m_Session->loadModule(filePath.c_str());
    if (!module)
    {
        LOG_ERROR("[Slang] Failed to load module: {}", filePath);
        return false;
    }

    Slang::ComPtr<slang::IEntryPoint> slangEntryPoint;
    if (SLANG_FAILED(module->findEntryPointByName(entryPoint.c_str(), slangEntryPoint.writeRef())))
    {
        LOG_ERROR("[Slang] Failed to find entry point: {}", entryPoint);
        return false;
    }

    Slang::ComPtr<slang::IComponentType> program;
    {
        slang::IComponentType* components[] = {module, slangEntryPoint};
        if (SLANG_FAILED(m_Session->createCompositeComponentType(components, 2, program.writeRef())))
        {
            LOG_ERROR("[Slang] Failed to create composite component type");
            return false;
        }
    }

    if (SLANG_FAILED(program->link(m_LinkedProgram.writeRef())))
    {
        LOG_ERROR("[Slang] Failed to link program");
        return false;
    }

    // Get program layout first with diagnostics
    Slang::ComPtr<slang::IBlob> diagnostics;
    m_ProgramLayout = m_LinkedProgram->getLayout(0, diagnostics.writeRef());
    if (diagnostics && diagnostics->getBufferSize() > 0)
        LOG_ERROR("[Slang] Program layout diagnostics: {}", (const char*)diagnostics->getBufferPointer());
    if (!m_ProgramLayout)
    {
        LOG_ERROR("[Slang] Failed to get program layout");
        return false;
    }

    Slang::ComPtr<slang::IBlob> shaderBlob;
    if (SLANG_FAILED(m_LinkedProgram->getTargetCode(0, shaderBlob.writeRef())))
    {
        LOG_ERROR("[Slang] Failed to get target code");
        return false;
    }

    nvrhi::ShaderDesc desc;
    desc.entryName = entryPoint.c_str();
    desc.shaderType = shaderType;
    m_Shader = device->createShader(desc, shaderBlob->getBufferPointer(), shaderBlob->getBufferSize());

    return m_Shader != nullptr;
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
    const std::unordered_map<std::string, nvrhi::BufferHandle>& bufferMap
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
        if (!processParameterGroup(globalScopeLayout, outLayoutItems, outBindings, bufferMap))
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
            if (!processParameterGroup(entryPointLayout, outLayoutItems, outBindings, bufferMap))
                return false;
        }
    }

    return true;
}

bool ShaderProgram::processParameterGroup(
    slang::VariableLayoutReflection* varLayout,
    std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
    std::vector<nvrhi::BindingSetItem>& outBindings,
    const std::unordered_map<std::string, nvrhi::BufferHandle>& bufferMap
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
            if (!processParameter(fieldLayout, outLayoutItems, outBindings, bufferMap))
                return false;
        }
    }
    else
        return processParameter(varLayout, outLayoutItems, outBindings, bufferMap);

    return true;
}

bool ShaderProgram::processParameter(
    slang::VariableLayoutReflection* varLayout,
    std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
    std::vector<nvrhi::BindingSetItem>& outBindings,
    const std::unordered_map<std::string, nvrhi::BufferHandle>& bufferMap
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
    else if (typeLayout->getKind() == slang::TypeReflection::Kind::ConstantBuffer ||
             typeLayout->getKind() == slang::TypeReflection::Kind::ParameterBlock)
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
        case SLANG_STRUCTURED_BUFFER:
            if (resourceAccess == SLANG_RESOURCE_ACCESS_READ)
            {
                auto bufferIt = bufferMap.find(paramNameStr);
                if (bufferIt != bufferMap.end())
                {
                    layoutItem = nvrhi::BindingLayoutItem::StructuredBuffer_SRV(bindingSlot);
                    bindingItem = nvrhi::BindingSetItem::StructuredBuffer_SRV(bindingSlot, bufferIt->second);
                }
                else
                    LOG_WARN("[ShaderBinding] SRV Buffer not found in map: {}", paramNameStr);

                LOG_DEBUG("[ShaderBinding] Found SRV structured buffer: {} at slot {}", paramNameStr, bindingSlot);
            }
            else if (resourceAccess == SLANG_RESOURCE_ACCESS_READ_WRITE)
            {
                auto bufferIt = bufferMap.find(paramNameStr);
                if (bufferIt != bufferMap.end())
                {
                    bindingItem.resourceHandle = bufferIt->second;
                    layoutItem = nvrhi::BindingLayoutItem::StructuredBuffer_UAV(bindingSlot);
                    bindingItem = nvrhi::BindingSetItem::StructuredBuffer_UAV(bindingSlot, bufferIt->second);
                }
                else
                    LOG_WARN("[ShaderBinding] UAV Buffer not found in map: {}", paramNameStr);

                LOG_DEBUG("[ShaderBinding] Found UAV structured buffer: {} at slot {}", paramNameStr, bindingSlot);
            }
            break;

        default:
            LOG_WARN("[ShaderBinding] Unknown resource shape for: {} (shape: {})", paramNameStr, static_cast<int>(resourceShape));
            return true;
        }
    }
    break;

    default:
        return true;
    }

    outLayoutItems.push_back(layoutItem);
    outBindings.push_back(bindingItem);
    return true;
}
