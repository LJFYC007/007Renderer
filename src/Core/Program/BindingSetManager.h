#pragma once
#include <nvrhi/nvrhi.h>
#include <vector>
#include <unordered_map>

#include "Core/Device.h"
#include "Core/Program/ReflectionInfo.h"

class BindingSetManager
{
public:
    BindingSetManager(ref<Device> device, std::vector<ReflectionInfo> reflectionInfo);

    ~BindingSetManager() {}

    std::vector<nvrhi::BindingSetHandle> getBindingSets();
    std::vector<nvrhi::BindingLayoutHandle> getBindingLayouts();

    void setResourceHandle(const std::string& name, nvrhi::ResourceHandle resource);

private:
    struct SpaceData
    {
        nvrhi::BindingLayoutHandle bindingLayout;
        std::vector<nvrhi::BindingLayoutItem> layoutItems;
        std::vector<nvrhi::BindingSetItem> bindingSetItems;
        std::unordered_map<size_t, nvrhi::BindingSetHandle> bindingSets;
    };

    ref<Device> m_device;
    std::vector<SpaceData> m_spaces;
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> m_resourceMap;
    uint32_t m_space = 8;
};