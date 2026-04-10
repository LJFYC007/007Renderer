#pragma once
#include <tinyusdz.hh>
#include <tydra/render-data.hh>
#include <unordered_map>
#include <unordered_set>
#include <map>

#include "Importer.h"

class UsdImporter : public Importer
{
public:
    UsdImporter(ref<Device> pDevice) : Importer(pDevice) {}

    ref<Scene> loadScene(const std::string& fileName) override;

private:
    tinyusdz::Stage mStage;
    tinyusdz::tydra::RenderScene mRenderScene;

    void traverseXformNode(const tinyusdz::tydra::XformNode& node, ref<Scene> scene);

    void extractCamera(const tinyusdz::GeomCamera* geomCamera, const tinyusdz::value::matrix4d& worldMatrix, ref<Scene> scene);

    void extractMeshGeometry(
        const tinyusdz::GeomMesh* geomMesh,
        const tinyusdz::value::matrix4d& worldMatrix,
        ref<Scene> scene,
        const std::unordered_set<int32_t>* faceFilter = nullptr
    );

    Material extractMaterial(const tinyusdz::tydra::RenderMaterial& usdMaterial, ref<Scene> scene);

    uint32_t loadTextureFromRenderScene(int32_t textureId, ref<Scene> scene);

    std::unordered_map<std::string, uint32_t> mMaterialPathToIndex;
    // Cache keyed by (textureId, channelKey) to handle ORM-packed textures where
    // the same image is referenced with different channel selectors (e.g., .g / .b)
    std::map<std::pair<int32_t, int>, uint32_t> mUsdTextureIdChannelToEngineId;
    std::unordered_map<std::string, uint32_t> mTransmissionTextureCache;
};
