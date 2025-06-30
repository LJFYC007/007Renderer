#include "Geometry.h"
#include "Device.h"
#include "OBJLoader.h"
#include "../Utils/Logger.h"

namespace
{
// Define triangle vertices (world space coordinates) - for backward compatibility
static const Vertex triangleVertices[] = {
    //  position          texCoord         normal
    {{0.f, 0.5f, -0.5f}, {0.5f, 0.f}, {0.f, 0.f, 1.f}},
    {{-0.5f, -0.5f, -0.5f}, {0.f, 1.f}, {0.f, 0.f, 1.f}},
    {{0.5f, -0.5f, -0.5f}, {1.f, 1.f}, {0.f, 0.f, 1.f}}
};

// Define triangle indices
static const uint32_t triangleIndices[] = {0, 1, 2};
} // namespace

Geometry::Geometry(Device& device, const GeometryData& geometryData)
{
    if (!geometryData.isValid())
        LOG_ERROR_RETURN("Invalid geometry data");

    // For now, we'll merge all sub-meshes into a single mesh for simplicity
    // In the future, this could be extended to support multiple sub-meshes
    auto flatIndices = geometryData.getFlattenedIndices();

    m_name = geometryData.name;
    m_vertexCount = geometryData.vertices.size();
    m_indexCount = flatIndices.size();

    if (!createBuffers(device, geometryData.vertices, flatIndices))
        LOG_ERROR_RETURN("Failed to create geometry buffers for '{}'", m_name);
    if (!createAccelerationStructures(device))
        LOG_ERROR_RETURN("Failed to create acceleration structures for '{}'", m_name);
    if (!buildAccelerationStructures(device, geometryData.vertices, flatIndices))
        LOG_ERROR_RETURN("Failed to build acceleration structures for '{}'", m_name);
    LOG_INFO("Successfully initialized geometry '{}' ({} vertices, {} indices)", m_name, m_vertexCount, m_indexCount);
}

Geometry::Geometry(Device& device)
{
    std::vector<Vertex> vertices(std::begin(triangleVertices), std::end(triangleVertices));
    std::vector<uint32_t> indices(std::begin(triangleIndices), std::end(triangleIndices));

    m_name = "Triangle";
    m_vertexCount = vertices.size();
    m_indexCount = indices.size();

    if (!createBuffers(device, vertices, indices))
        LOG_ERROR_RETURN("Failed to create geometry buffers");
    if (!createAccelerationStructures(device))
        LOG_ERROR_RETURN("Failed to create acceleration structures");
    if (!buildAccelerationStructures(device, vertices, indices))
        LOG_ERROR_RETURN("Failed to build acceleration structures");
}

void Geometry::cleanup()
{
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    m_blas = nullptr;
    m_tlas = nullptr;
    m_vertexCount = 0;
    m_indexCount = 0;
    m_name.clear();
}

bool Geometry::createBuffers(Device& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    // Create vertex buffer for acceleration structure build input
    nvrhi::BufferDesc vertexBufferDesc = nvrhi::BufferDesc()
                                             .setByteSize(vertexBufferSize)
                                             .setIsVertexBuffer(true)
                                             .setInitialState(nvrhi::ResourceStates::VertexBuffer)
                                             .setKeepInitialState(true) // enable fully automatic state tracking
                                             .setDebugName("Vertex Buffer: " + m_name)
                                             .setCanHaveRawViews(true)
                                             .setIsAccelStructBuildInput(true);
    m_vertexBuffer = device.getDevice()->createBuffer(vertexBufferDesc);
    if (!m_vertexBuffer)
    {
        LOG_ERROR("Failed to create vertex buffer for '{}'", m_name);
        return false;
    }

    // Create index buffer for acceleration structure build input
    nvrhi::BufferDesc indexBufferDesc = nvrhi::BufferDesc()
                                            .setByteSize(indexBufferSize)
                                            .setIsIndexBuffer(true)
                                            .setInitialState(nvrhi::ResourceStates::IndexBuffer)
                                            .setKeepInitialState(true) // enable fully automatic state tracking
                                            .setDebugName("Index Buffer: " + m_name)
                                            .setCanHaveRawViews(true)
                                            .setIsAccelStructBuildInput(true);
    m_indexBuffer = device.getDevice()->createBuffer(indexBufferDesc);
    if (!m_indexBuffer)
    {
        LOG_ERROR("Failed to create index buffer for '{}'", m_name);
        return false;
    }

    return true;
}

bool Geometry::createAccelerationStructures(Device& device)
{
    // BLAS descriptor - we'll update this in buildAccelerationStructures with actual geometry data
    auto blasDesc = nvrhi::rt::AccelStructDesc().setDebugName("BLAS: " + m_name).setIsTopLevel(false);

    m_blas = device.getDevice()->createAccelStruct(blasDesc);
    if (!m_blas)
    {
        LOG_ERROR("Failed to create BLAS for '{}'", m_name);
        return false;
    }

    auto tlasDesc = nvrhi::rt::AccelStructDesc().setDebugName("TLAS: " + m_name).setIsTopLevel(true).setTopLevelMaxInstances(1);

    m_tlas = device.getDevice()->createAccelStruct(tlasDesc);
    if (!m_tlas)
    {
        LOG_ERROR("Failed to create TLAS for '{}'", m_name);
        return false;
    }

    return true;
}

bool Geometry::buildAccelerationStructures(Device& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    auto commandList = device.getCommandList();
    commandList->open();

    // Upload the vertex and index data
    commandList->writeBuffer(m_vertexBuffer, vertices.data(), vertices.size() * sizeof(Vertex));
    commandList->writeBuffer(m_indexBuffer, indices.data(), indices.size() * sizeof(uint32_t));

    // Build the BLAS
    auto triangles = nvrhi::rt::GeometryTriangles()
                         .setVertexBuffer(m_vertexBuffer)
                         .setVertexFormat(nvrhi::Format::RGB32_FLOAT)
                         .setVertexCount(static_cast<uint32_t>(vertices.size()))
                         .setVertexStride(sizeof(Vertex))
                         .setIndexBuffer(m_indexBuffer)
                         .setIndexFormat(nvrhi::Format::R32_UINT)
                         .setIndexCount(static_cast<uint32_t>(indices.size()));

    auto geometryDesc = nvrhi::rt::GeometryDesc().setTriangles(triangles).setFlags(nvrhi::rt::GeometryFlags::Opaque);
    commandList->buildBottomLevelAccelStruct(m_blas, &geometryDesc, 1);

    auto instanceDesc = nvrhi::rt::InstanceDesc()
                            .setBLAS(m_blas)
                            .setFlags(nvrhi::rt::InstanceFlags::TriangleCullDisable)
                            .setTransform(nvrhi::rt::c_IdentityTransform)
                            .setInstanceMask(0xFF);

    commandList->buildTopLevelAccelStruct(m_tlas, &instanceDesc, 1);

    commandList->close();
    device.getDevice()->executeCommandList(commandList);

    return true;
}
