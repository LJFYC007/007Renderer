#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>
#include <assimp/GltfMaterial.h>

#include "AssimpImporter.h"
#include "Utils/Logger.h"

ref<Scene> AssimpImporter::loadScene(const std::string& fileName)
{
    // Configure import flags for good geometry processing
    unsigned int postProcessFlags = aiProcess_Triangulate |              // Convert polygons to triangles
                                    aiProcess_FlipUVs |                  // Flip UV coordinates (OpenGL -> D3D convention)
                                    aiProcess_GenSmoothNormals |         // Generate smooth normals if missing
                                    aiProcess_CalcTangentSpace |         // Calculate tangent and bitangent vectors
                                    aiProcess_JoinIdenticalVertices |    // Remove duplicate vertices
                                    aiProcess_ImproveCacheLocality |     // Optimize vertex cache locality
                                    aiProcess_RemoveRedundantMaterials | // Remove unused materials
                                    aiProcess_OptimizeMeshes |           // Reduce mesh count
                                    aiProcess_PreTransformVertices |     // Apply node transformations to vertex data
                                    aiProcess_ValidateDataStructure;     // Validate the imported scene
    const aiScene* aiScene = importer.ReadFile(fileName, postProcessFlags);
    if (aiScene == nullptr)
    {
        LOG_ERROR("Assimp failed to load file: {}", importer.GetErrorString());
        return nullptr;
    }

    ref<Scene> scene = make_ref<Scene>(m_device);
    // Load meshes
    scene->meshes.reserve(aiScene->mNumMeshes);
    for (unsigned int i = 0; i < aiScene->mNumMeshes; ++i)
    {
        const aiMesh* aiMesh = aiScene->mMeshes[i];

        Mesh mesh;
        // mesh.name = aiMesh->mName.C_Str();
        // if (mesh.name.empty())
        //     mesh.name = "Mesh_" + std::to_string(i);
        mesh.materialIndex = aiMesh->mMaterialIndex;

        // Current vertex offset for this mesh
        uint32_t vertexOffset = static_cast<uint32_t>(scene->vertices.size());

        // Load vertices
        for (unsigned int j = 0; j < aiMesh->mNumVertices; ++j)
        {
            Vertex vertex = {};

            // Position
            vertex.position[0] = aiMesh->mVertices[j].x;
            vertex.position[1] = aiMesh->mVertices[j].y;
            vertex.position[2] = aiMesh->mVertices[j].z;

            // Normal
            vertex.normal[0] = aiMesh->mNormals[j].x;
            vertex.normal[1] = aiMesh->mNormals[j].y;
            vertex.normal[2] = aiMesh->mNormals[j].z;

            // Texture coordinates (use first UV channel)
            vertex.texCoord[0] = aiMesh->mTextureCoords[0][j].x;
            vertex.texCoord[1] = aiMesh->mTextureCoords[0][j].y;

            scene->vertices.push_back(vertex);
        }

        // Load indices
        uint32_t currentMeshIndex = static_cast<uint32_t>(scene->meshes.size());
        for (unsigned int j = 0; j < aiMesh->mNumFaces; ++j)
        {
            const aiFace& face = aiMesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; ++k)
                scene->indices.push_back(vertexOffset + face.mIndices[k]);
            scene->triangleToMesh.push_back(currentMeshIndex);
        }

        scene->meshes.push_back(std::move(mesh));
    }

    // Load materials
    scene->materials.reserve(aiScene->mNumMaterials);
    for (unsigned int i = 0; i < aiScene->mNumMaterials; ++i)
    {
        const aiMaterial* aiMat = aiScene->mMaterials[i];
        Material material;

        // Get material name
        aiString matName;
        // if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
        //     material.name = matName.C_Str();
        // else
        //     material.name = "Material_" + std::to_string(i);

        // PBR Metallic-Roughness workflow properties (GLTF 2.0)
        aiColor3D baseColor;
        if (aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS)
            material.baseColorFactor = glm::vec3(baseColor.r, baseColor.g, baseColor.b);
        else if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS) // Fallback to diffuse
            material.baseColorFactor = glm::vec3(baseColor.r, baseColor.g, baseColor.b);

        float metallicFactor;
        if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor) == AI_SUCCESS)
            material.metallicFactor = metallicFactor;

        float roughnessFactor;
        if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor) == AI_SUCCESS)
            material.roughnessFactor = roughnessFactor;

        aiColor3D emissive; // TODO: this may have some bugs
        float emissiveIntensity;
        if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS && aiMat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveIntensity) == AI_SUCCESS)
            material.emissiveFactor = glm::vec3(emissive.r, emissive.g, emissive.b) * emissiveIntensity;

        // Log material information
        LOG_DEBUG(
            "Loaded material '{}': baseColor({}, {}, {}), metallic={}, roughness={}, emissive=({}, {}, {})",
            //  material.name,
            matName.C_Str(),
            material.baseColorFactor.r,
            material.baseColorFactor.g,
            material.baseColorFactor.b,
            material.metallicFactor,
            material.roughnessFactor,
            material.emissiveFactor.r,
            material.emissiveFactor.g,
            material.emissiveFactor.b
        );

        scene->materials.push_back(std::move(material));
    }

    // Create default material if none exist
    if (scene->materials.empty())
    {
        // Material defaultMaterial("DefaultMaterial");
        Material defaultMaterial;
        scene->materials.push_back(std::move(defaultMaterial));
        LOG_DEBUG("Created default material");
    }
    return scene;
}