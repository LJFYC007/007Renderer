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
        std::cerr << "[Slang] Failed to load module: " << filePath << std::endl;
        return false;
    }

    Slang::ComPtr<slang::IEntryPoint> slangEntryPoint;
    if (SLANG_FAILED(module->findEntryPointByName(entryPoint.c_str(), slangEntryPoint.writeRef())))
    {
        std::cerr << "[Slang] Failed to find entry point: " << entryPoint << std::endl;
        return false;
    }

    Slang::ComPtr<slang::IComponentType> program;
    {
        slang::IComponentType* components[] = {module, slangEntryPoint};
        if (SLANG_FAILED(m_Session->createCompositeComponentType(components, 2, program.writeRef())))
        {
            std::cerr << "[Slang] Failed to create composite component type" << std::endl;
            return false;
        }
    }

    if (SLANG_FAILED(program->link(m_LinkedProgram.writeRef())))
    {
        std::cerr << "[Slang] Failed to link program" << std::endl;
        return false;
    }

    // Get program layout first with diagnostics
    Slang::ComPtr<slang::IBlob> diagnostics;
    m_ProgramLayout = m_LinkedProgram->getLayout(0, diagnostics.writeRef());
    if (diagnostics && diagnostics->getBufferSize() > 0)
    {
        std::cerr << "[Slang] Layout diagnostics: " << (const char*)diagnostics->getBufferPointer() << std::endl;
    }
    if (!m_ProgramLayout)
    {
        std::cerr << "[Slang] Failed to get program layout" << std::endl;
        return false;
    }

    Slang::ComPtr<slang::IBlob> shaderBlob;
    if (SLANG_FAILED(m_LinkedProgram->getTargetCode(0, shaderBlob.writeRef())))
    {
        std::cerr << "[Slang] Failed to get target code" << std::endl;
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
        std::cout << "No program layout available" << std::endl;
        return;
    }

    std::cout << "=== Shader Reflection Information ===" << std::endl;

    auto globalScopeLayout = m_ProgramLayout->getGlobalParamsVarLayout();
    if (globalScopeLayout)
    {
        std::cout << "Global Parameters:" << std::endl;
        printVariableLayout(globalScopeLayout, 1);
    }

    auto entryPointCount = m_ProgramLayout->getEntryPointCount();
    for (unsigned int i = 0; i < entryPointCount; i++)
    {
        auto entryPoint = m_ProgramLayout->getEntryPointByIndex(i);
        std::cout << "Entry Point " << i << ": " << entryPoint->getName() << std::endl;

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
        std::cout << indentStr << "Variable: " << varLayout->getName() << std::endl;
    std::cout << indentStr << "  Type: ";

    switch (typeLayout->getKind())
    {
    case slang::TypeReflection::Kind::Struct:
        std::cout << "Struct" << std::endl;
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
        std::cout << "Array[" << typeLayout->getElementCount() << "]" << std::endl;
        break;

    case slang::TypeReflection::Kind::Vector:
        std::cout << "Vector" << typeLayout->getElementCount() << std::endl;
        break;

    case slang::TypeReflection::Kind::Matrix:
        std::cout << "Matrix" << typeLayout->getRowCount() << "x" << typeLayout->getColumnCount() << std::endl;
        break;

    case slang::TypeReflection::Kind::Scalar:
        std::cout << "Scalar" << std::endl;
        break;

    case slang::TypeReflection::Kind::Resource:
        std::cout << "Resource" << std::endl;
        break;

    case slang::TypeReflection::Kind::ConstantBuffer:
        std::cout << "ConstantBuffer" << std::endl;
        break;

    case slang::TypeReflection::Kind::ParameterBlock:
        std::cout << "ParameterBlock" << std::endl;
        break;

    default:
        std::cout << "Unknown" << std::endl;
        break;
    }

    auto offset = varLayout->getOffset(slang::ParameterCategory::Uniform);
    if (offset != ~0u)
        std::cout << indentStr << "  Uniform Offset: " << offset << std::endl;

    auto bindingOffset = varLayout->getOffset(slang::ParameterCategory::DescriptorTableSlot);
    if (bindingOffset != ~0u)
        std::cout << indentStr << "  Binding Offset: " << bindingOffset << std::endl;
}

bool ShaderProgram::generateBindingLayout(
    std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
    std::vector<nvrhi::BindingSetItem>& outBindings,
    const std::unordered_map<std::string, nvrhi::BufferHandle>& bufferMap
)
{
    if (!m_ProgramLayout)
    {
        std::cerr << "[ShaderProgram] No program layout available for binding generation" << std::endl;
        return false;
    }

    outLayoutItems.clear();
    outBindings.clear();

    // Process global parameters
    auto globalScopeLayout = m_ProgramLayout->getGlobalParamsVarLayout();
    if (globalScopeLayout)
    {
        if (!processParameterGroup(globalScopeLayout, outLayoutItems, outBindings, bufferMap))
        {
            return false;
        }
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
            {
                return false;
            }
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
        return true;

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
                    std::cout << "[ShaderBinding] WARNING: SRV Buffer not found in map: " << paramNameStr << std::endl;

                std::cout << "[ShaderBinding] Found SRV structured buffer: " << paramNameStr << " at slot " << bindingSlot << std::endl;
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
                    std::cout << "[ShaderBinding] WARNING: UAV Buffer not found in map: " << paramNameStr << std::endl;

                std::cout << "[ShaderBinding] Found UAV structured buffer: " << paramNameStr << " at slot " << bindingSlot << std::endl;
            }
            break;

        default:
            std::cout << "[ShaderBinding] Unknown resource shape for: " << paramNameStr << " (shape: " << resourceShape << ")" << std::endl;
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
