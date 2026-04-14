#include "Scene.h"
#include "Utils/Logger.h"

Scene::Scene(ref<Device> pDevice) : mpDevice(pDevice)
{
    mTextureManager = make_ref<TextureManager>(pDevice);
    mTextureManager->initialize();
}

uint32_t Scene::loadTexture(const float* data, uint32_t width, uint32_t height, uint32_t channels, const std::string& debugName)
{
    return mTextureManager->loadTexture(data, width, height, channels, debugName);
}

nvrhi::TextureHandle Scene::getTexture(uint32_t textureId) const
{
    return mTextureManager->getTexture(textureId);
}

const std::vector<nvrhi::TextureHandle>& Scene::getTextures() const
{
    return mTextureManager->getAllTextures();
}

size_t Scene::getTextureCount() const
{
    return mTextureManager->getTextureCount();
}

nvrhi::TextureHandle Scene::getDefaultTexture() const
{
    return mTextureManager->getDefaultTexture();
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

    // --- Collect emissive triangles and build area-weighted CDF ---
    emissiveTriangles.clear();
    totalEmissiveArea = 0.f;
    uint32_t numTriangles = static_cast<uint32_t>(triangleToMesh.size());
    for (uint32_t triIdx = 0; triIdx < numTriangles; triIdx++)
    {
        uint32_t meshIdx = triangleToMesh[triIdx];
        uint32_t matIdx = meshes[meshIdx].materialIndex;
        const Material& mat = materials[matIdx];

        bool isEmissive = (mat.emissiveFactor.r > 0.f || mat.emissiveFactor.g > 0.f || mat.emissiveFactor.b > 0.f);
        if (!isEmissive)
            continue;

        uint32_t i0 = indices[triIdx * 3 + 0];
        uint32_t i1 = indices[triIdx * 3 + 1];
        uint32_t i2 = indices[triIdx * 3 + 2];
        float3 p0(vertices[i0].position[0], vertices[i0].position[1], vertices[i0].position[2]);
        float3 p1(vertices[i1].position[0], vertices[i1].position[1], vertices[i1].position[2]);
        float3 p2(vertices[i2].position[0], vertices[i2].position[1], vertices[i2].position[2]);
        float area = 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));

        if (area > 0.f)
        {
            EmissiveTriangle et;
            et.triangleIndex = triIdx;
            et.area = area;
            et.cdfUpper = 0.f;
            et._padding = 0.f;
            emissiveTriangles.push_back(et);
            totalEmissiveArea += area;
        }
    }

    // Build CDF (cumulative area / totalArea)
    if (!emissiveTriangles.empty() && totalEmissiveArea > 0.f)
    {
        float runningSum = 0.f;
        for (auto& et : emissiveTriangles)
        {
            runningSum += et.area;
            et.cdfUpper = runningSum / totalEmissiveArea;
        }
        // Ensure last entry is exactly 1.0 (avoid floating-point drift)
        emissiveTriangles.back().cdfUpper = 1.f;
    }

    // Always create the buffer (shader reflection expects the binding even if empty).
    // Use a single dummy entry when there are no emissive triangles.
    EmissiveTriangle dummy = {0, 0.f, 0.f, 0.f};
    std::vector<EmissiveTriangle> dummyVec = {dummy};
    const auto* pBufferData = emissiveTriangles.empty() ? &dummyVec : &emissiveTriangles;

    size_t emissiveBufferSize = pBufferData->size() * sizeof(EmissiveTriangle);
    nvrhi::BufferDesc emissiveBufferDesc = nvrhi::BufferDesc()
                                               .setByteSize(emissiveBufferSize)
                                               .setInitialState(nvrhi::ResourceStates::ShaderResource)
                                               .setKeepInitialState(true)
                                               .setDebugName("Scene Emissive Triangle Buffer")
                                               .setCanHaveRawViews(true)
                                               .setStructStride(sizeof(EmissiveTriangle));
    mEmissiveTriangleBuffer = nvrhiDevice->createBuffer(emissiveBufferDesc);
    if (!mEmissiveTriangleBuffer)
        LOG_ERROR_RETURN("Failed to create emissive triangle buffer");
    commandList->writeBuffer(mEmissiveTriangleBuffer, pBufferData->data(), emissiveBufferSize);

    commandList->close();
    nvrhiDevice->executeCommandList(commandList);
    LOG_INFO(
        "Successfully initialized geometry ({} vertices, {} indices, {} meshes, {} materials, {} emissive triangles)",
        vertices.size(),
        indices.size(),
        meshes.size(),
        materials.size(),
        emissiveTriangles.size()
    );
}