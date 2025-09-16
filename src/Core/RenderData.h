#pragma once
#include <unordered_map>
#include <nvrhi/nvrhi.h>

#include "utils/Logger.h"

class RenderData
{
public:
    nvrhi::ResourceHandle getResource(const std::string& name) const
    {
        auto it = mResources.find(name);
        if (it != mResources.end())
            return it->second;
        LOG_WARN("Resource not found: {}", name);
        return nullptr;
    }

    // Non-const version for setting resources
    nvrhi::ResourceHandle& operator[](const std::string& name) { return mResources[name]; }

    // Const version for getting resources (returns copy to avoid modifying map)
    nvrhi::ResourceHandle operator[](const std::string& name) const
    {
        auto it = mResources.find(name);
        if (it != mResources.end())
            return it->second;
        LOG_WARN("Resource not found: {}", name);
        return nullptr;
    }

    void setResource(const std::string& name, const nvrhi::ResourceHandle& resource) { mResources[name] = resource; }

private:
    std::unordered_map<std::string, nvrhi::ResourceHandle> mResources;
};