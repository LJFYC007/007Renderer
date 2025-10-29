#include <tinyusdz.hh>
#include <tydra/attribute-eval.hh>
#include <tydra/render-data.hh>
#include <tydra/shader-network.hh>
#include <tydra/scene-access.hh>
#include <tydra/attribute-eval.hh>
#include <pprinter.hh>
#include <io-util.hh>
#include <usdShade.hh>
#include <value-pprint.hh>

#include <array>
#include <cmath>
#include <optional>
#include <map>

#include "UsdImporter.h"
#include "Utils/Logger.h"

// key = Full absolute prim path(e.g. `/bora/dora`)
using XformMap = std::map<std::string, const tinyusdz::Xform*>;
using MeshMap = std::map<std::string, const tinyusdz::GeomMesh*>;
using MaterialMap = std::map<std::string, const tinyusdz::Material*>;
using PreviewSurfaceMap = std::map<std::string, std::pair<const tinyusdz::Shader*, const tinyusdz::UsdPreviewSurface*>>;
using UVTextureMap = std::map<std::string, std::pair<const tinyusdz::Shader*, const tinyusdz::UsdUVTexture*>>;
using PrimvarReader_float2Map = std::map<std::string, std::pair<const tinyusdz::Shader*, const tinyusdz::UsdPrimvarReader_float2*>>;

ref<Scene> UsdImporter::loadScene(const std::string& fileName)
{
    std::string warn;
    std::string err;

    bool ret = tinyusdz::LoadUSDFromFile(fileName, &mStage, &warn, &err);
    if (warn.size())
        LOG_WARN("USD Importer warning: {}", warn);
    if (!ret)
    {
        LOG_ERROR("Failed to load USD file: {}. Error: {}", fileName, err);
        return nullptr;
    }
    LOG_DEBUG("Successfully loaded USD file: {}", fileName);

    // Create a new scene to hold the converted data
    ref<Scene> scene = make_ref<Scene>(mpDevice);
    scene->name = fileName;

    // Build XformNode hierarchy to handle transforms properly
    tinyusdz::tydra::XformNode rootXformNode;
    double t = tinyusdz::value::TimeCode::Default();
    tinyusdz::value::TimeSampleInterpolationType tinterp = tinyusdz::value::TimeSampleInterpolationType::Linear;

    if (!tinyusdz::tydra::BuildXformNodeFromStage(mStage, &rootXformNode, t, tinterp))
    {
        LOG_ERROR("Failed to build XformNode hierarchy from USD stage");
        return nullptr;
    }

    // Mapping hold the pointer to concrete Prim object,
    // So stage content should not be changed(no Prim addition/deletion).
    // XformMap xformmap;
    // MeshMap meshmap;
    // PreviewSurfaceMap surfacemap;
    // UVTextureMap texmap;
    // PrimvarReader_float2Map preadermap;
    // tinyusdz::tydra::ListPrims(mStage, xformmap);
    // tinyusdz::tydra::ListPrims(mStage, meshmap);

    MaterialMap matmap;
    tinyusdz::tydra::ListPrims(mStage, matmap);

    // Build RenderScene to access materials
    tinyusdz::tydra::RenderSceneConverter converter;
    tinyusdz::tydra::RenderSceneConverterEnv env(mStage);
    std::string usdBaseDir = tinyusdz::io::GetBaseDir(fileName);

    LOG_INFO("USD base directory for asset search: '{}'", usdBaseDir);
    LOG_INFO("USD file path: '{}'", fileName);

    // Set search paths for texture loading
    env.set_search_paths({usdBaseDir});
    env.scene_config.load_texture_assets = true;

    LOG_INFO("RenderSceneConverter config: load_texture_assets = {}", env.scene_config.load_texture_assets ? "true" : "false");

    // Convert USD Stage to RenderScene to access processed materials
    if (!converter.ConvertToRenderScene(env, &mRenderScene))
    {
        std::string warn = converter.GetWarning();
        std::string err = converter.GetError();
        if (!warn.empty())
            LOG_WARN("USD material conversion warning: {}", warn);
        if (!err.empty())
            LOG_ERROR("USD material conversion error: {}", err);
        return nullptr;
    }

    // Log conversion results
    if (!converter.GetWarning().empty())
        LOG_WARN("USD RenderScene conversion warning: {}", converter.GetWarning());

    LOG_INFO(
        "RenderScene conversion completed: {} textures, {} images, {} buffers",
        mRenderScene.textures.size(),
        mRenderScene.images.size(),
        mRenderScene.buffers.size()
    );

    // Print the entire prim hierarchy for debugging
    auto prim_visit_fun = [](const tinyusdz::Path& abs_path, const tinyusdz::Prim& prim, const int32_t level, void* userdata, std::string* err
                          ) -> bool
    {
        (void)err;
        std::cout << tinyusdz::pprint::Indent(level) << "[" << level << "] (" << prim.data().type_name() << ") " << prim.local_path().prim_part()
                  << " : AbsPath " << tinyusdz::to_string(abs_path) << "\n";

        // Use as() or is() for Prim specific processing.
        if (const tinyusdz::Material* pm = prim.as<tinyusdz::Material>())
        {
            (void)pm;
            std::cout << tinyusdz::pprint::Indent(level) << "  Got Material!\n";
            // return false + `err` empty if you want to terminate traversal earlier.
            // return false;
        }

        return true;
    };
    void* userdata = nullptr;
    // tinyusdz::tydra::VisitPrims(mStage, prim_visit_fun, userdata);

    // Convert RenderMaterials to renderer materials
    for (const auto& renderMaterial : mRenderScene.materials)
    {
        Material material = extractMaterial(renderMaterial, scene);
        scene->materials.push_back(material);
        uint32_t materialIndex = static_cast<uint32_t>(scene->materials.size() - 1);
        mMaterialPathToIndex[renderMaterial.abs_path] = materialIndex;
    }

    // Traverse the XformNode hierarchy for meshes
    for (const auto& child : rootXformNode.children)
        traverseXformNode(child, scene);

    LOG_INFO(
        "Scene conversion completed. Found {} vertices, {} indices, {} meshes, {} materials",
        scene->vertices.size(),
        scene->indices.size(),
        scene->meshes.size(),
        scene->materials.size()
    );
    return scene;
}

