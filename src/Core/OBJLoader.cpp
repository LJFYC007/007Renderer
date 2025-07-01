#include "OBJLoader.h"
#include "../Utils/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

ref<GeometryData> OBJLoader::loadFromFile(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath))
    {
        LOG_ERROR("OBJ file does not exist: {}", filePath.string());
        return nullptr;
    }

    auto geometryData = make_ref<GeometryData>();
    geometryData->name = filePath.stem().string();

    if (!loadOBJ(filePath, *geometryData))
    {
        LOG_ERROR("Failed to load OBJ file: {}", filePath.string());
        return nullptr;
    }

    validateGeometry(*geometryData);

    LOG_INFO(
        "Successfully loaded OBJ: {} ({} vertices, {} sub-meshes, {} materials)",
        filePath.filename().string(),
        geometryData->vertices.size(),
        geometryData->subMeshes.size(),
        geometryData->materials.size()
    );

    return geometryData;
}

bool OBJLoader::loadOBJ(const std::filesystem::path& filePath, GeometryData& geometryData)
{
    Assimp::Importer importer;

    // Configure import flags for good geometry processing
    unsigned int postProcessFlags = aiProcess_Triangulate |              // Convert polygons to triangles
                                    aiProcess_FlipUVs |                  // Flip UV coordinates (OpenGL -> D3D convention)
                                    aiProcess_GenSmoothNormals |         // Generate smooth normals if missing
                                    aiProcess_CalcTangentSpace |         // Calculate tangent and bitangent vectors
                                    aiProcess_JoinIdenticalVertices |    // Remove duplicate vertices
                                    aiProcess_ImproveCacheLocality |     // Optimize vertex cache locality
                                    aiProcess_RemoveRedundantMaterials | // Remove unused materials
                                    aiProcess_OptimizeMeshes |           // Reduce mesh count
                                    aiProcess_ValidateDataStructure;     // Validate the imported data

    const aiScene* scene = importer.ReadFile(filePath.string(), postProcessFlags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        LOG_ERROR("Assimp failed to load file: {}", importer.GetErrorString());
        return false;
    }

    // Load materials first
    geometryData.materials.reserve(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* aiMat = scene->mMaterials[i];
        Material material;

        // Get material name
        aiString name;
        if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
            material.name = name.C_Str();
        else
            material.name = "Material_" + std::to_string(i);

        // Get diffuse color
        aiColor3D diffuse(0.8f, 0.8f, 0.8f);
        aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
        material.diffuse[0] = diffuse.r;
        material.diffuse[1] = diffuse.g;
        material.diffuse[2] = diffuse.b;

        // Get specular color
        aiColor3D specular(0.0f, 0.0f, 0.0f);
        aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
        material.specular[0] = specular.r;
        material.specular[1] = specular.g;
        material.specular[2] = specular.b;

        // Get emission color
        aiColor3D emission(0.0f, 0.0f, 0.0f);
        aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emission);
        material.emission[0] = emission.r;
        material.emission[1] = emission.g;
        material.emission[2] = emission.b;

        // Get shininess
        float shininess = 1.0f;
        aiMat->Get(AI_MATKEY_SHININESS, shininess);
        material.shininess = glm::max(shininess, 1.0f);

        // Get texture paths (for future use)
        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
        {
            auto parentPath = filePath.parent_path();
            material.diffuseTexturePath = (parentPath / texPath.C_Str()).string();
        }

        geometryData.materials.push_back(material);
    }

    // If no materials, add a default one
    if (geometryData.materials.empty())
    {
        Material defaultMaterial;
        defaultMaterial.name = "DefaultMaterial";
        geometryData.materials.push_back(defaultMaterial);
    }

    // Load meshes
    geometryData.subMeshes.reserve(scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];

        SubMesh subMesh;
        subMesh.name = mesh->mName.C_Str();
        if (subMesh.name.empty())
            subMesh.name = "SubMesh_" + std::to_string(i);

        // Material index
        subMesh.materialIndex = mesh->mMaterialIndex;
        if (subMesh.materialIndex >= geometryData.materials.size())
            subMesh.materialIndex = 0;

        // Current vertex offset for this sub-mesh
        uint32_t vertexOffset = static_cast<uint32_t>(geometryData.vertices.size());

        // Load vertices
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            Vertex vertex = {};

            // Position
            vertex.position[0] = mesh->mVertices[j].x;
            vertex.position[1] = mesh->mVertices[j].y;
            vertex.position[2] = mesh->mVertices[j].z;

            // Normal
            if (mesh->HasNormals())
            {
                vertex.normal[0] = mesh->mNormals[j].x;
                vertex.normal[1] = mesh->mNormals[j].y;
                vertex.normal[2] = mesh->mNormals[j].z;
            }
            else
            {
                // Default normal pointing up
                vertex.normal[0] = 0.0f;
                vertex.normal[1] = 1.0f;
                vertex.normal[2] = 0.0f;
            }

            // Texture coordinates (use first UV channel)
            if (mesh->HasTextureCoords(0))
            {
                vertex.texCoord[0] = mesh->mTextureCoords[0][j].x;
                vertex.texCoord[1] = mesh->mTextureCoords[0][j].y;
            }
            else
            {
                vertex.texCoord[0] = 0.0f;
                vertex.texCoord[1] = 0.0f;
            }

            geometryData.vertices.push_back(vertex);
        }

        // Load indices
        subMesh.indices.reserve(mesh->mNumFaces * 3);
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const aiFace& face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; ++k)
                subMesh.indices.push_back(vertexOffset + face.mIndices[k]);
        }

        geometryData.subMeshes.push_back(std::move(subMesh));
    }

    // Calculate normals if they weren't provided or seem incorrect
    if (!geometryData.vertices.empty() && geometryData.vertices[0].normal[0] == 0.0f && geometryData.vertices[0].normal[1] == 1.0f &&
        geometryData.vertices[0].normal[2] == 0.0f)
        calculateNormals(geometryData);
    return true;
}

