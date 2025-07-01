#pragma once
#include <nvrhi/nvrhi.h>
#include <vector>
#include <unordered_map>

#include "Core/Device.h"
#include "Core/Pointer.h"

class BindingSetManager
{
public:
    BindingSetManager(
        ref<Device> device,
        std::vector<nvrhi::BindingLayoutItem> bindingLayoutItems,
        std::unordered_map<std::string, nvrhi::BindingSetItem> bindingMap
    );

    ~BindingSetManager() {}

    nvrhi::BindingSetHandle getBindingSet();
    nvrhi::IBindingLayout* getBindingLayout() { return m_BindingLayout.Get(); }

    void setResourceHandle(const std::string& name, nvrhi::ResourceHandle resource);

private:
    ref<Device> m_device;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    std::vector<nvrhi::BindingLayoutItem> m_LayoutItems;
    std::unordered_map<size_t, nvrhi::BindingSetHandle> m_BindingSets;
    std::vector<nvrhi::BindingSetItem> m_BindingSetItems;
    std::unordered_map<std::string, uint32_t> m_IndexMap;
};