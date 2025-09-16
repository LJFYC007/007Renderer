#pragma once
#include <string>
#include <assimp/Importer.hpp>

#include "Scene/Scene.h"
#include "Core/Pointer.h"
#include "Core/Device.h"

/*
TODO: use our logger instead of Assimp's
*/

class AssimpImporter
{
public:
    AssimpImporter(ref<Device> pDevice) : mpDevice(pDevice) {}

    ref<Scene> loadScene(const std::string& fileName);

private:
    Assimp::Importer mImporter;
    ref<Device> mpDevice;
};