#pragma once

#include <vector>
#include <string>
#include <filesystem>

struct Vertex
{
    float position[3];
    float texCoord[2];
    float normal[3]; // Add normal for proper lighting and shading
};

struct Material
{
    std::string name;
    float diffuse[3] = {0.8f, 0.8f, 0.8f};  // Default white-ish diffuse
    float specular[3] = {0.0f, 0.0f, 0.0f}; // Default no specular
    float emission[3] = {0.0f, 0.0f, 0.0f}; // Default no emission
    float shininess = 1.0f;
    std::string diffuseTexturePath;
    std::string normalTexturePath;
    std::string specularTexturePath;
};

struct SubMesh
{
    std::vector<uint32_t> indices;
    uint32_t materialIndex = 0; // Index into materials array
    std::string name;
};

struct GeometryData
{
    std::vector<Vertex> vertices;
    std::vector<SubMesh> subMeshes;
    std::vector<Material> materials;
    std::string name;

    // Get total index count across all sub-meshes
    size_t getTotalIndexCount() const
    {
        size_t count = 0;
        for (const auto& subMesh : subMeshes)
            count += subMesh.indices.size();
        return count;
    }

    // Get flattened indices (useful for single-mesh rendering)
    std::vector<uint32_t> getFlattenedIndices() const
    {
        std::vector<uint32_t> flatIndices;
        flatIndices.reserve(getTotalIndexCount());

        for (const auto& subMesh : subMeshes)
            flatIndices.insert(flatIndices.end(), subMesh.indices.begin(), subMesh.indices.end());

        return flatIndices;
    }

    // Check if geometry has valid data
    bool isValid() const { return !vertices.empty() && !subMeshes.empty(); }

    // Clear all data
    void clear()
    {
        vertices.clear();
        subMeshes.clear();
        materials.clear();
        name.clear();
    }
};
