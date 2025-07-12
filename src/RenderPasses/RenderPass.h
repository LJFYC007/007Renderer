#pragma once
#include "Core/Device.h"
#include "Core/RenderData.h"
#include "Scene/Scene.h"
#include "Utils/GUI.h"

class RenderPass
{
public:
    RenderPass(ref<Device> device) : m_Device(device) {};

    virtual RenderData execute(const RenderData& input = RenderData()) = 0;

    virtual void renderUI() = 0;

    virtual void setScene(ref<Scene> scene) { m_Scene = scene; }

protected:
    ref<Device> m_Device;
    ref<Scene> m_Scene;
};