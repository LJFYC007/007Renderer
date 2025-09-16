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
        nvrhi::BindingSetHandle bindingSet;
        size_t currentHash = 0;
    };

    ref<Device> mpDevice;
    std::vector<SpaceData> mSpaces;
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> mResourceMap;
    uint32_t mSpace = 8;
};