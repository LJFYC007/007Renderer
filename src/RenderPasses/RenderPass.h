#pragma once
#include "Core/Device.h"
#include "Core/RenderData.h"

class RenderPass
{
public:
    RenderPass(ref<Device> device) : m_Device(device) {};

    virtual RenderData execute(const RenderData& input) = 0;

    virtual void renderUI() = 0;

protected:
    ref<Device> m_Device;
};