#pragma once
#include "Core/Device.h"
#include "Core/Program/BindingSetManager.h"
#include "Core/Pointer.h"

class Pass
{
public:
    Pass(ref<Device> device) : m_Device(device) {};

    virtual void execute(uint32_t width, uint32_t height, uint32_t depth) = 0;

    // We can use pass["name"] = resourceHandle;
    class BindingSlot
    {
    public:
        BindingSlot(BindingSetManager* mgr, const std::string& name) : m_Manager(mgr), m_Name(name) {}

        BindingSlot& operator=(const nvrhi::ResourceHandle& resource)
        {
            if (m_Manager)
                m_Manager->setResourceHandle(m_Name, resource);
            return *this;
        }

    private:
        BindingSetManager* m_Manager;
        std::string m_Name;
    };

    BindingSlot operator[](const std::string& name) { return BindingSlot(m_BindingSetManager.get(), name); }

protected:
    void trackingResourceState(nvrhi::CommandListHandle commandList);

    ref<Device> m_Device;
    ref<BindingSetManager> m_BindingSetManager; // Manages binding sets and layouts
};
