#include "Scene.h"
#include "Utils/Logger.h"

void Scene::buildAccelStructs()
{
    auto nvrhiDevice = m_device->getDevice();
    auto commandList = m_device->getCommandList();
    size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    // Create vertex buffer for acceleration structure build input
    nvrhi::BufferDesc vertexBufferDesc = nvrhi::BufferDesc()
                                             .setByteSize(vertexBufferSize)
                                             .setIsVertexBuffer(true)
                                             .setInitialState(nvrhi::ResourceStates::VertexBuffer)
                                             .setKeepInitialState(true) // enable fully automatic state tracking
                                             .setDebugName("Scene Vertex Buffer")
                                             .setCanHaveRawViews(true)
                                             .setIsAccelStructBuildInput(true)
                                             .setStructStride(sizeof(Vertex));
    m_vertexBuffer = nvrhiDevice->createBuffer(vertexBufferDesc);
    if (!m_vertexBuffer)
        LOG_ERROR_RETURN("Failed to create vertex buffer for scene");

    // Create index buffer for acceleration structure build input
    nvrhi::BufferDesc indexBufferDesc = nvrhi::BufferDesc()
                                            .setByteSize(indexBufferSize)
                                            .setIsIndexBuffer(true)
                                            .setInitialState(nvrhi::ResourceStates::IndexBuffer)
                                            .setKeepInitialState(true) // enable fully automatic state tracking
                                            .setDebugName("Scene Index Buffer")
                                            .setCanHaveRawViews(true)
                                            .setIsAccelStructBuildInput(true)
                                            .setStructStride(sizeof(uint32_t));
    m_indexBuffer = nvrhiDevice->createBuffer(indexBufferDesc);
    if (!m_indexBuffer)
        LOG_ERROR_RETURN("Failed to create index buffer for scene");

    // Create mesh buffer for shader access
    size_t meshBufferSize = meshes.size() * sizeof(Mesh);
    nvrhi::BufferDesc meshBufferDesc = nvrhi::BufferDesc()
                                           .setByteSize(meshBufferSize)
                                           .setInitialState(nvrhi::ResourceStates::ShaderResource)
                                           .setKeepInitialState(true)
                                           .setDebugName("Scene Mesh Buffer")
                                           .setCanHaveRawViews(true)
                                           .setStructStride(sizeof(Mesh));
    m_meshBuffer = nvrhiDevice->createBuffer(meshBufferDesc);
    if (!m_meshBuffer)
        LOG_ERROR_RETURN("Failed to create mesh buffer for scene");

    // Create triangleToMesh buffer for shader access
    size_t triangleToMeshBufferSize = triangleToMesh.size() * sizeof(uint32_t);
    nvrhi::BufferDesc triangleToMeshBufferDesc = nvrhi::BufferDesc()
                                                     .setByteSize(triangleToMeshBufferSize)
                                                     .setInitialState(nvrhi::ResourceStates::ShaderResource)
                                                     .setKeepInitialState(true)
                                                     .setDebugName("Scene TriangleToMesh Buffer")
                                                     .setCanHaveRawViews(true)
                                                     .setStructStride(sizeof(uint32_t));
    m_triangleToMeshBuffer = nvrhiDevice->createBuffer(triangleToMeshBufferDesc);
    if (!m_triangleToMeshBuffer)
        LOG_ERROR_RETURN("Failed to create triangleToMesh buffer for scene");

    // Create material buffer for shader access
    size_t materialBufferSize = materials.size() * sizeof(Material);
    nvrhi::BufferDesc materialBufferDesc = nvrhi::BufferDesc()
                                               .setByteSize(materialBufferSize)
                                               .setInitialState(nvrhi::ResourceStates::ShaderResource)
                                               .setKeepInitialState(true)
                                               .setDebugName("Scene Material Buffer")
                                               .setCanHaveRawViews(true)
                                               .setStructStride(sizeof(Material));
    m_materialBuffer = nvrhiDevice->createBuffer(materialBufferDesc);
    if (!m_materialBuffer)
        LOG_ERROR_RETURN("Failed to create material buffer for scene");

    // BLAS descriptor
    auto blasDesc = nvrhi::rt::AccelStructDesc().setDebugName("BLAS").setIsTopLevel(false);
    m_blas = nvrhiDevice->createAccelStruct(blasDesc);
    if (!m_blas)
        LOG_ERROR_RETURN("Failed to create BLAS");

    auto tlasDesc = nvrhi::rt::AccelStructDesc().setDebugName("TLAS").setIsTopLevel(true).setTopLevelMaxInstances(1);
    m_tlas = nvrhiDevice->createAccelStruct(tlasDesc);
    if (!m_tlas)
        LOG_ERROR_RETURN("Failed to create TLAS");

    // Build the acceleration structures
    commandList->open();

    // Upload the vertex and index data
    commandList->writeBuffer(m_vertexBuffer, vertices.data(), vertices.size() * sizeof(Vertex));
    commandList->writeBuffer(m_indexBuffer, indices.data(), indices.size() * sizeof(uint32_t));
    commandList->writeBuffer(m_meshBuffer, meshes.data(), meshes.size() * sizeof(Mesh));
    commandList->writeBuffer(m_triangleToMeshBuffer, triangleToMesh.data(), triangleToMesh.size() * sizeof(uint32_t));
    commandList->writeBuffer(m_materialBuffer, materials.data(), materials.size() * sizeof(Material));

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
    nvrhiDevice->executeCommandList(commandList);
    LOG_INFO(
        "Successfully initialized geometry ({} vertices, {} indices, {} meshes, {} materials)",
        vertices.size(),
        indices.size(),
        meshes.size(),
        materials.size()
    );
}