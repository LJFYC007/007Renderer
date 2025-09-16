#include "Scene.h"
#include "Utils/Logger.h"

void Scene::buildAccelStructs()
{
    auto nvrhiDevice = mpDevice->getDevice();
    auto commandList = mpDevice->getCommandList();
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
    mVertexBuffer = nvrhiDevice->createBuffer(vertexBufferDesc);
    if (!mVertexBuffer)
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
    mIndexBuffer = nvrhiDevice->createBuffer(indexBufferDesc);
    if (!mIndexBuffer)
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
    mMeshBuffer = nvrhiDevice->createBuffer(meshBufferDesc);
    if (!mMeshBuffer)
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
    mTriangleToMeshBuffer = nvrhiDevice->createBuffer(triangleToMeshBufferDesc);
    if (!mTriangleToMeshBuffer)
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
    mMaterialBuffer = nvrhiDevice->createBuffer(materialBufferDesc);
    if (!mMaterialBuffer)
        LOG_ERROR_RETURN("Failed to create material buffer for scene"); // BLAS descriptor
    auto blasDesc = nvrhi::rt::AccelStructDesc().setDebugName("BLAS").setIsTopLevel(false);
    mBlas = nvrhiDevice->createAccelStruct(blasDesc);
    if (!mBlas)
        LOG_ERROR_RETURN("Failed to create BLAS");

    auto tlasDesc = nvrhi::rt::AccelStructDesc().setDebugName("TLAS").setIsTopLevel(true).setTopLevelMaxInstances(1);
    mTlas = nvrhiDevice->createAccelStruct(tlasDesc);
    if (!mTlas)
        LOG_ERROR_RETURN("Failed to create TLAS");

    // Build the acceleration structures
    commandList->open(); // Upload the vertex and index data
    commandList->writeBuffer(mVertexBuffer, vertices.data(), vertices.size() * sizeof(Vertex));
    commandList->writeBuffer(mIndexBuffer, indices.data(), indices.size() * sizeof(uint32_t));
    commandList->writeBuffer(mMeshBuffer, meshes.data(), meshes.size() * sizeof(Mesh));
    commandList->writeBuffer(mTriangleToMeshBuffer, triangleToMesh.data(), triangleToMesh.size() * sizeof(uint32_t));
    commandList->writeBuffer(mMaterialBuffer, materials.data(), materials.size() * sizeof(Material)); // Build the BLAS
    auto triangles = nvrhi::rt::GeometryTriangles()
                         .setVertexBuffer(mVertexBuffer)
                         .setVertexFormat(nvrhi::Format::RGB32_FLOAT)
                         .setVertexCount(static_cast<uint32_t>(vertices.size()))
                         .setVertexStride(sizeof(Vertex))
                         .setIndexBuffer(mIndexBuffer)
                         .setIndexFormat(nvrhi::Format::R32_UINT)
                         .setIndexCount(static_cast<uint32_t>(indices.size()));

    auto geometryDesc = nvrhi::rt::GeometryDesc().setTriangles(triangles).setFlags(nvrhi::rt::GeometryFlags::Opaque);
    commandList->buildBottomLevelAccelStruct(mBlas, &geometryDesc, 1);

    auto instanceDesc = nvrhi::rt::InstanceDesc()
                            .setBLAS(mBlas)
                            .setFlags(nvrhi::rt::InstanceFlags::TriangleCullDisable)
                            .setTransform(nvrhi::rt::c_IdentityTransform)
                            .setInstanceMask(0xFF);

    commandList->buildTopLevelAccelStruct(mTlas, &instanceDesc, 1);

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