void UsdImporter::traverseXformNode(const tinyusdz::tydra::XformNode& node, ref<Scene> scene)
{
    // Check if this node has a valid prim
    if (node.prim)
    {
        const tinyusdz::value::matrix4d& worldMatrix = node.get_world_matrix();
        if (const tinyusdz::GeomMesh* geomMesh = node.prim->as<tinyusdz::GeomMesh>())
        {
            extractMeshGeometry(geomMesh, worldMatrix, scene);

            tinyusdz::Path matPath;
            const tinyusdz::Material* material{nullptr};
            std::string err;
            bool ret = tinyusdz::tydra::GetBoundMaterial(
                mStage, tinyusdz::Path(tinyusdz::to_string(node.absolute_path), ""), /* purpose */ "", &matPath, &material, &err
            );

            // Create mesh entry
            Mesh mesh;
            std::string meshName = geomMesh->get_name();
            mesh.materialIndex = mMaterialPathToIndex[tinyusdz::to_string(matPath)];
            scene->meshes.push_back(mesh);
        }
        else if (const tinyusdz::GeomCamera* geomCamera = node.prim->as<tinyusdz::GeomCamera>())
            extractCamera(geomCamera, worldMatrix, scene);
    }

    // Recursively traverse children
    for (const auto& child : node.children)
        traverseXformNode(child, scene);
}

void UsdImporter::extractCamera(const tinyusdz::GeomCamera* geomCamera, const tinyusdz::value::matrix4d& worldMatrix, ref<Scene> scene)
{
    // Evaluate camera position in world space using tinyusdz helpers
    tinyusdz::value::point3f origin{0.0f, 0.0f, 0.0f};
    tinyusdz::value::point3f worldOrigin = tinyusdz::transform(worldMatrix, origin);
    float3 cameraPos(worldOrigin[0], worldOrigin[1], worldOrigin[2]);

    // Extract forward direction from camera transform
    // USD cameras look down -Z by default in local space
    tinyusdz::value::float3 localForwardUsd{0.f, 0.f, -1.f};
    tinyusdz::value::float3 worldForwardUsd = tinyusdz::transform_dir(worldMatrix, localForwardUsd);
    float3 worldForward(worldForwardUsd[0], worldForwardUsd[1], worldForwardUsd[2]);

    // Calculate target position (camera position + forward direction)
    float3 cameraTarget = cameraPos + normalize(worldForward);
    scene->camera = make_ref<Camera>(cameraPos, cameraTarget, glm::radians(45.f));
    LOG_INFO(
        "Extracted camera : pos({}, {}, {}), target({}, {}, {})",
        cameraPos.x,
        cameraPos.y,
        cameraPos.z,
        cameraTarget.x,
        cameraTarget.y,
        cameraTarget.z
    );
}

