#include "BindingSetManager.h"
#include "Utils/Logger.h"

BindingSetManager::BindingSetManager(ref<Device> pDevice, std::vector<ReflectionInfo> reflectionInfo) : mpDevice(pDevice)
{
    mSpaces.resize(mSpace);

    // Group reflection info by binding space and separate descriptor tables from regular bindings
    for (const auto& info : reflectionInfo)
    {
        uint32_t space = info.bindingSpace;

        if (info.isDescriptorTable)
        {
            // Descriptor table gets its own binding layout
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

            // Create descriptor table
            nvrhi::DescriptorTableHandle descriptorTable = mpDevice->getDevice()->createDescriptorTable(descriptorTableLayout);
            if (!descriptorTable)
            {
                LOG_ERROR("[BindingSetManager] Failed to create descriptor table '{}'", info.name);
                continue;
            }

            mpDevice->getDevice()->resizeDescriptorTable(descriptorTable, info.descriptorTableSize, false);
            LOG_DEBUG("[BindingSetManager] Creating new descriptor table for space {} with size {}", space, info.descriptorTableSize);

            // Store descriptor table info
            DescriptorTableInfo tableInfo;
            tableInfo.space = space;
            tableInfo.index = 0;
            tableInfo.descriptorTable = descriptorTable;
            tableInfo.bindingLayout = descriptorTableLayout;
            tableInfo.size = info.descriptorTableSize;
            mDescriptorTables[info.name] = tableInfo;

            mSpaces[space].descriptorTables.push_back(descriptorTable);
            mSpaces[space].bindingLayout = descriptorTableLayout;
        }
        else
        {
            // Regular binding - add to layout and binding set items
            uint32_t index = static_cast<uint32_t>(mSpaces[space].layoutItems.size());
            mSpaces[space].layoutItems.push_back(info.bindingLayoutItem);
            mSpaces[space].bindingSetItems.push_back(info.bindingSetItem);
            mResourceMap[info.name] = {space, index};
        }
    }

    // Create binding layouts for spaces with regular bindings
    for (uint32_t space = 0; space < mSpace; ++space)
    {
        // Skip if already has descriptor table or no items
        if (!mSpaces[space].descriptorTables.empty() || mSpaces[space].layoutItems.empty())
            continue;

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.registerSpace = space;
        layoutDesc.bindings = mSpaces[space].layoutItems;
        mSpaces[space].bindingLayout = mpDevice->getDevice()->createBindingLayout(layoutDesc);
    }
}

std::vector<nvrhi::BindingSetHandle> BindingSetManager::getBindingSets()
{
    std::vector<nvrhi::BindingSetHandle> result;
    result.resize(mSpace);

    for (uint32_t space = 0; space < mSpace; ++space)
    {
        // Descriptor tables
        if (!mSpaces[space].descriptorTables.empty())
        {
            auto descriptorTable = mSpaces[space].descriptorTables[0];
            result[space] = descriptorTable;
            continue;
        }

        // Regular binding sets
        if (mSpaces[space].layoutItems.empty())
        {
            result[space] = nullptr;
            continue;
        }

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.bindings = mSpaces[space].bindingSetItems;

        size_t hash = 0;
        nvrhi::hash_combine(hash, bindingSetDesc);
        nvrhi::hash_combine(hash, mSpaces[space].bindingLayout);

        if (mSpaces[space].currentHash != hash)
        {
            LOG_DEBUG("[BindingSetManager] Creating new binding set for space {} with hash: {}", space, hash);
            mSpaces[space].bindingSet = mpDevice->getDevice()->createBindingSet(bindingSetDesc, mSpaces[space].bindingLayout);
            mSpaces[space].currentHash = hash;
        }
        result[space] = mSpaces[space].bindingSet;
    }

    return result;
}

std::vector<nvrhi::BindingLayoutHandle> BindingSetManager::getBindingLayouts()
{
    std::vector<nvrhi::BindingLayoutHandle> result;
    result.resize(mSpace);
    for (uint32_t space = 0; space < mSpace; ++space)
        result[space] = mSpaces[space].bindingLayout;
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

    // Write actual textures to the descriptor table
    for (size_t i = 0; i < textures.size(); ++i)
    {
        nvrhi::BindingSetItem item = nvrhi::BindingSetItem::Texture_SRV(static_cast<uint32_t>(i), textures[i]);
        if (!mpDevice->getDevice()->writeDescriptorTable(tableInfo.descriptorTable, item))
            LOG_ERROR("[BindingSetManager] Failed to write texture {} to descriptor table '{}'", i, name);
    }

    // Fill unused slots with default texture (important for bindless to avoid unbound descriptors)
    for (size_t i = textures.size(); i < tableInfo.size; ++i)
    {
        nvrhi::BindingSetItem item = nvrhi::BindingSetItem::Texture_SRV(static_cast<uint32_t>(i), defaultTexture);
        if (!mpDevice->getDevice()->writeDescriptorTable(tableInfo.descriptorTable, item))
            LOG_ERROR("[BindingSetManager] Failed to write default texture to slot {} in descriptor table '{}'", i, name);
    }
}