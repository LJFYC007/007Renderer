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

    Scene(ref<Device> device) : m_device(device) {}

    void buildAccelStructs();

    nvrhi::rt::AccelStructHandle getBLAS() const { return m_blas; }
    nvrhi::rt::AccelStructHandle getTLAS() const { return m_tlas; }

    // Get geometry buffers for shader access
    nvrhi::BufferHandle getVertexBuffer() const { return m_vertexBuffer; }
    nvrhi::BufferHandle getIndexBuffer() const { return m_indexBuffer; }
    nvrhi::BufferHandle getMaterialBuffer() const { return m_materialBuffer; }
    nvrhi::BufferHandle getMeshBuffer() const { return m_meshBuffer; }
    nvrhi::BufferHandle getTriangleToMeshBuffer() const { return m_triangleToMeshBuffer; }

    // Get material by index
    const Material& getMaterial(uint32_t index) const
    {
        if (index < materials.size())
            return materials[index];
        static Material defaultMaterial;
        return defaultMaterial;
    }

private:
    ref<Device> m_device;
    nvrhi::BufferHandle m_vertexBuffer;
    nvrhi::BufferHandle m_indexBuffer;
    nvrhi::BufferHandle m_materialBuffer;
    nvrhi::BufferHandle m_meshBuffer;
    nvrhi::BufferHandle m_triangleToMeshBuffer;
    nvrhi::rt::AccelStructHandle m_blas;
    nvrhi::rt::AccelStructHandle m_tlas;
};
