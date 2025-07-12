#pragma once
#include <unordered_map>
#include <nvrhi/nvrhi.h>

#include "utils/Logger.h"

class RenderData
{
public:
    nvrhi::ResourceHandle getResource(const std::string& name) const
    {
        auto it = m_Resources.find(name);
        if (it != m_Resources.end())
            return it->second;
        LOG_WARN("Resource not found: {}", name);
        return nullptr;
    }

    // Non-const version for setting resources
    nvrhi::ResourceHandle& operator[](const std::string& name) { return m_Resources[name]; }

    // Const version for getting resources (returns copy to avoid modifying map)
    nvrhi::ResourceHandle operator[](const std::string& name) const
    {
        auto it = m_Resources.find(name);
        if (it != m_Resources.end())
            return it->second;
        LOG_WARN("Resource not found: {}", name);
        return nullptr;
    }

    void setResource(const std::string& name, const nvrhi::ResourceHandle& resource) { m_Resources[name] = resource; }

private:
    std::unordered_map<std::string, nvrhi::ResourceHandle> m_Resources;
};