#pragma once

#include "GeometryData.h"
#include "Pointer.h"

class OBJLoader
{
public:
    static ref<GeometryData> loadFromFile(const std::filesystem::path& filePath);

private:
    static bool loadOBJ(const std::filesystem::path& filePath, GeometryData& geometryData);
    static void calculateNormals(GeometryData& geometryData);
    static void validateGeometry(GeometryData& geometryData);
};
