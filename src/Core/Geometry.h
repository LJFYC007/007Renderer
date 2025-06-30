#pragma once

#include <nvrhi/nvrhi.h>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <vector>
#include "GeometryData.h"

class Device;

class Geometry
{
public:
    Geometry(Device& device);
    Geometry(Device& device, const GeometryData& geometryData);
    ~Geometry() = default;

    // Non-copyable
    Geometry(const Geometry&) = delete;
    Geometry& operator=(const Geometry&) = delete;

    // Movable
    Geometry(Geometry&&) = default;
    Geometry& operator=(Geometry&&) = default;

    void cleanup();

    nvrhi::rt::AccelStructHandle getBLAS() const { return m_blas; }
    nvrhi::rt::AccelStructHandle getTLAS() const { return m_tlas; }

    // Get geometry buffers for shader access
    nvrhi::BufferHandle getVertexBuffer() const { return m_vertexBuffer; }
    nvrhi::BufferHandle getIndexBuffer() const { return m_indexBuffer; }

    // Get geometry info
    size_t getVertexCount() const { return m_vertexCount; }
    size_t getIndexCount() const { return m_indexCount; }
    const std::string& getName() const { return m_name; }

private:
    nvrhi::BufferHandle m_vertexBuffer;
    nvrhi::BufferHandle m_indexBuffer;
    nvrhi::rt::AccelStructHandle m_blas;
    nvrhi::rt::AccelStructHandle m_tlas;

    // Cached geometry info
    size_t m_vertexCount = 0;
    size_t m_indexCount = 0;
    std::string m_name;

    bool createBuffers(Device& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    bool createAccelerationStructures(Device& device);
    bool buildAccelerationStructures(Device& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
};