void UsdImporter::extractMeshGeometry(const tinyusdz::GeomMesh* geomMesh, const tinyusdz::value::matrix4d& worldMatrix, ref<Scene> scene)
{
    auto points = geomMesh->get_points();
    auto faceVertexIndices = geomMesh->get_faceVertexIndices();
    auto faceVertexCounts = geomMesh->get_faceVertexCounts();
    auto normals = geomMesh->get_normals();
    bool hasNormals = !normals.empty();

    // Get UV coordinates from primvars
    std::vector<tinyusdz::value::texcoord2f> uvCoords;
    bool hasUVs = false;
    for (const auto& uvName : {"st", "uv", "st0", "uv0"})
    {
        if (geomMesh->has_primvar(uvName))
        {
            tinyusdz::GeomPrimvar uvPrimvar;
            std::string uvErr;
            if (geomMesh->get_primvar(uvName, &uvPrimvar, &uvErr) && uvPrimvar.get_value(&uvCoords))
            {
                hasUVs = true;
                if (uvPrimvar.get_interpolation() != tinyusdz::Interpolation::FaceVarying)
                    LOG_WARN("UV coordinates are not faceVarying, skipping");
                break;
            }
        }
    }
    if (!hasUVs)
        LOG_WARN("No UV coordinates found, defaulting to zero UVs");

    // Helper to create vertex from indices
    auto createVertex = [&](int32_t pointIdx, int32_t uvIdx) -> Vertex
    {
        Vertex v;
        auto pos = tinyusdz::transform(worldMatrix, points[pointIdx]);
        v.position[0] = pos[0];
        v.position[1] = pos[1];
        v.position[2] = pos[2];

        if (uvIdx < static_cast<int32_t>(uvCoords.size()))
        {
            v.texCoord[0] = uvCoords[uvIdx][0];
            v.texCoord[1] = uvCoords[uvIdx][1];
        }
        else
        {
            v.texCoord[0] = 0.0f;
            v.texCoord[1] = 0.0f;
        }

        if (hasNormals && pointIdx < static_cast<int32_t>(normals.size()))
        {
            auto n = tinyusdz::transform_dir(worldMatrix, normals[pointIdx]);
            v.normal[0] = n[0];
            v.normal[1] = n[1];
            v.normal[2] = n[2];
        }
        else
        {
            v.normal[0] = 0.0f;
            v.normal[1] = 1.0f;
            v.normal[2] = 0.0f;
        }

        return v;
    };

    // Helper to add vertex and return its index
    auto addVertex = [&](int32_t faceOffset, int32_t localIdx) -> uint32_t
    {
        int32_t pointIdx = faceVertexIndices[faceOffset + localIdx];
        int32_t uvIdx = static_cast<int32_t>(faceOffset + localIdx);
        scene->vertices.push_back(createVertex(pointIdx, uvIdx));
        uint32_t idx = static_cast<uint32_t>(scene->vertices.size()) - 1;
        scene->indices.push_back(idx);
        return idx;
    };

    size_t faceOffset = 0;
    uint32_t triangleCount = 0;

    for (int32_t faceVertCount : faceVertexCounts)
    {
        if (faceVertCount == 3)
        {
            // Triangle
            for (int32_t i = 0; i < 3; ++i)
                addVertex(faceOffset, i);
            triangleCount++;
        }
        else if (faceVertCount == 4)
        {
            // Quad -> split into two triangles (0,1,2) and (0,2,3)
            addVertex(faceOffset, 0);
            addVertex(faceOffset, 1);
            addVertex(faceOffset, 2);

            addVertex(faceOffset, 0);
            addVertex(faceOffset, 2);
            addVertex(faceOffset, 3);
            triangleCount += 2;
        }
        else
            LOG_WARN("Polygon with {} vertices found, skipping", faceVertCount);

        faceOffset += faceVertCount;
    }

    for (uint32_t i = 0; i < triangleCount; ++i)
        scene->triangleToMesh.push_back(static_cast<uint32_t>(scene->meshes.size()));
}

