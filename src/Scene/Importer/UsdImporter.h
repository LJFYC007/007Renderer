#pragma once
#include <string>
#include <tinyusdz.hh>
#include <tydra/scene-access.hh>

#include "Scene/Scene.h"
#include "Core/Pointer.h"
#include "Core/Device.h"

class UsdImporter
{
public:
    UsdImporter(ref<Device> pDevice) : mpDevice(pDevice) {}

    ref<Scene> loadScene(const std::string& fileName);

private:
    tinyusdz::Stage mStage;
    ref<Device> mpDevice;

    void extractCameras(const tinyusdz::tydra::XformNode& node, ref<Scene> scene);
    void traverseXformNode(const tinyusdz::tydra::XformNode& node, ref<Scene> scene);
    void extractMeshGeometry(const tinyusdz::GeomMesh& geomMesh, const tinyusdz::value::matrix4d& worldMatrix, ref<Scene> scene);
};