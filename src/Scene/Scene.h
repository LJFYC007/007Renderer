#pragma once
#include <vector>
#include <nvrhi/nvrhi.h>

#include "Core/Device.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Material/Material.h"
#include "Core/Pointer.h"

struct Vertex
{
    float position[3];
    float texCoord[2];
    float normal[3];
};

struct Mesh
{
    uint32_t materialIndex;
    // std::string name;
};

class Scene
{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<uint32_t> triangleToMesh;
    ref<Camera> camera;
    std::string name;

    Scene(ref<Device> pDevice) : mpDevice(pDevice) {}

    void buildAccelStructs();

    nvrhi::rt::AccelStructHandle getBLAS() const { return mBlas; }
    nvrhi::rt::AccelStructHandle getTLAS() const { return mTlas; }

    // Get geometry buffers for shader access
    nvrhi::BufferHandle getVertexBuffer() const { return mVertexBuffer; }
    nvrhi::BufferHandle getIndexBuffer() const { return mIndexBuffer; }
    nvrhi::BufferHandle getMaterialBuffer() const { return mMaterialBuffer; }
    nvrhi::BufferHandle getMeshBuffer() const { return mMeshBuffer; }
    nvrhi::BufferHandle getTriangleToMeshBuffer() const { return mTriangleToMeshBuffer; }

    // Get material by index
    const Material& getMaterial(uint32_t index) const
    {
        if (index < materials.size())
            return materials[index];
        static Material defaultMaterial;
        return defaultMaterial;
    }

    uint32_t loadTexture(const float* data, uint32_t width, uint32_t height, uint32_t channels, const std::string& debugName = "");

    // Get texture by ID
    nvrhi::TextureHandle getTexture(uint32_t textureId) const
    {
        if (textureId < mTextures.size())
            return mTextures[textureId];
        return nullptr;
    }

    // Get all textures
    const std::vector<nvrhi::TextureHandle>& getTextures() const { return mTextures; }
    size_t getTextureCount() const { return mTextures.size(); }

    // Get default texture (for unfilled texture slots)
    nvrhi::TextureHandle getDefaultTexture() const { return mDefaultTexture; }

private:
    ref<Device> mpDevice;
    nvrhi::BufferHandle mVertexBuffer;
    nvrhi::BufferHandle mIndexBuffer;
    nvrhi::BufferHandle mMaterialBuffer;
    nvrhi::BufferHandle mMeshBuffer;
    nvrhi::BufferHandle mTriangleToMeshBuffer;
    nvrhi::rt::AccelStructHandle mBlas;
    nvrhi::rt::AccelStructHandle mTlas;
    std::vector<nvrhi::TextureHandle> mTextures;
    nvrhi::TextureHandle mDefaultTexture; // Default 1x1 white texture for unused slots
};