Material UsdImporter::extractMaterial(const tinyusdz::tydra::RenderMaterial& usdMaterial, ref<Scene> scene)
{
    Material material;
    const auto& surfaceShader = usdMaterial.surfaceShader;

    // Base Color (diffuse)
    if (!surfaceShader.diffuseColor.is_texture())
    {
        const auto& diffuse = surfaceShader.diffuseColor.value;
        material.baseColorFactor = glm::vec3(diffuse[0], diffuse[1], diffuse[2]);
    }
    else
    {
        int32_t texId = surfaceShader.diffuseColor.texture_id;
        material.baseColorTextureId = loadTextureFromRenderScene(texId, scene);
        LOG_INFO("{}: Base color texture loaded (engine ID: {})", usdMaterial.abs_path, material.baseColorTextureId);
    }

    // Metallic
    if (!surfaceShader.metallic.is_texture())
        material.metallicFactor = surfaceShader.metallic.value;
    else
    {
        int32_t texId = surfaceShader.metallic.texture_id;
        material.metallicRoughnessTextureId = loadTextureFromRenderScene(texId, scene);
        LOG_INFO("{}: Metallic texture loaded (engine ID: {})", usdMaterial.abs_path, material.metallicRoughnessTextureId);
    }

    // Roughness
    if (!surfaceShader.roughness.is_texture())
        material.roughnessFactor = surfaceShader.roughness.value;
    else if (material.metallicRoughnessTextureId == kInvalidTextureId)
    {
        int32_t texId = surfaceShader.roughness.texture_id;
        material.metallicRoughnessTextureId = loadTextureFromRenderScene(texId, scene);
        LOG_INFO("{}: Roughness texture loaded (engine ID: {})", usdMaterial.abs_path, material.metallicRoughnessTextureId);
    }

    // Emissive
    if (!surfaceShader.emissiveColor.is_texture())
    {
        const auto& emissive = surfaceShader.emissiveColor.value;
        material.emissiveFactor = glm::vec3(emissive[0], emissive[1], emissive[2]);
    }
    else
    {
        int32_t texId = surfaceShader.emissiveColor.texture_id;
        material.emissiveTextureId = loadTextureFromRenderScene(texId, scene);
        LOG_INFO("{}: Emissive texture loaded (engine ID: {})", usdMaterial.abs_path, material.emissiveTextureId);
    }

    return material;
}

uint32_t UsdImporter::loadTextureFromRenderScene(int32_t textureId, ref<Scene> scene)
{
    // Check if already loaded
    auto it = mUsdTextureIdToEngineId.find(textureId);
    if (it != mUsdTextureIdToEngineId.end())
        return it->second;

    const auto& uvTexture = mRenderScene.textures[textureId];
    const auto& texImage = mRenderScene.images[uvTexture.texture_image_id];
    const auto& buffer = mRenderScene.buffers[texImage.buffer_id];

    if (buffer.data.empty())
    {
        LOG_ERROR("Texture buffer is empty! Texture asset may not have been loaded by tinyusdz.");
        return kInvalidTextureId;
    }

    // Verify buffer size matches expected size based on component type
    uint32_t bytesPerComponent = 4; // float32
    size_t expectedSize = static_cast<size_t>(texImage.width) * texImage.height * texImage.channels * bytesPerComponent;
    if (buffer.data.size() != expectedSize)
    {
        LOG_WARN(
            "Buffer size ({}) doesn't match expected ({} = {}x{}x{}x{})",
            buffer.data.size(),
            expectedSize,
            texImage.width,
            texImage.height,
            texImage.channels,
            bytesPerComponent
        );
    }

    const float* floatData = reinterpret_cast<const float*>(buffer.data.data());
    uint32_t engineTextureId = scene->loadTexture(floatData, texImage.width, texImage.height, texImage.channels, texImage.asset_identifier);

    // Cache the mapping
    mUsdTextureIdToEngineId[textureId] = engineTextureId;
    return engineTextureId;
}
