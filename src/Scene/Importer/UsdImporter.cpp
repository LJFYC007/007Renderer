#include <tinyusdz.hh>
#include <tydra/attribute-eval.hh>
#include <tydra/render-data.hh>
#include <tydra/shader-network.hh>
#include <tydra/scene-access.hh>
#include <tydra/attribute-eval.hh>
#include <pprinter.hh>
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
    tinyusdz::tydra::RenderScene renderScene;
    tinyusdz::tydra::RenderSceneConverter converter;
    tinyusdz::tydra::RenderSceneConverterEnv env(mStage);

    // Convert USD Stage to RenderScene to access processed materials
    if (!converter.ConvertToRenderScene(env, &renderScene))
    {
        std::string warn = converter.GetWarning();
        std::string err = converter.GetError();
        if (!warn.empty())
            LOG_WARN("USD material conversion warning: {}", warn);
        if (!err.empty())
            LOG_ERROR("USD material conversion error: {}", err);
        return nullptr;
    }

    // Convert RenderMaterials to renderer materials
    for (const auto& renderMaterial : renderScene.materials)
    {
        Material material = extractMaterial(renderMaterial);
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
    std::vector<tinyusdz::value::point3f> points = geomMesh->get_points();
    std::vector<int32_t> faceVertexIndices = geomMesh->get_faceVertexIndices();
    std::vector<int32_t> faceVertexCounts = geomMesh->get_faceVertexCounts();
    std::vector<tinyusdz::value::normal3f> normals = geomMesh->get_normals();
    bool hasNormals = !normals.empty();

    // Get UV coordinates from primvars (try common UV primvar names)
    std::vector<tinyusdz::value::texcoord2f> uvCoords;
    bool hasUVs = false;
    std::vector<std::string> uvNames = {"st", "uv", "st0", "uv0"};

    for (const std::string& uvName : uvNames)
        if (geomMesh->has_primvar(uvName))
        {
            tinyusdz::GeomPrimvar uvPrimvar;
            std::string uvErr;
            if (geomMesh->get_primvar(uvName, &uvPrimvar, &uvErr) && uvPrimvar.get_value(&uvCoords))
            {
                hasUVs = true;
                break;
            }
        }
    if (!hasUVs)
        LOG_WARN("No UV coordinates found in mesh primvars, defaulting to zero UVs");

    // Store the current vertex and index offsets
    uint32_t vertexOffset = static_cast<uint32_t>(scene->vertices.size());
    uint32_t indexOffset = static_cast<uint32_t>(scene->indices.size());

    // Convert points to vertices and apply transform
    for (size_t i = 0; i < points.size(); ++i)
    {
        Vertex vertex;

        // Apply world transform to position
        tinyusdz::value::point3f transformedPos = tinyusdz::transform(worldMatrix, points[i]);
        vertex.position[0] = transformedPos[0];
        vertex.position[1] = transformedPos[1];
        vertex.position[2] = transformedPos[2];

        // Set texture coordinates (UV)
        if (hasUVs && i < uvCoords.size())
        {
            vertex.texCoord[0] = uvCoords[i][0];
            vertex.texCoord[1] = uvCoords[i][1];
        }
        else
        {
            vertex.texCoord[0] = 0.0f;
            vertex.texCoord[1] = 0.0f;
        }

        // Set normals if available and apply transform
        if (hasNormals && i < normals.size())
        {
            // Transform normal using the 3x3 part of the matrix (direction transform)
            tinyusdz::value::normal3f transformedNormal = tinyusdz::transform_dir(worldMatrix, normals[i]);
            vertex.normal[0] = transformedNormal[0];
            vertex.normal[1] = transformedNormal[1];
            vertex.normal[2] = transformedNormal[2];
        }
        else
        {
            // Default normal pointing up
            vertex.normal[0] = 0.0f;
            vertex.normal[1] = 1.0f;
            vertex.normal[2] = 0.0f;
        }

        scene->vertices.push_back(vertex);
    }

    // Convert face data to triangles (rest of the triangulation logic remains the same)
    size_t faceIndexOffset = 0;
    uint32_t triangleCount = 0;

    // Process each face based on face vertex counts
    for (size_t faceIdx = 0; faceIdx < faceVertexCounts.size(); ++faceIdx)
    {
        int32_t vertexCount = faceVertexCounts[faceIdx];

        if (vertexCount == 3)
        {
            // Triangle - add directly
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset]));
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset + 1]));
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset + 2]));
            triangleCount++;
        }
        else if (vertexCount == 4)
        {
            // Quad - split into two triangles
            // Triangle 1: 0, 1, 2
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset]));
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset + 1]));
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset + 2]));

            // Triangle 2: 0, 2, 3
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset]));
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset + 2]));
            scene->indices.push_back(vertexOffset + static_cast<uint32_t>(faceVertexIndices[faceIndexOffset + 3]));
            triangleCount += 2;
        }
        else
            LOG_WARN("Polygon with more than 4 vertices found, skipping triangulation");
        faceIndexOffset += vertexCount;
    }

    // Update triangle to mesh mapping
    for (uint32_t i = 0; i < triangleCount; ++i)
        scene->triangleToMesh.push_back(static_cast<uint32_t>(scene->meshes.size()));
}

Material UsdImporter::extractMaterial(const tinyusdz::tydra::RenderMaterial& usdMaterial)
{
    Material material;
    const auto& surfaceShader = usdMaterial.surfaceShader;

    if (!surfaceShader.diffuseColor.is_texture())
    {
        const auto& diffuse = surfaceShader.diffuseColor.value;
        material.baseColorFactor = glm::vec3(diffuse[0], diffuse[1], diffuse[2]);
    }
    if (!surfaceShader.metallic.is_texture())
        material.metallicFactor = surfaceShader.metallic.value;
    if (!surfaceShader.roughness.is_texture())
        material.roughnessFactor = surfaceShader.roughness.value;
    if (!surfaceShader.emissiveColor.is_texture())
    {
        const auto& emissive = surfaceShader.emissiveColor.value;
        material.emissiveFactor = glm::vec3(emissive[0], emissive[1], emissive[2]);
    }
    return material;
}
