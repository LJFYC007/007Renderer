#include "BindingSetManager.h"
#include "Utils/Logger.h"

BindingSetManager::BindingSetManager(ref<Device> device, std::vector<ReflectionInfo> reflectionInfo) : m_device(device)
{
    m_spaces.resize(m_space);

    // Group reflection info by binding space
    for (const auto& info : reflectionInfo)
    {
        uint32_t space = info.bindingSpace;
        uint32_t index = static_cast<uint32_t>(m_spaces[space].layoutItems.size());

        m_spaces[space].layoutItems.push_back(info.bindingLayoutItem);
        m_spaces[space].bindingSetItems.push_back(info.bindingSetItem);
        m_resourceMap[info.name] = {space, index};
    }

    // Create binding layouts for each space
    for (uint32_t space = 0; space < m_space; ++space)
    {
        if (m_spaces[space].layoutItems.empty())
            continue;

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.bindings = m_spaces[space].layoutItems;
        m_spaces[space].bindingLayout = m_device->getDevice()->createBindingLayout(layoutDesc);
    }
}

std::vector<nvrhi::BindingSetHandle> BindingSetManager::getBindingSets()
{
    std::vector<nvrhi::BindingSetHandle> result;
    result.resize(m_space);

    for (uint32_t space = 0; space < m_space; ++space)
    {
        if (m_spaces[space].layoutItems.empty())
        {
            result[space] = nullptr;
            continue;
        }

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.bindings = m_spaces[space].bindingSetItems;

        size_t hash = 0;
        nvrhi::hash_combine(hash, bindingSetDesc);
        nvrhi::hash_combine(hash, m_spaces[space].bindingLayout);

        if (m_spaces[space].bindingSets.find(hash) != m_spaces[space].bindingSets.end())
        {
            result[space] = m_spaces[space].bindingSets[hash];
        }
        else
        {
            LOG_DEBUG("[BindingSetManager] Creating new binding set for space {} with hash: {}", space, hash);
            m_spaces[space].bindingSets[hash] = m_device->getDevice()->createBindingSet(bindingSetDesc, m_spaces[space].bindingLayout);
            result[space] = m_spaces[space].bindingSets[hash];
        }
    }

    return result;
}

std::vector<nvrhi::BindingLayoutHandle> BindingSetManager::getBindingLayouts()
{
    std::vector<nvrhi::BindingLayoutHandle> result;
    result.resize(m_space);
    for (uint32_t space = 0; space < m_space; ++space)
        result[space] = m_spaces[space].bindingLayout;
    return result;
}

void BindingSetManager::setResourceHandle(const std::string& name, nvrhi::ResourceHandle resource)
{
    auto it = m_resourceMap.find(name);
    if (it == m_resourceMap.end())
        LOG_ERROR_RETURN("[BindingSetManager] Resource '{}' not found in layout", name);

    uint32_t space = it->second.first;
    uint32_t index = it->second.second;
    m_spaces[space].bindingSetItems[index].resourceHandle = resource;
}