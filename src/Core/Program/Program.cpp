#include "Program.h"
#include "ShaderCompiler.h"

namespace
{
// AccessPath pattern from Slang's reflection user guide: a leaf-to-root linked list
// of variable layouts, plus boundary markers for the deepest enclosing ConstantBuffer
// (stops byte-offset accumulation) and ParameterBlock (stops slot-offset accumulation
// and starts register-space accumulation).
struct AccessPathNode
{
    slang::VariableLayoutReflection* varLayout;
    const AccessPathNode* outer;
};

struct AccessPath
{
    const AccessPathNode* leaf = nullptr;
    const AccessPathNode* deepestConstantBuffer = nullptr;
    const AccessPathNode* deepestParameterBlock = nullptr;
};

struct CumulativeOffset
{
    uint32_t offset = 0;
    uint32_t space = 0;
};

CumulativeOffset computeCumulativeOffset(const AccessPath& path, slang::ParameterCategory category)
{
    CumulativeOffset result;
    switch (category)
    {
    case slang::ParameterCategory::Uniform:
        for (auto node = path.leaf; node != path.deepestConstantBuffer; node = node->outer)
            result.offset += static_cast<uint32_t>(node->varLayout->getOffset(category));
        break;

    case slang::ParameterCategory::ConstantBuffer:
    case slang::ParameterCategory::ShaderResource:
    case slang::ParameterCategory::UnorderedAccess:
    case slang::ParameterCategory::SamplerState:
    case slang::ParameterCategory::DescriptorTableSlot:
        for (auto node = path.leaf; node != path.deepestParameterBlock; node = node->outer)
        {
            result.offset += static_cast<uint32_t>(node->varLayout->getOffset(category));
            result.space += static_cast<uint32_t>(node->varLayout->getBindingSpace(category));
        }
        for (auto node = path.deepestParameterBlock; node != nullptr; node = node->outer)
            result.space += static_cast<uint32_t>(node->varLayout->getOffset(slang::ParameterCategory::SubElementRegisterSpace));
        break;

    default:
        for (auto node = path.leaf; node != nullptr; node = node->outer)
            result.offset += static_cast<uint32_t>(node->varLayout->getOffset(category));
        break;
    }
    return result;
}

std::string joinName(const AccessPath& path)
{
    std::vector<const char*> parts;
    for (auto node = path.leaf; node != nullptr; node = node->outer)
    {
        const char* name = node->varLayout->getName();
        if (name && *name)
            parts.push_back(name);
    }
    std::string result;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
    {
        if (!result.empty())
            result += '.';
        result += *it;
    }
    return result;
}

bool makeResourceBinding(
    SlangResourceShape shape,
    SlangResourceAccess access,
    uint32_t slot,
    nvrhi::BindingLayoutItem& outLayoutItem,
    nvrhi::BindingSetItem& outBindingItem
)
{
    const bool isRead = (access == SLANG_RESOURCE_ACCESS_READ);
    const bool isReadWrite = (access == SLANG_RESOURCE_ACCESS_READ_WRITE);

    switch (shape)
    {
    case SLANG_TEXTURE_1D:
    case SLANG_TEXTURE_2D:
    case SLANG_TEXTURE_3D:
    case SLANG_TEXTURE_CUBE:
    case SLANG_TEXTURE_BUFFER:
        if (!isRead && !isReadWrite)
            return false;
        outLayoutItem = isRead ? nvrhi::BindingLayoutItem::Texture_SRV(slot) : nvrhi::BindingLayoutItem::Texture_UAV(slot);
        outBindingItem = isRead ? nvrhi::BindingSetItem::Texture_SRV(slot, nullptr) : nvrhi::BindingSetItem::Texture_UAV(slot, nullptr);
        return true;

    case SLANG_STRUCTURED_BUFFER:
        if (!isRead && !isReadWrite)
            return false;
        outLayoutItem = isRead ? nvrhi::BindingLayoutItem::StructuredBuffer_SRV(slot) : nvrhi::BindingLayoutItem::StructuredBuffer_UAV(slot);
        outBindingItem =
            isRead ? nvrhi::BindingSetItem::StructuredBuffer_SRV(slot, nullptr) : nvrhi::BindingSetItem::StructuredBuffer_UAV(slot, nullptr);
        return true;

    case SLANG_ACCELERATION_STRUCTURE:
        outLayoutItem = nvrhi::BindingLayoutItem::RayTracingAccelStruct(slot);
        outBindingItem = nvrhi::BindingSetItem::RayTracingAccelStruct(slot, nullptr);
        return true;

    default:
        return false;
    }
}

const char* resourceLabel(SlangResourceShape shape, SlangResourceAccess access)
{
    const bool isRead = (access == SLANG_RESOURCE_ACCESS_READ);
    switch (shape)
    {
    case SLANG_TEXTURE_1D:
    case SLANG_TEXTURE_2D:
    case SLANG_TEXTURE_3D:
    case SLANG_TEXTURE_CUBE:
    case SLANG_TEXTURE_BUFFER:
        return isRead ? "SRV texture" : "UAV texture";
    case SLANG_STRUCTURED_BUFFER:
        return isRead ? "SRV structured buffer" : "UAV structured buffer";
    case SLANG_ACCELERATION_STRUCTURE:
        return "ray tracing accel struct";
    default:
        return "unknown";
    }
}

void walk(const AccessPath& path, std::vector<ReflectionInfo>& out);

void walkScope(slang::VariableLayoutReflection* scope, std::vector<ReflectionInfo>& out)
{
    if (!scope)
        return;
    auto typeLayout = scope->getTypeLayout();
    if (!typeLayout)
        return;

    AccessPathNode scopeNode{scope, nullptr};
    AccessPath rootPath;
    rootPath.leaf = &scopeNode;

    if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
    {
        for (int i = 0; i < static_cast<int>(typeLayout->getFieldCount()); ++i)
        {
            AccessPathNode fieldNode{typeLayout->getFieldByIndex(i), &scopeNode};
            AccessPath fieldPath = rootPath;
            fieldPath.leaf = &fieldNode;
            walk(fieldPath, out);
        }
    }
    else
    {
        walk(rootPath, out);
    }
}

void walk(const AccessPath& path, std::vector<ReflectionInfo>& out)
{
    auto* varLayout = path.leaf->varLayout;
    auto* typeLayout = varLayout->getTypeLayout();
    if (!typeLayout)
        return;

    switch (typeLayout->getKind())
    {
    case slang::TypeReflection::Kind::Struct:
    {
        for (int i = 0; i < static_cast<int>(typeLayout->getFieldCount()); ++i)
        {
            AccessPathNode fieldNode{typeLayout->getFieldByIndex(i), path.leaf};
            AccessPath fieldPath = path;
            fieldPath.leaf = &fieldNode;
            walk(fieldPath, out);
        }
        return;
    }

    case slang::TypeReflection::Kind::ConstantBuffer:
    {
        const auto offset = computeCumulativeOffset(path, slang::ParameterCategory::ConstantBuffer);
        const uint32_t slot = offset.offset;
        std::string name = joinName(path);

        ReflectionInfo info;
        info.name = name;
        // TODO: every constant buffer we treat as volatile for now
        info.bindingLayoutItem = nvrhi::BindingLayoutItem::VolatileConstantBuffer(slot);
        info.bindingSetItem = nvrhi::BindingSetItem::ConstantBuffer(slot, nullptr);
        info.bindingSetItem.type = nvrhi::ResourceType::VolatileConstantBuffer;
        info.bindingSpace = offset.space;
        LOG_DEBUG("[ShaderBinding] Found constant buffer: {} at slot {} in space {}", name, slot, offset.space);
        out.push_back(std::move(info));
        return;
    }

    case slang::TypeReflection::Kind::ParameterBlock:
    {
        auto* elementVarLayout = typeLayout->getElementVarLayout();
        if (!elementVarLayout)
            return;
        auto* elementTypeLayout = elementVarLayout->getTypeLayout();
        if (!elementTypeLayout)
            return;

        const std::string name = joinName(path);
        // ParameterBlock allocates its own register space and an implicit uniform CB.
        // Walk its struct fields with path boundaries updated so inner bindings compute
        // their slot within the PB and accumulate the PB's space offset.
        if (elementTypeLayout->getKind() == slang::TypeReflection::Kind::Struct)
        {
            LOG_DEBUG("[ShaderBinding] Processing parameter block: {}", name);
            for (int i = 0; i < static_cast<int>(elementTypeLayout->getFieldCount()); ++i)
            {
                AccessPathNode fieldNode{elementTypeLayout->getFieldByIndex(i), path.leaf};
                AccessPath fieldPath = path;
                fieldPath.leaf = &fieldNode;
                fieldPath.deepestParameterBlock = path.leaf;
                fieldPath.deepestConstantBuffer = path.leaf;
                walk(fieldPath, out);
            }
        }
        else
        {
            LOG_WARN("[ShaderBinding] ParameterBlock {} element is not a struct, skipping", name);
        }
        return;
    }

    case slang::TypeReflection::Kind::Resource:
    {
        const SlangResourceAccess access = typeLayout->getResourceAccess();
        const SlangResourceShape shape = typeLayout->getResourceShape();
        const auto category =
            (access == SLANG_RESOURCE_ACCESS_READ) ? slang::ParameterCategory::ShaderResource : slang::ParameterCategory::UnorderedAccess;
        const auto offset = computeCumulativeOffset(path, category);
        std::string name = joinName(path);

        nvrhi::BindingLayoutItem layoutItem;
        nvrhi::BindingSetItem bindingItem;
        if (!makeResourceBinding(shape, access, offset.offset, layoutItem, bindingItem))
        {
            LOG_WARN("[ShaderBinding] Unsupported resource shape for: {} (shape: {})", name, static_cast<int>(shape));
            return;
        }

        ReflectionInfo info;
        info.name = name;
        info.bindingLayoutItem = layoutItem;
        info.bindingSetItem = bindingItem;
        info.bindingSpace = offset.space;
        LOG_DEBUG("[ShaderBinding] Found {}: {} at slot {} in space {}", resourceLabel(shape, access), name, offset.offset, offset.space);
        out.push_back(std::move(info));
        return;
    }

    case slang::TypeReflection::Kind::Array:
    {
        auto* elementType = typeLayout->getElementTypeLayout();
        std::string name = joinName(path);
        if (!elementType)
        {
            LOG_WARN("[ShaderBinding] Array {} has no element type layout", name);
            return;
        }
        if (elementType->getKind() != slang::TypeReflection::Kind::Resource)
        {
            LOG_WARN("[ShaderBinding] Unsupported array element kind for: {}", name);
            return;
        }

        const auto elementCount = static_cast<uint32_t>(typeLayout->getElementCount());
        const SlangResourceShape shape = elementType->getResourceShape();
        const SlangResourceAccess access = elementType->getResourceAccess();

        // Non-texture arrays (StructuredBuffer, Sampler, AccelStruct) are rejected:
        // the runtime descriptor-table writer in BindingSetManager is texture-only.
        const bool isTextureShape =
            (shape == SLANG_TEXTURE_1D || shape == SLANG_TEXTURE_2D || shape == SLANG_TEXTURE_3D || shape == SLANG_TEXTURE_CUBE ||
             shape == SLANG_TEXTURE_BUFFER);
        const bool hasSupportedAccess = (access == SLANG_RESOURCE_ACCESS_READ || access == SLANG_RESOURCE_ACCESS_READ_WRITE);
        if (!isTextureShape || !hasSupportedAccess)
        {
            LOG_WARN(
                "[ShaderBinding] Unsupported array type for {}: shape {} — descriptor-table writer supports textures only",
                name,
                static_cast<int>(shape)
            );
            return;
        }

        const auto category =
            (access == SLANG_RESOURCE_ACCESS_READ) ? slang::ParameterCategory::ShaderResource : slang::ParameterCategory::UnorderedAccess;
        const auto offset = computeCumulativeOffset(path, category);

        nvrhi::BindingLayoutItem layoutItem;
        nvrhi::BindingSetItem bindingItem;
        if (!makeResourceBinding(shape, access, offset.offset, layoutItem, bindingItem))
            return;
        layoutItem.setSize(elementCount);

        ReflectionInfo info;
        info.name = name;
        info.bindingLayoutItem = layoutItem;
        info.bindingSetItem = bindingItem;
        info.bindingSpace = offset.space;
        info.isDescriptorTable = true;
        info.descriptorTableSize = elementCount;
        LOG_DEBUG(
            "[ShaderBinding] Created descriptor table: {} with {} elements at slot {} in space {}", name, elementCount, offset.offset, offset.space
        );
        out.push_back(std::move(info));
        return;
    }

    case slang::TypeReflection::Kind::SamplerState:
    {
        const auto offset = computeCumulativeOffset(path, slang::ParameterCategory::SamplerState);
        std::string name = joinName(path);

        ReflectionInfo info;
        info.name = name;
        info.bindingLayoutItem = nvrhi::BindingLayoutItem::Sampler(offset.offset);
        info.bindingSetItem = nvrhi::BindingSetItem::Sampler(offset.offset, nullptr);
        info.bindingSpace = offset.space;
        LOG_DEBUG("[ShaderBinding] Found sampler state: {} at slot {} in space {}", name, offset.offset, offset.space);
        out.push_back(std::move(info));
        return;
    }

    default:
        return;
    }
}

} // namespace

