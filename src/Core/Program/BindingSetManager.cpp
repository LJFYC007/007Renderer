#include "BindingSetManager.h"
#include "Utils/Logger.h"

BindingSetManager::BindingSetManager(
    ref<Device> device,
    std::vector<nvrhi::BindingLayoutItem> bindingLayoutItems,
    std::unordered_map<std::string, nvrhi::BindingSetItem> bindingMap
)
    : m_device(device), m_LayoutItems(bindingLayoutItems)
{
    uint32_t index = 0;
    for (const auto& [name, item] : bindingMap)
    {
        m_BindingSetItems.push_back(item);
        m_IndexMap[name] = index++;
    }

    // Create binding layout
    nvrhi::BindingLayoutDesc layoutDesc;
    layoutDesc.visibility = nvrhi::ShaderType::All;
    layoutDesc.bindings = m_LayoutItems;
    m_BindingLayout = m_device->getDevice()->createBindingLayout(layoutDesc);
}

nvrhi::BindingSetHandle BindingSetManager::getBindingSet()
{
    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.bindings = m_BindingSetItems;

    size_t hash = 0;
    nvrhi::hash_combine(hash, bindingSetDesc);
    nvrhi::hash_combine(hash, m_BindingLayout);
    if (m_BindingSets.find(hash) != m_BindingSets.end())
        return m_BindingSets[hash];

    LOG_DEBUG("[BindingSetManager] Creating new binding set with hash: {}", hash);
    m_BindingSets[hash] = m_device->getDevice()->createBindingSet(bindingSetDesc, m_BindingLayout.Get());
    return m_BindingSets[hash];
}

void BindingSetManager::setResourceHandle(const std::string& name, nvrhi::ResourceHandle resource)
{
    auto it = m_IndexMap.find(name);
    if (it == m_IndexMap.end())
        LOG_ERROR_RETURN("[BindingSetManager] Resource '{}' not found in layout", name);

    uint32_t index = it->second;
    m_BindingSetItems[index].resourceHandle = resource;
}