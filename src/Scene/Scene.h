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

private:
    ref<Device> mpDevice;
    nvrhi::BufferHandle mVertexBuffer;
    nvrhi::BufferHandle mIndexBuffer;
    nvrhi::BufferHandle mMaterialBuffer;
    nvrhi::BufferHandle mMeshBuffer;
    nvrhi::BufferHandle mTriangleToMeshBuffer;
    nvrhi::rt::AccelStructHandle mBlas;
    nvrhi::rt::AccelStructHandle mTlas;
};
