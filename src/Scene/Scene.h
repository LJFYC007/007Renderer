#pragma once
#include <vector>
#include <nvrhi/nvrhi.h>

#include "Core/Device.h"

struct Vertex
{
    float position[3];
    float texCoord[2];
    float normal[3];
};

struct Mesh
{
    uint32_t materialIndex;
    std::string name;
};

class Scene
{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh> meshes;
    std::string name;

    Scene(ref<Device> device) : m_device(device) {}

    void buildAccelStructs();

    nvrhi::rt::AccelStructHandle getBLAS() const { return m_blas; }
    nvrhi::rt::AccelStructHandle getTLAS() const { return m_tlas; }

    // Get geometry buffers for shader access
    nvrhi::BufferHandle getVertexBuffer() const { return m_vertexBuffer; }
    nvrhi::BufferHandle getIndexBuffer() const { return m_indexBuffer; }

private:
    ref<Device> m_device;
    nvrhi::BufferHandle m_vertexBuffer;
    nvrhi::BufferHandle m_indexBuffer;
    nvrhi::rt::AccelStructHandle m_blas;
    nvrhi::rt::AccelStructHandle m_tlas;
};
