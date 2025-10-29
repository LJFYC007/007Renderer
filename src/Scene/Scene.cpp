#include "Scene.h"
#include "Utils/Logger.h"
#include "Utils/ResourceIO.h"

uint32_t Scene::loadTexture(const float* data, uint32_t width, uint32_t height, uint32_t channels, const std::string& debugName)
{
    nvrhi::Format format;
    uint32_t gpuChannels = channels;

    if (channels == 1)
        format = nvrhi::Format::R32_FLOAT;
    else if (channels == 2)
        format = nvrhi::Format::RG32_FLOAT;
    else if (channels == 3 || channels == 4)
    {
        format = nvrhi::Format::RGBA32_FLOAT;
        gpuChannels = 4;
    }

    auto textureDesc = nvrhi::TextureDesc()
                           .setDimension(nvrhi::TextureDimension::Texture2D)
                           .setWidth(width)
                           .setHeight(height)
                           .setMipLevels(1)
                           .setFormat(format)
                           .setInitialState(nvrhi::ResourceStates::ShaderResource)
                           .setKeepInitialState(true)
                           .setDebugName(debugName);

    auto nvrhiDevice = mpDevice->getDevice();
    nvrhi::TextureHandle texture = nvrhiDevice->createTexture(textureDesc);

    std::vector<float> uploadData;
    if (channels == 3)
    {
        uploadData.resize(width * height * 4);
        for (uint32_t i = 0; i < width * height; ++i)
        {
            uploadData[i * 4] = data[i * 3];
            uploadData[i * 4 + 1] = data[i * 3 + 1];
            uploadData[i * 4 + 2] = data[i * 3 + 2];
            uploadData[i * 4 + 3] = 1.0f;
        }
    }
    else
        uploadData.assign(data, data + width * height * channels);

    size_t dataSize = width * height * gpuChannels * sizeof(float);
    if (!ResourceIO::uploadTexture(mpDevice, texture, uploadData.data(), dataSize))
    {
        LOG_ERROR("Failed to upload texture '{}'", debugName);
        return kInvalidTextureId;
    }

    mTextures.push_back(texture);
    LOG_INFO("Loaded texture '{}' ({}x{}, {} channels)", debugName, width, height, channels);

    return static_cast<uint32_t>(mTextures.size() - 1);
}

void Scene::buildAccelStructs()
{
    auto nvrhiDevice = mpDevice->getDevice();
    auto commandList = mpDevice->getCommandList();
    size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    if (vertices.empty() || indices.empty())
    {
        LOG_WARN("Scene has no geometry to build acceleration structures");
        return;
    }

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
        LOG_ERROR_RETURN("Failed to create material buffer for scene");

    auto triangles = nvrhi::rt::GeometryTriangles()
                         .setVertexBuffer(mVertexBuffer)
                         .setVertexFormat(nvrhi::Format::RGB32_FLOAT)
                         .setVertexCount(static_cast<uint32_t>(vertices.size()))
                         .setVertexStride(sizeof(Vertex))
                         .setIndexBuffer(mIndexBuffer)
                         .setIndexFormat(nvrhi::Format::R32_UINT)
                         .setIndexCount(static_cast<uint32_t>(indices.size()));

    auto geometryDesc = nvrhi::rt::GeometryDesc().setTriangles(triangles).setFlags(nvrhi::rt::GeometryFlags::Opaque);

    auto blasDesc = nvrhi::rt::AccelStructDesc().setDebugName("BLAS").setIsTopLevel(false);
    blasDesc.addBottomLevelGeometry(geometryDesc);
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
    commandList->writeBuffer(mMaterialBuffer, materials.data(), materials.size() * sizeof(Material));

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