void OBJLoader::calculateNormals(GeometryData& geometryData)
{
    // Reset all normals to zero
    for (auto& vertex : geometryData.vertices)
        vertex.normal[0] = vertex.normal[1] = vertex.normal[2] = 0.0f;

    // Calculate face normals and accumulate to vertex normals
    for (const auto& subMesh : geometryData.subMeshes)
    {
        for (size_t i = 0; i < subMesh.indices.size(); i += 3)
        {
            uint32_t i0 = subMesh.indices[i];
            uint32_t i1 = subMesh.indices[i + 1];
            uint32_t i2 = subMesh.indices[i + 2];

            if (i0 >= geometryData.vertices.size() || i1 >= geometryData.vertices.size() || i2 >= geometryData.vertices.size())
                continue;

            const Vertex& v0 = geometryData.vertices[i0];
            const Vertex& v1 = geometryData.vertices[i1];
            const Vertex& v2 = geometryData.vertices[i2];

            // Calculate edge vectors
            glm::vec3 edge1(v1.position[0] - v0.position[0], v1.position[1] - v0.position[1], v1.position[2] - v0.position[2]);

            glm::vec3 edge2(v2.position[0] - v0.position[0], v2.position[1] - v0.position[1], v2.position[2] - v0.position[2]);

            // Calculate face normal using cross product
            glm::vec3 faceNormal = glm::cross(edge1, edge2);

            // Accumulate to vertex normals
            geometryData.vertices[i0].normal[0] += faceNormal.x;
            geometryData.vertices[i0].normal[1] += faceNormal.y;
            geometryData.vertices[i0].normal[2] += faceNormal.z;

            geometryData.vertices[i1].normal[0] += faceNormal.x;
            geometryData.vertices[i1].normal[1] += faceNormal.y;
            geometryData.vertices[i1].normal[2] += faceNormal.z;

            geometryData.vertices[i2].normal[0] += faceNormal.x;
            geometryData.vertices[i2].normal[1] += faceNormal.y;
            geometryData.vertices[i2].normal[2] += faceNormal.z;
        }
    }

    // Normalize all vertex normals
    for (auto& vertex : geometryData.vertices)
    {
        glm::vec3 normal(vertex.normal[0], vertex.normal[1], vertex.normal[2]);
        normal = glm::normalize(normal);

        vertex.normal[0] = normal.x;
        vertex.normal[1] = normal.y;
        vertex.normal[2] = normal.z;
    }
}

void OBJLoader::validateGeometry(GeometryData& geometryData)
{
    if (geometryData.vertices.empty())
        LOG_WARN_RETURN("Geometry has no vertices");
    if (geometryData.subMeshes.empty())
        LOG_WARN_RETURN("Geometry has no sub-meshes");

    // Check index bounds
    uint32_t maxIndex = static_cast<uint32_t>(geometryData.vertices.size() - 1);
    for (const auto& subMesh : geometryData.subMeshes)
    {
        for (uint32_t index : subMesh.indices)
        {
            if (index > maxIndex)
            {
                LOG_ERROR("Invalid index {} (max: {}) in sub-mesh '{}'", index, maxIndex, subMesh.name);
            }
        }
    }

    LOG_DEBUG(
        "Geometry validation passed for '{}' ({} vertices, {} indices)",
        geometryData.name,
        geometryData.vertices.size(),
        geometryData.getTotalIndexCount()
    );
}
