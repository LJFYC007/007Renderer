#include "BindingSetManager.h"
#include "Utils/Logger.h"

BindingSetManager::BindingSetManager(ref<Device> pDevice, const std::vector<ReflectionInfo>& reflectionInfo) : mpDevice(pDevice)
{
    for (const auto& info : reflectionInfo)
    {
        const uint32_t space = info.bindingSpace;
        SpaceData& spaceData = mSpaces[space];

        if (info.isDescriptorTable)
        {
            if (!spaceData.layoutItems.empty() || !spaceData.descriptorTables.empty())
            {
                LOG_ERROR(
                    "[BindingSetManager] Space {} already has bindings; refusing to add descriptor table '{}'. Put bindless arrays in "
                    "their own ParameterBlock.",
                    space,
                    info.name
                );
                continue;
            }

            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.registerSpace = space;
            layoutDesc.bindings.push_back(info.bindingLayoutItem);

            nvrhi::BindingLayoutHandle descriptorTableLayout = mpDevice->getDevice()->createBindingLayout(layoutDesc);
            if (!descriptorTableLayout)
            {
                LOG_ERROR("[BindingSetManager] Failed to create binding layout for descriptor table '{}'", info.name);
                continue;
            }

            nvrhi::DescriptorTableHandle descriptorTable = mpDevice->getDevice()->createDescriptorTable(descriptorTableLayout);
            if (!descriptorTable)
            {
                LOG_ERROR("[BindingSetManager] Failed to create descriptor table '{}'", info.name);
                continue;
            }

            mpDevice->getDevice()->resizeDescriptorTable(descriptorTable, info.descriptorTableSize, false);
            LOG_DEBUG("[BindingSetManager] Creating new descriptor table for space {} with size {}", space, info.descriptorTableSize);

            DescriptorTableInfo tableInfo;
            tableInfo.space = space;
            tableInfo.index = 0;
            tableInfo.descriptorTable = descriptorTable;
            tableInfo.bindingLayout = descriptorTableLayout;
            tableInfo.size = info.descriptorTableSize;
            mDescriptorTables[info.name] = tableInfo;

            spaceData.descriptorTables.push_back(descriptorTable);
            spaceData.bindingLayout = descriptorTableLayout;
        }
        else
        {
            if (!spaceData.descriptorTables.empty())
            {
                LOG_ERROR(
                    "[BindingSetManager] Space {} already has a descriptor table; refusing to add regular binding '{}'. Move one into its own "
                    "ParameterBlock.",
                    space,
                    info.name
                );
                continue;
            }

            const uint32_t index = static_cast<uint32_t>(spaceData.layoutItems.size());
            spaceData.layoutItems.push_back(info.bindingLayoutItem);
            spaceData.bindingSetItems.push_back(info.bindingSetItem);
            mResourceMap[info.name] = {space, index};
        }
    }

    for (auto& [space, data] : mSpaces)
    {
        if (!data.descriptorTables.empty() || data.layoutItems.empty())
            continue;

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.registerSpace = space;
        layoutDesc.bindings = data.layoutItems;
        data.bindingLayout = mpDevice->getDevice()->createBindingLayout(layoutDesc);
    }
}

std::vector<nvrhi::BindingSetHandle> BindingSetManager::getBindingSets()
{
    std::vector<nvrhi::BindingSetHandle> result;
    result.reserve(mSpaces.size());

    for (auto& [space, data] : mSpaces)
    {
        if (!data.descriptorTables.empty())
        {
            result.push_back(data.descriptorTables[0]);
            continue;
        }

        if (data.layoutItems.empty())
            continue;

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.bindings = data.bindingSetItems;

        size_t hash = 0;
        nvrhi::hash_combine(hash, bindingSetDesc);
        nvrhi::hash_combine(hash, data.bindingLayout);

        if (data.currentHash != hash)
        {
            LOG_DEBUG("[BindingSetManager] Creating new binding set for space {} with hash: {}", space, hash);
            data.bindingSet = mpDevice->getDevice()->createBindingSet(bindingSetDesc, data.bindingLayout);
            data.currentHash = hash;
        }
        result.push_back(data.bindingSet);
    }

    return result;
}

std::vector<nvrhi::BindingLayoutHandle> BindingSetManager::getBindingLayouts()
{
    std::vector<nvrhi::BindingLayoutHandle> result;
    result.reserve(mSpaces.size());
    for (auto& [space, data] : mSpaces)
        if (data.bindingLayout)
            result.push_back(data.bindingLayout);
    return result;
}

void BindingSetManager::setResourceHandle(const std::string& name, nvrhi::ResourceHandle resource)
{
    auto it = mResourceMap.find(name);
    if (it == mResourceMap.end())
        LOG_ERROR_RETURN("[BindingSetManager] Resource '{}' not found in layout", name);

    uint32_t space = it->second.first;
    uint32_t index = it->second.second;
    mSpaces[space].bindingSetItems[index].resourceHandle = resource;
}

void BindingSetManager::setDescriptorTable(
    const std::string& name,
    const std::vector<nvrhi::TextureHandle>& textures,
    nvrhi::TextureHandle defaultTexture
)
{
    auto it = mDescriptorTables.find(name);
    if (it == mDescriptorTables.end())
        LOG_ERROR_RETURN("[BindingSetManager] Descriptor table '{}' not found", name);

    DescriptorTableInfo& tableInfo = it->second;

    for (size_t i = 0; i < textures.size(); ++i)
    {
        nvrhi::BindingSetItem item = nvrhi::BindingSetItem::Texture_SRV(static_cast<uint32_t>(i), textures[i]);
        if (!mpDevice->getDevice()->writeDescriptorTable(tableInfo.descriptorTable, item))
            LOG_ERROR("[BindingSetManager] Failed to write texture {} to descriptor table '{}'", i, name);
    }

    // Fill unused slots with default texture — bindless arrays require every slot
    // to be written even if unused; unbound descriptors crash the GPU.
    for (size_t i = textures.size(); i < tableInfo.size; ++i)
    {
        nvrhi::BindingSetItem item = nvrhi::BindingSetItem::Texture_SRV(static_cast<uint32_t>(i), defaultTexture);
        if (!mpDevice->getDevice()->writeDescriptorTable(tableInfo.descriptorTable, item))
            LOG_ERROR("[BindingSetManager] Failed to write default texture to slot {} in descriptor table '{}'", i, name);
    }
}
