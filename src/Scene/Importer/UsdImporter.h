#pragma once
#include <tinyusdz.hh>
#include <tydra/render-data.hh>
#include <unordered_map>

#include "Importer.h"

class UsdImporter : public Importer
{
public:
    UsdImporter(ref<Device> pDevice) : Importer(pDevice) {}

    ref<Scene> loadScene(const std::string& fileName) override;

private:
    tinyusdz::Stage mStage;

    void traverseXformNode(const tinyusdz::tydra::XformNode& node, ref<Scene> scene);

    void extractCamera(const tinyusdz::GeomCamera* geomCamera, const tinyusdz::value::matrix4d& worldMatrix, ref<Scene> scene);

    void extractMeshGeometry(const tinyusdz::GeomMesh* geomMesh, const tinyusdz::value::matrix4d& worldMatrix, ref<Scene> scene);

    Material extractMaterial(const tinyusdz::tydra::RenderMaterial& usdMaterial);

    std::unordered_map<std::string, uint32_t> mMaterialPathToIndex;
};
