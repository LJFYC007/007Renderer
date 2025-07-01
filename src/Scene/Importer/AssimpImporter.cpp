#include <assimp/scene.h>
#include <assimp/postprocess.h>

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
        mesh.name = aiMesh->mName.C_Str();
        if (mesh.name.empty())
            mesh.name = "Mesh_" + std::to_string(i);
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
        // mesh.indices.reserve(aiMesh->mNumFaces * 3);
        for (unsigned int j = 0; j < aiMesh->mNumFaces; ++j)
        {
            const aiFace& face = aiMesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; ++k)
                scene->indices.push_back(vertexOffset + face.mIndices[k]);
        }
        scene->meshes.push_back(std::move(mesh));
    }
    return scene;
}