#pragma once
#include <nvrhi/nvrhi.h>
#include <map>
#include <vector>
#include <unordered_map>

#include "Core/Device.h"
#include "Core/Program/ReflectionInfo.h"

// Each register space holds EITHER one descriptor table OR one or more regular
// bindings — never both, and never multiple descriptor tables. Slang's
// ParameterBlock<> convention gives each bindless array its own space, so this
// lines up with how shaders in this project are written (see gMaterialTextures
// / gMaterialSampler in Material.slang). The constructor asserts this.
class BindingSetManager
{
public:
    BindingSetManager(ref<Device> device, const std::vector<ReflectionInfo>& reflectionInfo);

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
    std::map<uint32_t, SpaceData> mSpaces;
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> mResourceMap;
    std::unordered_map<std::string, DescriptorTableInfo> mDescriptorTables;
};