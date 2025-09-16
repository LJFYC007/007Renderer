#include "BindingSetManager.h"
#include "Utils/Logger.h"

BindingSetManager::BindingSetManager(ref<Device> pDevice, std::vector<ReflectionInfo> reflectionInfo) : mpDevice(pDevice)
{
    mSpaces.resize(mSpace);

    // Group reflection info by binding space
    for (const auto& info : reflectionInfo)
    {
        uint32_t space = info.bindingSpace;
        uint32_t index = static_cast<uint32_t>(mSpaces[space].layoutItems.size());

        mSpaces[space].layoutItems.push_back(info.bindingLayoutItem);
        mSpaces[space].bindingSetItems.push_back(info.bindingSetItem);
        mResourceMap[info.name] = {space, index};
    }

    // Create binding layouts for each space
    for (uint32_t space = 0; space < mSpace; ++space)
    {
        if (mSpaces[space].layoutItems.empty())
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