Program::Program(
    nvrhi::IDevice* device,
    const std::string& filePath,
    const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints,
    const std::string& profile,
    const std::vector<std::pair<std::string, std::string>>& defines
)
{
    if (entryPoints.empty())
        LOG_ERROR_RETURN("[Program] No entry points provided");

    slang::ISession* pSession = ShaderCompiler::get().getSession(profile, defines);
    if (!pSession)
        LOG_ERROR_RETURN("[Program] Failed to obtain Slang session for profile: {}", profile);

    // Load module
    Slang::ComPtr<slang::IModule> pModule;
    Slang::ComPtr<slang::IBlob> pDiagnostics;
    pModule = pSession->loadModule(filePath.c_str(), pDiagnostics.writeRef());
    if (pDiagnostics && pDiagnostics->getBufferSize() > 0)
        LOG_DEBUG("[Program] Compilation diagnostics: {}", (const char*)pDiagnostics->getBufferPointer());
    if (!pModule)
        LOG_ERROR_RETURN("[Slang] Failed to load module: {}", filePath);

    std::vector<Slang::ComPtr<slang::IEntryPoint>> slangEntryPoints;
    slangEntryPoints.reserve(entryPoints.size());

    for (const std::pair<std::string, nvrhi::ShaderType>& entryPoint : entryPoints)
    {
        Slang::ComPtr<slang::IEntryPoint> pSlangEntryPoint;
        if (SLANG_FAILED(pModule->findEntryPointByName(entryPoint.first.c_str(), pSlangEntryPoint.writeRef())))
            LOG_ERROR_RETURN("[Slang] Failed to find entry point: {}", entryPoint.first);
        slangEntryPoints.push_back(pSlangEntryPoint);
    }

    std::vector<slang::IComponentType*> components;
    components.reserve(1 + slangEntryPoints.size());
    components.push_back(pModule);
    for (auto& entryPoint : slangEntryPoints)
        components.push_back(entryPoint);

    Slang::ComPtr<slang::IComponentType> pProgram;
    if (SLANG_FAILED(pSession->createCompositeComponentType(components.data(), static_cast<SlangInt>(components.size()), pProgram.writeRef())))
        LOG_ERROR_RETURN("[Slang] Failed to create composite component type");

    if (SLANG_FAILED(pProgram->link(mLinkedProgram.writeRef())))
        LOG_ERROR_RETURN("[Slang] Failed to link program");

    mpProgramLayout = mLinkedProgram->getLayout(0, pDiagnostics.writeRef());
    if (pDiagnostics && pDiagnostics->getBufferSize() > 0)
        LOG_DEBUG("[Slang] Program layout diagnostics: {}", (const char*)pDiagnostics->getBufferPointer());
    if (!mpProgramLayout)
        LOG_ERROR_RETURN("[Slang] Failed to get program layout");

    mShaders.clear();
    mShaders.reserve(entryPoints.size());
    mEntryPointToShaderIndex.clear();
    uint32_t entryPointIndex = 0;
    for (const auto& entryPoint : entryPoints)
    {
        const auto& entryPointName = entryPoint.first;
        const auto& entryPointType = entryPoint.second;

        Slang::ComPtr<slang::IBlob> pKernelBlob;
        Slang::ComPtr<slang::IBlob> pDiagnosticBlob;

        if (SLANG_FAILED(mLinkedProgram->getEntryPointCode(entryPointIndex, 0, pKernelBlob.writeRef(), pDiagnosticBlob.writeRef())))
        {
            if (pDiagnosticBlob && pDiagnosticBlob->getBufferSize() > 0)
                LOG_ERROR("[Slang] Entry point diagnostics for {}: {}", entryPointName, (const char*)pDiagnosticBlob->getBufferPointer());
            LOG_ERROR_RETURN("[Slang] Failed to get entry point code for {}", entryPointName);
        }

        LOG_DEBUG("[Program] Compiled entry point {}: {} bytes", entryPointName, pKernelBlob->getBufferSize());

        nvrhi::ShaderDesc desc;
        desc.entryName = entryPointName.c_str();
        desc.shaderType = entryPointType;
        auto pShader = device->createShader(desc, pKernelBlob->getBufferPointer(), pKernelBlob->getBufferSize());
        if (!pShader)
            LOG_ERROR_RETURN("[Program] Failed to create shader for entry point: {}", entryPointName);

        mShaders.push_back(pShader);
        mEntryPointToShaderIndex[entryPointName] = entryPointIndex;
        entryPointIndex++;
    }

    LOG_DEBUG("[Program] Successfully loaded shader with {} entry points from: {}", entryPoints.size(), filePath);

    walkScope(mpProgramLayout->getGlobalParamsVarLayout(), mReflectionInfo);
    const auto entryPointCount = mpProgramLayout->getEntryPointCount();
    for (unsigned int i = 0; i < entryPointCount; i++)
        walkScope(mpProgramLayout->getEntryPointByIndex(i)->getVarLayout(), mReflectionInfo);
}

nvrhi::ShaderHandle Program::getShader(const std::string& entryPoint) const
{
    auto it = mEntryPointToShaderIndex.find(entryPoint);
    if (it != mEntryPointToShaderIndex.end())
        return mShaders[it->second];

    LOG_WARN("[Program] Entry point '{}' not found", entryPoint);
    return nullptr;
}
