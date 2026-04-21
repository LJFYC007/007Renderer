#include "Scene.h"
#include "Utils/Logger.h"
#include <cstring>

Scene::Scene(ref<Device> pDevice) : mpDevice(pDevice)
{
    mTextureManager = make_ref<TextureManager>(pDevice);
    mTextureManager->initialize();
}

void Scene::addMeshInstance(uint32_t indexOffset, uint32_t indexCount, uint32_t materialIndex, const glm::mat4& localToWorld)
{
    // Zero-index BLAS builds fail; drop empty meshes (e.g. USD n-gon subsets) at the source.
    if (indexCount == 0)
        return;

    MeshDesc md;
    md.indexOffset = indexOffset;
    md.indexCount = indexCount;
    meshes.push_back(md);

    MeshInstance mi;
    mi.meshID = static_cast<uint32_t>(meshes.size() - 1);
    mi.materialIndex = materialIndex;
    mi.localToWorld = localToWorld;
    instances.push_back(mi);
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

    if (instances.empty())
    {
        LOG_WARN("Scene has no geometry to build acceleration structures");
        return;
    }

    size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    nvrhi::BufferDesc vertexBufferDesc = nvrhi::BufferDesc()
                                             .setByteSize(vertexBufferSize)
                                             .setIsVertexBuffer(true)
                                             .setInitialState(nvrhi::ResourceStates::VertexBuffer)
                                             .setKeepInitialState(true)
                                             .setDebugName("Scene Vertex Buffer")
                                             .setCanHaveRawViews(true)
                                             .setIsAccelStructBuildInput(true)
                                             .setStructStride(sizeof(Vertex));
    mVertexBuffer = nvrhiDevice->createBuffer(vertexBufferDesc);
    if (!mVertexBuffer)
        LOG_ERROR_RETURN("Failed to create vertex buffer for scene");

    nvrhi::BufferDesc indexBufferDesc = nvrhi::BufferDesc()
                                            .setByteSize(indexBufferSize)
                                            .setIsIndexBuffer(true)
                                            .setInitialState(nvrhi::ResourceStates::IndexBuffer)
                                            .setKeepInitialState(true)
                                            .setDebugName("Scene Index Buffer")
                                            .setCanHaveRawViews(true)
                                            .setIsAccelStructBuildInput(true)
                                            .setStructStride(sizeof(uint32_t));
    mIndexBuffer = nvrhiDevice->createBuffer(indexBufferDesc);
    if (!mIndexBuffer)
        LOG_ERROR_RETURN("Failed to create index buffer for scene");

    size_t meshBufferSize = meshes.size() * sizeof(MeshDesc);
    nvrhi::BufferDesc meshBufferDesc = nvrhi::BufferDesc()
                                           .setByteSize(meshBufferSize)
                                           .setInitialState(nvrhi::ResourceStates::ShaderResource)
                                           .setKeepInitialState(true)
                                           .setDebugName("Scene Mesh Buffer")
                                           .setCanHaveRawViews(true)
                                           .setStructStride(sizeof(MeshDesc));
    mMeshBuffer = nvrhiDevice->createBuffer(meshBufferDesc);
    if (!mMeshBuffer)
        LOG_ERROR_RETURN("Failed to create mesh buffer for scene");

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

    std::vector<InstanceData> instanceData(instances.size());
    for (size_t i = 0; i < instances.size(); i++)
    {
        const MeshInstance& mi = instances[i];
        glm::mat4 mt = glm::transpose(mi.localToWorld);
        InstanceData& id = instanceData[i];
        id.row0 = mt[0];
        id.row1 = mt[1];
        id.row2 = mt[2];
        id.meshID = mi.meshID;
        id.materialID = mi.materialIndex;
    }

    size_t instanceBufferSize = instanceData.size() * sizeof(InstanceData);
    nvrhi::BufferDesc instanceBufferDesc = nvrhi::BufferDesc()
                                               .setByteSize(instanceBufferSize)
                                               .setInitialState(nvrhi::ResourceStates::ShaderResource)
                                               .setKeepInitialState(true)
                                               .setDebugName("Scene Instance Buffer")
                                               .setCanHaveRawViews(true)
                                               .setStructStride(sizeof(InstanceData));
    mInstanceBuffer = nvrhiDevice->createBuffer(instanceBufferDesc);
    if (!mInstanceBuffer)
        LOG_ERROR_RETURN("Failed to create instance buffer for scene");

    // Build per-mesh BLAS descs. Each BLAS references only its own index slice
    // of the shared vertex/index buffers. PrimitiveIndex() in closest-hit is
    // therefore mesh-local (0 .. triangleCount-1 per BLAS).
    mBlases.clear();
    mBlases.reserve(meshes.size());
    std::vector<nvrhi::rt::GeometryDesc> blasGeomDescs(meshes.size());
    for (size_t i = 0; i < meshes.size(); i++)
    {
        const MeshDesc& md = meshes[i];
        auto triangles = nvrhi::rt::GeometryTriangles()
                             .setVertexBuffer(mVertexBuffer)
                             .setVertexFormat(nvrhi::Format::RGB32_FLOAT)
                             .setVertexCount(static_cast<uint32_t>(vertices.size()))
                             .setVertexStride(sizeof(Vertex))
                             .setIndexBuffer(mIndexBuffer)
                             .setIndexFormat(nvrhi::Format::R32_UINT)
                             .setIndexCount(md.indexCount)
                             .setIndexOffset(static_cast<uint64_t>(md.indexOffset) * sizeof(uint32_t));

        blasGeomDescs[i] = nvrhi::rt::GeometryDesc().setTriangles(triangles).setFlags(nvrhi::rt::GeometryFlags::Opaque);

        auto blasDesc = nvrhi::rt::AccelStructDesc().setDebugName("BLAS_" + std::to_string(i)).setIsTopLevel(false);
        blasDesc.addBottomLevelGeometry(blasGeomDescs[i]);
        auto blas = nvrhiDevice->createAccelStruct(blasDesc);
        if (!blas)
            LOG_ERROR_RETURN("Failed to create BLAS for mesh {}", i);
        mBlases.push_back(blas);
    }

    auto tlasDesc =
        nvrhi::rt::AccelStructDesc().setDebugName("TLAS").setIsTopLevel(true).setTopLevelMaxInstances(static_cast<uint32_t>(instances.size()));
    mTlas = nvrhiDevice->createAccelStruct(tlasDesc);
    if (!mTlas)
        LOG_ERROR_RETURN("Failed to create TLAS");

    commandList->open();
    commandList->writeBuffer(mVertexBuffer, vertices.data(), vertexBufferSize);
    commandList->writeBuffer(mIndexBuffer, indices.data(), indexBufferSize);
    commandList->writeBuffer(mMeshBuffer, meshes.data(), meshBufferSize);
    commandList->writeBuffer(mMaterialBuffer, materials.data(), materialBufferSize);
    commandList->writeBuffer(mInstanceBuffer, instanceData.data(), instanceBufferSize);

    for (size_t i = 0; i < meshes.size(); i++)
        commandList->buildBottomLevelAccelStruct(mBlases[i], &blasGeomDescs[i], 1);

    // InstanceData::row{0,1,2} packs 12 contiguous floats in row-major order — identical
    // to nvrhi::rt::AffineTransform's layout (float[12]), so we copy the 48B straight across.
    static_assert(sizeof(nvrhi::rt::AffineTransform) == 3 * sizeof(glm::vec4), "InstanceData rows must match AffineTransform layout");
    std::vector<nvrhi::rt::InstanceDesc> instanceDescs(instances.size());
    for (size_t i = 0; i < instances.size(); i++)
    {
        const InstanceData& id = instanceData[i];
        nvrhi::rt::AffineTransform transform;
        std::memcpy(&transform, &id.row0, sizeof(transform));
        instanceDescs[i] = nvrhi::rt::InstanceDesc()
                               .setBLAS(mBlases[id.meshID])
                               .setFlags(nvrhi::rt::InstanceFlags::TriangleCullDisable)
                               .setTransform(transform)
                               .setInstanceID(static_cast<uint32_t>(i))
                               .setInstanceMask(0xFF);
    }
    commandList->buildTopLevelAccelStruct(mTlas, instanceDescs.data(), instanceDescs.size());

    // --- Collect emissive triangles (world-space area) and build area-weighted CDF ---
    emissiveTriangles.clear();
    totalEmissiveArea = 0.f;
    for (uint32_t instIdx = 0; instIdx < static_cast<uint32_t>(instances.size()); instIdx++)
    {
        const MeshInstance& inst = instances[instIdx];
        const Material& mat = materials[instanceData[instIdx].materialID];
        bool isEmissive = (mat.emissiveFactor.r > 0.f || mat.emissiveFactor.g > 0.f || mat.emissiveFactor.b > 0.f);
        if (!isEmissive)
            continue;

        const MeshDesc& md = meshes[inst.meshID];
        uint32_t triangleCount = md.indexCount / 3;
        for (uint32_t localTri = 0; localTri < triangleCount; localTri++)
        {
            uint32_t i0 = indices[md.indexOffset + localTri * 3 + 0];
            uint32_t i1 = indices[md.indexOffset + localTri * 3 + 1];
            uint32_t i2 = indices[md.indexOffset + localTri * 3 + 2];
            glm::vec3 p0(vertices[i0].position[0], vertices[i0].position[1], vertices[i0].position[2]);
            glm::vec3 p1(vertices[i1].position[0], vertices[i1].position[1], vertices[i1].position[2]);
            glm::vec3 p2(vertices[i2].position[0], vertices[i2].position[1], vertices[i2].position[2]);
            glm::vec3 p0w = glm::vec3(inst.localToWorld * glm::vec4(p0, 1.f));
            glm::vec3 p1w = glm::vec3(inst.localToWorld * glm::vec4(p1, 1.f));
            glm::vec3 p2w = glm::vec3(inst.localToWorld * glm::vec4(p2, 1.f));
            float area = 0.5f * glm::length(glm::cross(p1w - p0w, p2w - p0w));
            if (area > 0.f)
            {
                EmissiveTriangle et;
                et.instanceID = instIdx;
                et.localTriangleIndex = localTri;
                et.area = area;
                et.cdfUpper = 0.f;
                emissiveTriangles.push_back(et);
                totalEmissiveArea += area;
            }
        }
    }

    if (!emissiveTriangles.empty() && totalEmissiveArea > 0.f)
    {
        float runningSum = 0.f;
        for (auto& et : emissiveTriangles)
        {
            runningSum += et.area;
            et.cdfUpper = runningSum / totalEmissiveArea;
        }
        emissiveTriangles.back().cdfUpper = 1.f;
    }

    // Always create the buffer (shader reflection expects the binding even if empty).
    std::vector<EmissiveTriangle> dummyVec = {EmissiveTriangle{}};
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
        "Scene AS built: {} verts, {} indices, {} meshes ({} BLAS), {} instances, {} materials, {} emissive triangles",
        vertices.size(),
        indices.size(),
        meshes.size(),
        mBlases.size(),
        instances.size(),
        materials.size(),
        emissiveTriangles.size()
    );
}
