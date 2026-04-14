#pragma once
#include <vector>
#include <nvrhi/nvrhi.h>

#include "Core/Device.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Material/Material.h"
#include "Scene/Material/TextureManager.h"
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

struct EmissiveTriangle
{
    uint32_t triangleIndex; // Global primitive index (PrimitiveIndex() in shader)
    float area;             // World-space triangle area
    float cdfUpper;         // Cumulative area / totalArea (upper bound of CDF bin)
    float _padding;         // Pad to 16 bytes for GPU alignment
};

class Scene
{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<uint32_t> triangleToMesh;
    std::vector<EmissiveTriangle> emissiveTriangles;
    float totalEmissiveArea = 0.f;
    ref<Camera> camera;
    std::string name;

    Scene(ref<Device> pDevice);

    void buildAccelStructs();

    nvrhi::rt::AccelStructHandle getBLAS() const { return mBlas; }
    nvrhi::rt::AccelStructHandle getTLAS() const { return mTlas; }

    // Get geometry buffers for shader access
    nvrhi::BufferHandle getVertexBuffer() const { return mVertexBuffer; }
    nvrhi::BufferHandle getIndexBuffer() const { return mIndexBuffer; }
    nvrhi::BufferHandle getMaterialBuffer() const { return mMaterialBuffer; }
    nvrhi::BufferHandle getMeshBuffer() const { return mMeshBuffer; }
    nvrhi::BufferHandle getTriangleToMeshBuffer() const { return mTriangleToMeshBuffer; }
    nvrhi::BufferHandle getEmissiveTriangleBuffer() const { return mEmissiveTriangleBuffer; }
    uint32_t getEmissiveTriangleCount() const { return static_cast<uint32_t>(emissiveTriangles.size()); }

    // Get material by index
    const Material& getMaterial(uint32_t index) const
    {
        if (index < materials.size())
            return materials[index];
        static Material defaultMaterial;
        return defaultMaterial;
    }

    // Texture management
    uint32_t loadTexture(const float* data, uint32_t width, uint32_t height, uint32_t channels, const std::string& debugName = "");
    nvrhi::TextureHandle getTexture(uint32_t textureId) const;
    const std::vector<nvrhi::TextureHandle>& getTextures() const;
    size_t getTextureCount() const;
    nvrhi::TextureHandle getDefaultTexture() const;

    // Get texture manager for direct access
    ref<TextureManager> getTextureManager() const { return mTextureManager; }

private:
    ref<Device> mpDevice;
    ref<TextureManager> mTextureManager;
    nvrhi::BufferHandle mVertexBuffer;
    nvrhi::BufferHandle mIndexBuffer;
    nvrhi::BufferHandle mMaterialBuffer;
    nvrhi::BufferHandle mMeshBuffer;
    nvrhi::BufferHandle mTriangleToMeshBuffer;
    nvrhi::BufferHandle mEmissiveTriangleBuffer;
    nvrhi::rt::AccelStructHandle mBlas;
    nvrhi::rt::AccelStructHandle mTlas;
};
