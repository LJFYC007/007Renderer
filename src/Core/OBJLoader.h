#pragma once

#include "GeometryData.h"
#include <memory>

class OBJLoader
{
public:
    static std::unique_ptr<GeometryData> loadFromFile(const std::filesystem::path& filePath);

private:
    static bool loadOBJ(const std::filesystem::path& filePath, GeometryData& geometryData);
    static void calculateNormals(GeometryData& geometryData);
    static void validateGeometry(GeometryData& geometryData);
};
