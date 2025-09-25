#pragma once
#include "Core/Device.h"
#include "Core/Program/BindingSetManager.h"
#include "Core/Pointer.h"

struct ConstantBuffer
{
    nvrhi::BufferHandle buffer;
    void* pData;
    size_t sizeBytes;
};

class Pass
{
public:
    Pass(ref<Device> pDevice) : mpDevice(pDevice) {};

    virtual void execute(uint32_t width, uint32_t height, uint32_t depth) = 0;

    void addConstantBuffer(nvrhi::BufferHandle buffer, void* pData, size_t sizeBytes) { mConstantBuffers.push_back({buffer, pData, sizeBytes}); }

    // We can use pass["name"] = resourceHandle;
    class BindingSlot
    {
    public:
        BindingSlot(BindingSetManager* pMgr, const std::string& name) : mpManager(pMgr), mName(name) {}

        BindingSlot& operator=(const nvrhi::ResourceHandle& resource)
        {
            if (mpManager)
                mpManager->setResourceHandle(mName, resource);
            return *this;
        }

    private:
        BindingSetManager* mpManager;
        std::string mName;
    };

    BindingSlot operator[](const std::string& name) { return BindingSlot(mpBindingSetManager.get(), name); }

protected:
    ref<Device> mpDevice;
    ref<BindingSetManager> mpBindingSetManager; // Manages binding sets and layouts
    std::vector<ConstantBuffer> mConstantBuffers;
};
