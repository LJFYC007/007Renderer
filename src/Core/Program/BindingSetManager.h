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

    void setDescriptorTable(const std::string& name, const std::vector<nvrhi::TextureHandle>& textures, nvrhi::TextureHandle defaultTexture);

private:
    struct DescriptorTableInfo
    {
        uint32_t space;
        uint32_t index;
        nvrhi::DescriptorTableHandle descriptorTable;
        nvrhi::BindingLayoutHandle bindingLayout;
        uint32_t size;
    };

    struct SpaceData
    {
        nvrhi::BindingLayoutHandle bindingLayout;
        std::vector<nvrhi::BindingLayoutItem> layoutItems;
        std::vector<nvrhi::BindingSetItem> bindingSetItems;
        nvrhi::BindingSetHandle bindingSet;
        size_t currentHash = 0;

        std::vector<nvrhi::DescriptorTableHandle> descriptorTables;
    };

    ref<Device> mpDevice;
    std::vector<SpaceData> mSpaces;
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> mResourceMap;
    std::unordered_map<std::string, DescriptorTableInfo> mDescriptorTables;
    uint32_t mSpace = 8;
};