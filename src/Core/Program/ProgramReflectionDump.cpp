#include "Program.h"
#include "Utils/Logger.h"

namespace
{
std::string kindName(slang::TypeReflection::Kind kind)
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

std::string layoutUnitName(slang::ParameterCategory layoutUnit)
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

void printOffset(slang::VariableLayoutReflection* pVarLayout, slang::ParameterCategory layoutUnit, int indent)
{
    std::string indentStr(indent * 2, ' ');
    size_t offset = pVarLayout->getOffset(layoutUnit);
    LOG_DEBUG("{}value: {}", indentStr, offset);
    LOG_DEBUG("{}unit: {}", indentStr, layoutUnitName(layoutUnit));

    size_t spaceOffset = pVarLayout->getBindingSpace(layoutUnit);
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

void printRelativeOffsets(slang::VariableLayoutReflection* pVarLayout, int indent)
{
    std::string indentStr(indent * 2, ' ');
    LOG_DEBUG("{}relative offsets: ", indentStr);
    int usedLayoutUnitCount = pVarLayout->getCategoryCount();
    for (int i = 0; i < usedLayoutUnitCount; i++)
        printOffset(pVarLayout, pVarLayout->getCategoryByIndex(i), indent + 1);
}

void printOffsets(slang::VariableLayoutReflection* pVarLayout, int indent)
{
    std::string indentStr(indent * 2, ' ');
    LOG_DEBUG("{}offsets:", indentStr);
    printRelativeOffsets(pVarLayout, indent + 1);
}

void printSize(slang::TypeLayoutReflection* pTypeLayout, slang::ParameterCategory layoutUnit, int indent)
{
    std::string indentStr(indent * 2, ' ');
    size_t size = pTypeLayout->getSize(layoutUnit);
    LOG_DEBUG("{}value: {}", indentStr, size);
    LOG_DEBUG("{}unit: {}", indentStr, layoutUnitName(layoutUnit));
}

void printSizes(slang::TypeLayoutReflection* pTypeLayout, int indent)
{
    std::string indentStr(indent * 2, ' ');
    int usedLayoutUnitCount = pTypeLayout->getCategoryCount();
    for (int i = 0; i < usedLayoutUnitCount; i++)
        printSize(pTypeLayout, pTypeLayout->getCategoryByIndex(i), indent + 1);
    if (pTypeLayout->getSize() != 0)
    {
        LOG_DEBUG("{}alignment in bytes: {}", indentStr, pTypeLayout->getAlignment());
        LOG_DEBUG("{}stride in bytes: {}", indentStr, pTypeLayout->getStride());
    }
}

void printVarLayout(slang::VariableLayoutReflection* pVarLayout, int indent);

void printTypeLayout(slang::TypeLayoutReflection* pTypeLayout, int indent)
{
    std::string indentStr(indent * 2, ' ');
    LOG_DEBUG("{}name: {}", indentStr, pTypeLayout->getName() ? pTypeLayout->getName() : "None");
    LOG_DEBUG("{}kind: {}", indentStr, kindName(pTypeLayout->getKind()));
    printSizes(pTypeLayout, indent + 1);

    switch (pTypeLayout->getKind())
    {
    case slang::TypeReflection::Kind::Struct:
    {
        LOG_DEBUG("{}fields:", indentStr);
        int fieldCount = pTypeLayout->getFieldCount();
        for (int i = 0; i < fieldCount; i++)
            printVarLayout(pTypeLayout->getFieldByIndex(i), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::Array:
    {
        LOG_DEBUG("{}element count: {}", indentStr, pTypeLayout->getElementCount());
        LOG_DEBUG("{}element type layout: ", indentStr);
        printTypeLayout(pTypeLayout->getElementTypeLayout(), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::Vector:
    {
        LOG_DEBUG("{}element type layout: ", indentStr);
        printTypeLayout(pTypeLayout->getElementTypeLayout(), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::ConstantBuffer:
    case slang::TypeReflection::Kind::ParameterBlock:
    case slang::TypeReflection::Kind::TextureBuffer:
    case slang::TypeReflection::Kind::ShaderStorageBuffer:
    {
        auto pContainerVarLayout = pTypeLayout->getContainerVarLayout();
        auto pElementVarLayout = pTypeLayout->getElementVarLayout();

        LOG_DEBUG("{}container", indentStr);
        printOffsets(pContainerVarLayout, indent + 1);

        LOG_DEBUG("{}element: ", indentStr);
        printOffsets(pElementVarLayout, indent + 1);

        LOG_DEBUG("{}type layout: ", indentStr);
        printTypeLayout(pElementVarLayout->getTypeLayout(), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::Resource:
    {
        if ((pTypeLayout->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER)
        {
            LOG_DEBUG("{}element type layout: ", indentStr);
            printTypeLayout(pTypeLayout->getElementTypeLayout(), indent + 1);
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

void printVarLayout(slang::VariableLayoutReflection* pVarLayout, int indent)
{
    std::string indentStr(indent * 2, ' ');
    LOG_DEBUG("{}name: {}", indentStr, pVarLayout->getName());
    printRelativeOffsets(pVarLayout, indent + 1);
    LOG_DEBUG("{}type layout:", indentStr);
    printTypeLayout(pVarLayout->getTypeLayout(), indent + 1);
}

void printScope(slang::VariableLayoutReflection* pScopeVarLayout, int indent)
{
    std::string indentStr(indent * 2, ' ');
    auto pScopeTypeLayout = pScopeVarLayout->getTypeLayout();
    switch (pScopeTypeLayout->getKind())
    {
    case slang::TypeReflection::Kind::Struct:
    {
        LOG_DEBUG("{}parameters:", indentStr);
        int paramCount = pScopeTypeLayout->getFieldCount();
        for (int i = 0; i < paramCount; i++)
            printVarLayout(pScopeTypeLayout->getFieldByIndex(i), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::ConstantBuffer:
    {
        LOG_DEBUG("{}automatically-introduced constant buffer:", indentStr);
        printOffsets(pScopeTypeLayout->getContainerVarLayout(), indent + 1);
        printScope(pScopeTypeLayout->getElementVarLayout(), indent + 1);
    }
    break;

    case slang::TypeReflection::Kind::ParameterBlock:
    {
        LOG_DEBUG("{}automatically-introduced parameter block:", indentStr);
        printOffsets(pScopeTypeLayout->getContainerVarLayout(), indent + 1);
        printScope(pScopeTypeLayout->getElementVarLayout(), indent + 1);
    }
    break;

    default:
        LOG_WARN("[Program] Unsupported scope type kind for printing: {}", static_cast<int>(pScopeTypeLayout->getKind()));
        break;
    }
}

} // namespace

void Program::printReflectionInfo() const
{
    if (!mpProgramLayout)
        LOG_DEBUG_RETURN("[Program] No program layout available for reflection");
    LOG_DEBUG("[Program] Printing shader reflection information");

    auto pGlobalScopeLayout = mpProgramLayout->getGlobalParamsVarLayout();
    if (pGlobalScopeLayout)
    {
        LOG_DEBUG("[Program] Global Scope:");
        printScope(pGlobalScopeLayout, 1);
    }

    auto entryPointCount = mpProgramLayout->getEntryPointCount();
    for (unsigned int i = 0; i < entryPointCount; i++)
    {
        auto pEntryPoint = mpProgramLayout->getEntryPointByIndex(i);
        LOG_DEBUG("[Program] Entry Point {}: {}", i, pEntryPoint->getName());

        auto pEntryPointLayout = pEntryPoint->getVarLayout();
        if (pEntryPointLayout)
            printScope(pEntryPointLayout, 1);
    }
}
