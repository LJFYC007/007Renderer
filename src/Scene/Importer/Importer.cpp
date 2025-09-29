#include "Importer.h"
#include "AssimpImporter.h"
#include "UsdImporter.h"
#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace
{
// USD file extensions
const std::unordered_set<std::string> kUsdExtensions = {".usd", ".usda", ".usdc", ".usdz"};

// Assimp supported file extensions (commonly used ones)
const std::unordered_set<std::string> kAssimpExtensions = {".gltf", ".glb", ".fbx", ".obj", ".3ds", ".dae", ".x",   ".blend", ".ase", ".ifc",  ".xgl",
                                                           ".zgl",  ".ply", ".dxf", ".lwo", ".lws", ".lxo", ".stl", ".x",     ".ac",  ".ms3d", ".cob",
                                                           ".scn",  ".md2", ".md3", ".pk3", ".mdc", ".md5", ".smd", ".vta",   ".m3",  ".3d",   ".b3d",
                                                           ".q3d",  ".q3s", ".nff", ".off", ".raw", ".ter", ".mdl", ".hmp",   ".ndo"};

std::string getFileExtension(const std::string& fileName)
{
    std::filesystem::path filePath(fileName);
    std::string extension = filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    return extension;
}

enum class ImporterType
{
    USD,
    Assimp,
    Unknown
};

ImporterType determineImporterType(const std::string& fileName)
{
    std::string extension = getFileExtension(fileName);
    if (kUsdExtensions.find(extension) != kUsdExtensions.end())
        return ImporterType::USD;
    else if (kAssimpExtensions.find(extension) != kAssimpExtensions.end())
        return ImporterType::Assimp;
    return ImporterType::Unknown;
}
} // namespace

ref<Scene> loadSceneWithImporter(const std::string& fileName, ref<Device> pDevice)
{
    ImporterType importerType = determineImporterType(fileName);

    switch (importerType)
    {
    case ImporterType::USD:
    {
        auto usdImporter = std::make_unique<UsdImporter>(pDevice);
        return usdImporter->loadScene(fileName);
    }
    case ImporterType::Assimp:
    {
        auto assimpImporter = std::make_unique<AssimpImporter>(pDevice);
        return assimpImporter->loadScene(fileName);
    }
    case ImporterType::Unknown:
    default:
        throw std::runtime_error("Unsupported file format: " + getFileExtension(fileName) + " for file: " + fileName);
    }
}
