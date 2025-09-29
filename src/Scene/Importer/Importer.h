#pragma once
#include <string>

#include "Scene/Scene.h"
#include "Core/Pointer.h"
#include "Core/Device.h"

class Importer
{
public:
    Importer(ref<Device> pDevice) : mpDevice(pDevice) {}

    virtual ref<Scene> loadScene(const std::string& fileName) { return nullptr; }

protected:
    ref<Device> mpDevice;
};

ref<Scene> loadSceneWithImporter(const std::string& fileName, ref<Device> pDevice);