#pragma once
#include <vector>
#include <glm/glm.hpp>
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

struct MeshDesc
{
    uint32_t indexOffset;
    uint32_t indexCount;
};

struct MeshInstance
{
    uint32_t meshID;
    uint32_t materialIndex;
    glm::mat4 localToWorld{1.0f};
};

// 3x4 row-major transform packed as three float4 rows (48 B) + tail (16 B) = 64 B.
struct InstanceData
{
    glm::vec4 row0;
    glm::vec4 row1;
    glm::vec4 row2;
    uint32_t meshID;
    uint32_t materialID;
    uint32_t _padding0;
    uint32_t _padding1;
};
static_assert(sizeof(InstanceData) == 64, "InstanceData must be 64 B to match Slang row-major float3x4 + 16-byte tail");

struct EmissiveTriangle
{
    uint32_t instanceID;
    uint32_t localTriangleIndex;
    float area;
    float cdfUpper;
};

class Scene
{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<MeshDesc> meshes;
    std::vector<MeshInstance> instances;
    std::vector<Material> materials;
    std::vector<EmissiveTriangle> emissiveTriangles;
    float totalEmissiveArea = 0.f;
    ref<Camera> camera;
    std::string name;

    Scene(ref<Device> pDevice);

    void addMeshInstance(uint32_t indexOffset, uint32_t indexCount, uint32_t materialIndex, const glm::mat4& localToWorld = glm::mat4(1.0f));

    void buildAccelStructs();

    nvrhi::rt::AccelStructHandle getTLAS() const { return mTlas; }

    // Get geometry buffers for shader access
    nvrhi::BufferHandle getVertexBuffer() const { return mVertexBuffer; }
    nvrhi::BufferHandle getIndexBuffer() const { return mIndexBuffer; }
    nvrhi::BufferHandle getMaterialBuffer() const { return mMaterialBuffer; }
    nvrhi::BufferHandle getMeshBuffer() const { return mMeshBuffer; }
    nvrhi::BufferHandle getInstanceBuffer() const { return mInstanceBuffer; }
    nvrhi::BufferHandle getEmissiveTriangleBuffer() const { return mEmissiveTriangleBuffer; }
    uint32_t getEmissiveTriangleCount() const { return static_cast<uint32_t>(emissiveTriangles.size()); }
    uint64_t getTriangleCount() const { return indices.size() / 3; }

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
    nvrhi::BufferHandle mInstanceBuffer;
    nvrhi::BufferHandle mEmissiveTriangleBuffer;
    std::vector<nvrhi::rt::AccelStructHandle> mBlases;
    nvrhi::rt::AccelStructHandle mTlas;
};
