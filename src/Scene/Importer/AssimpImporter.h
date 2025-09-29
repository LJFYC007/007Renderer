#pragma once
#include <assimp/Importer.hpp>

#include "Importer.h"

/*
TODO: use our logger instead of Assimp's
*/

class AssimpImporter : public Importer
{
public:
    AssimpImporter(ref<Device> pDevice) : Importer(pDevice) {}

    ref<Scene> loadScene(const std::string& fileName) override;

private:
    Assimp::Importer mImporter;
};