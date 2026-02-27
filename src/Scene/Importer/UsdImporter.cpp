#include <tinyusdz.hh>
#include <tydra/attribute-eval.hh>
#include <tydra/render-data.hh>
#include <tydra/shader-network.hh>
#include <tydra/scene-access.hh>
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

#include <DirectXTex.h>

namespace
{

bool loadDds(const std::string& path, std::vector<uint8_t>& outPixels, int32_t& outWidth, int32_t& outHeight, std::string& outErr)
{
    std::wstring wpath(path.begin(), path.end());
    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata;
    if (FAILED(DirectX::LoadFromDDSFile(wpath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image)))
    {
        outErr = "DirectXTex: Failed to load DDS: " + path;
        return false;
    }

    DirectX::ScratchImage decompressed;
    const DirectX::ScratchImage* src = &image;
    if (DirectX::IsCompressed(metadata.format))
    {
        if (FAILED(DirectX::Decompress(image.GetImages(), image.GetImageCount(), metadata, DXGI_FORMAT_R8G8B8A8_UNORM, decompressed)))
        {
            outErr = "DirectXTex: Failed to decompress DDS: " + path;
            return false;
        }
        src = &decompressed;
    }

    DirectX::ScratchImage converted;
    if (src->GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        if (FAILED(
                DirectX::Convert(
                    src->GetImages(),
                    src->GetImageCount(),
                    src->GetMetadata(),
                    DXGI_FORMAT_R8G8B8A8_UNORM,
                    DirectX::TEX_FILTER_DEFAULT,
                    DirectX::TEX_THRESHOLD_DEFAULT,
                    converted
                )
            ))
        {
            outErr = "DirectXTex: Failed to convert DDS to RGBA8: " + path;
            return false;
        }
        src = &converted;
    }

    const DirectX::Image* img = src->GetImages();
    outWidth = static_cast<int32_t>(img->width);
    outHeight = static_cast<int32_t>(img->height);
    const size_t rowBytes = img->width * 4;
    outPixels.resize(img->height * rowBytes);
    for (size_t row = 0; row < img->height; ++row)
        std::memcpy(outPixels.data() + row * rowBytes, img->pixels + row * img->rowPitch, rowBytes);
    return true;
}

bool ddsTextureLoader(
    const tinyusdz::value::AssetPath& assetPath,
    const tinyusdz::AssetInfo& assetInfo,
    const tinyusdz::AssetResolutionResolver& assetResolver,
    tinyusdz::tydra::TextureImage* imageOut,
    std::vector<uint8_t>* imageData,
    void* userdata,
    std::string* warn,
    std::string* err
)
{
    const std::string& path = assetPath.GetAssetPath();
    bool isDds = path.size() >= 4 && _stricmp(path.c_str() + path.size() - 4, ".dds") == 0;
    if (!isDds)
        return tinyusdz::tydra::DefaultTextureImageLoaderFunction(assetPath, assetInfo, assetResolver, imageOut, imageData, userdata, warn, err);

    std::string resolvedPath = assetResolver.resolve(path);
    if (resolvedPath.empty())
    {
        if (err)
            *err = "Failed to resolve DDS asset path: " + path;
        return false;
    }

    std::string ddsErr;
    if (!loadDds(resolvedPath, *imageData, imageOut->width, imageOut->height, ddsErr))
    {
        if (err)
            *err = ddsErr;
        return false;
    }

    imageOut->channels = 4;
    imageOut->assetTexelComponentType = tinyusdz::tydra::ComponentType::UInt8;
    return true;
}

} // namespace

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

    ref<Scene> scene = make_ref<Scene>(mpDevice);
    scene->name = fileName;

    tinyusdz::tydra::XformNode rootXformNode;
    double t = tinyusdz::value::TimeCode::Default();
    tinyusdz::value::TimeSampleInterpolationType tinterp = tinyusdz::value::TimeSampleInterpolationType::Linear;

    if (!tinyusdz::tydra::BuildXformNodeFromStage(mStage, &rootXformNode, t, tinterp))
    {
        LOG_ERROR("Failed to build XformNode hierarchy from USD stage");
        return nullptr;
    }

    MaterialMap matmap;
    tinyusdz::tydra::ListPrims(mStage, matmap);

    tinyusdz::tydra::RenderSceneConverter converter;
    tinyusdz::tydra::RenderSceneConverterEnv env(mStage);
    std::string usdBaseDir = tinyusdz::io::GetBaseDir(fileName);

    LOG_INFO("USD base directory for asset search: '{}'", usdBaseDir);
    LOG_INFO("USD file path: '{}'", fileName);

    // Set search paths for texture loading
    env.set_search_paths({usdBaseDir});
    env.scene_config.load_texture_assets = true;
    env.material_config.linearize_color_space = true;
    env.material_config.texture_image_loader_function = ddsTextureLoader;

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

    if (!converter.GetWarning().empty())
        LOG_WARN("USD RenderScene conversion warning: {}", converter.GetWarning());

    LOG_INFO(
        "RenderScene conversion completed: {} textures, {} images, {} buffers",
        mRenderScene.textures.size(),
        mRenderScene.images.size(),
        mRenderScene.buffers.size()
    );

    for (const auto& renderMaterial : mRenderScene.materials)
    {
        Material material = extractMaterial(renderMaterial, scene);
        scene->materials.push_back(material);
        uint32_t materialIndex = static_cast<uint32_t>(scene->materials.size() - 1);
        mMaterialPathToIndex[renderMaterial.abs_path] = materialIndex;
    }

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
    if (node.prim)
    {
        const tinyusdz::value::matrix4d& worldMatrix = node.get_world_matrix();
        if (const tinyusdz::GeomMesh* geomMesh = node.prim->as<tinyusdz::GeomMesh>())
        {
            auto subsets = tinyusdz::tydra::GetGeomSubsets(mStage, node.absolute_path, tinyusdz::value::token("materialBind"));

            if (subsets.empty())
            {
                // No subsets: whole mesh uses a single top-level material
                extractMeshGeometry(geomMesh, worldMatrix, scene);

                tinyusdz::Path matPath;
                const tinyusdz::Material* material{nullptr};
                std::string err;
                tinyusdz::tydra::GetBoundMaterial(mStage, node.absolute_path, "", &matPath, &material, &err);
                if (!err.empty())
                    LOG_WARN("No bound material for mesh {}: {}", tinyusdz::to_string(node.absolute_path), err);
                Mesh mesh;
                auto matIt = mMaterialPathToIndex.find(tinyusdz::to_string(matPath));
                if (matIt != mMaterialPathToIndex.end())
                    mesh.materialIndex = matIt->second;
                scene->meshes.push_back(mesh);
            }
            else
            {
                // Each GeomSubset is its own draw call with its own material
                for (const auto* subset : subsets)
                {
                    std::vector<int32_t> rawFaces;
                    if (auto animatable = subset->indices.get_value())
                        animatable->get_scalar(&rawFaces);
                    if (rawFaces.empty())
                        continue;

                    std::unordered_set<int32_t> faceSet(rawFaces.begin(), rawFaces.end());
                    extractMeshGeometry(geomMesh, worldMatrix, scene, &faceSet);

                    std::string subsetPathStr = tinyusdz::to_string(node.absolute_path) + "/" + subset->name;
                    tinyusdz::Path subsetPath(subsetPathStr, "");
                    tinyusdz::Path matPath;
                    const tinyusdz::Material* material{nullptr};
                    std::string err;
                    tinyusdz::tydra::GetBoundMaterial(mStage, subsetPath, "", &matPath, &material, &err);

                    Mesh mesh;
                    auto matIt = mMaterialPathToIndex.find(tinyusdz::to_string(matPath));
                    if (matIt != mMaterialPathToIndex.end())
                        mesh.materialIndex = matIt->second;
                    else
                        LOG_WARN("No material found for subset {}", subsetPathStr);
                    scene->meshes.push_back(mesh);
                }
            }
        }
        else if (const tinyusdz::GeomCamera* geomCamera = node.prim->as<tinyusdz::GeomCamera>())
            extractCamera(geomCamera, worldMatrix, scene);
    }

    for (const auto& child : node.children)
        traverseXformNode(child, scene);
}

void UsdImporter::extractCamera(const tinyusdz::GeomCamera* geomCamera, const tinyusdz::value::matrix4d& worldMatrix, ref<Scene> scene)
{
    tinyusdz::value::point3f origin{0.0f, 0.0f, 0.0f};
    tinyusdz::value::point3f worldOrigin = tinyusdz::transform(worldMatrix, origin);
    float3 cameraPos(worldOrigin[0], worldOrigin[1], worldOrigin[2]);

    // USD cameras look down -Z in local space
    tinyusdz::value::float3 localForwardUsd{0.f, 0.f, -1.f};
    tinyusdz::value::float3 worldForwardUsd = tinyusdz::transform_dir(worldMatrix, localForwardUsd);
    float3 worldForward(worldForwardUsd[0], worldForwardUsd[1], worldForwardUsd[2]);

    float3 cameraTarget = cameraPos + normalize(worldForward);
    // TODO: Calculate FOV from camera properties
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

void UsdImporter::extractMeshGeometry(
    const tinyusdz::GeomMesh* geomMesh,
    const tinyusdz::value::matrix4d& worldMatrix,
    ref<Scene> scene,
    const std::unordered_set<int32_t>* faceFilter
)
{
    auto points = geomMesh->get_points();
    auto faceVertexIndices = geomMesh->get_faceVertexIndices();
    auto faceVertexCounts = geomMesh->get_faceVertexCounts();
    auto normals = geomMesh->get_normals();
    bool hasNormals = !normals.empty();

    // Determine normal interpolation by comparing count: faceVarying == one per face-vertex
    bool normalsFaceVarying = hasNormals && (normals.size() == faceVertexIndices.size());

    // Get UV coordinates from primvars.
    // Use flatten_with_indices so indexed primvars are expanded to one entry per face-vertex.
    std::vector<tinyusdz::value::texcoord2f> uvCoords;
    for (const auto& uvName : {"st", "uv", "st0", "uv0"})
    {
        if (geomMesh->has_primvar(uvName))
        {
            tinyusdz::GeomPrimvar uvPrimvar;
            std::string uvErr;
            if (geomMesh->get_primvar(uvName, &uvPrimvar, &uvErr) && uvPrimvar.flatten_with_indices(&uvCoords, &uvErr))
                break;
        }
    }
    // After flatten_with_indices: face-varying → size == faceVertexIndices.size(), vertex → size == points.size()
    bool uvFaceVarying = uvCoords.size() == faceVertexIndices.size();

    // faceVertexIdx = faceOffset + localIdx (for FaceVarying data like UVs/normals)
    tinyusdz::value::matrix4d normalMatrix = hasNormals ? tinyusdz::inverse(tinyusdz::transpose(worldMatrix)) : tinyusdz::value::matrix4d{};
    auto createVertex = [&](int32_t pointIdx, int32_t faceVertexIdx) -> Vertex
    {
        Vertex v;
        auto pos = tinyusdz::transform(worldMatrix, points[pointIdx]);
        v.position[0] = pos[0];
        v.position[1] = pos[1];
        v.position[2] = pos[2];

        int32_t uvIdx = uvFaceVarying ? faceVertexIdx : pointIdx;
        if (uvIdx < static_cast<int32_t>(uvCoords.size()))
        {
            v.texCoord[0] = uvCoords[uvIdx][0];
            v.texCoord[1] = 1.0f - uvCoords[uvIdx][1]; // USD origin is bottom-left; DX is top-left
        }
        else
        {
            v.texCoord[0] = 0.0f;
            v.texCoord[1] = 0.0f;
        }

        int32_t normalIdx = normalsFaceVarying ? faceVertexIdx : pointIdx;
        if (hasNormals && normalIdx < static_cast<int32_t>(normals.size()))
        {
            auto n = tinyusdz::transform_dir(normalMatrix, normals[normalIdx]);
            n = tinyusdz::vnormalize(n); // re-normalize after transform
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

    size_t faceOffset = 0;
    auto addVertex = [&](int32_t localIdx) -> uint32_t
    {
        int32_t pointIdx = faceVertexIndices[faceOffset + localIdx];
        int32_t faceVertexIdx = static_cast<int32_t>(faceOffset + localIdx);
        scene->vertices.push_back(createVertex(pointIdx, faceVertexIdx));
        uint32_t idx = static_cast<uint32_t>(scene->vertices.size()) - 1;
        scene->indices.push_back(idx);
        return idx;
    };

    uint32_t triangleCount = 0;
    int32_t faceIdx = 0;

    for (int32_t faceVertCount : faceVertexCounts)
    {
        if (!faceFilter || faceFilter->count(faceIdx))
        {
            if (faceVertCount == 3)
            {
                for (int32_t i = 0; i < 3; ++i)
                    addVertex(i);
                triangleCount++;
            }
            else if (faceVertCount == 4)
            {
                addVertex(0);
                addVertex(1);
                addVertex(2);
                addVertex(0);
                addVertex(2);
                addVertex(3);
                triangleCount += 2;
            }
            else
                LOG_WARN("Polygon with {} vertices found, skipping", faceVertCount);
        }

        faceOffset += faceVertCount;
        faceIdx++;
    }

    for (uint32_t i = 0; i < triangleCount; ++i)
        scene->triangleToMesh.push_back(static_cast<uint32_t>(scene->meshes.size()));
}

static UVTransform extractUVTransform(const tinyusdz::tydra::UVTexture& uvTex)
{
    UVTransform t;
    if (uvTex.has_transform2d)
    {
        t.scale = {uvTex.tx_scale[0], uvTex.tx_scale[1]};
        t.offset = {uvTex.tx_translation[0], uvTex.tx_translation[1]};
    }
    return t;
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
        material.baseColorUV = extractUVTransform(mRenderScene.textures[texId]);
    }

    // Metallic
    if (!surfaceShader.metallic.is_texture())
        material.metallicFactor = surfaceShader.metallic.value;
    else
    {
        int32_t texId = surfaceShader.metallic.texture_id;
        material.metallicTextureId = loadTextureFromRenderScene(texId, scene);
        material.metallicUV = extractUVTransform(mRenderScene.textures[texId]);
    }

    // Roughness
    if (!surfaceShader.roughness.is_texture())
        material.roughnessFactor = surfaceShader.roughness.value;
    else
    {
        int32_t texId = surfaceShader.roughness.texture_id;
        material.roughnessTextureId = loadTextureFromRenderScene(texId, scene);
        material.roughnessUV = extractUVTransform(mRenderScene.textures[texId]);
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
        material.emissiveUV = extractUVTransform(mRenderScene.textures[texId]);
        material.emissiveFactor = glm::vec3(1000.f);
    }

    return material;
}

uint32_t UsdImporter::loadTextureFromRenderScene(int32_t textureId, ref<Scene> scene)
{
    const auto& uvTexture = mRenderScene.textures[textureId];
    int channelKey = static_cast<int>(uvTexture.connectedOutputChannel);
    auto cacheKey = std::make_pair(textureId, channelKey);

    auto it = mUsdTextureIdChannelToEngineId.find(cacheKey);
    if (it != mUsdTextureIdChannelToEngineId.end())
        return it->second;

    const auto& texImage = mRenderScene.images[uvTexture.texture_image_id];
    const auto& buffer = mRenderScene.buffers[texImage.buffer_id];

    if (buffer.data.empty())
    {
        LOG_ERROR("Texture buffer is empty for '{}'. Asset may not have been loaded.", texImage.asset_identifier);
        return kInvalidTextureId;
    }

    size_t pixelCount = static_cast<size_t>(texImage.width) * texImage.height * texImage.channels;
    size_t expectedFloat32Size = pixelCount * sizeof(float);
    size_t expectedUint8Size = pixelCount * sizeof(uint8_t);

    const float* floatData = nullptr;
    std::vector<float> convertedData;

    if (buffer.data.size() == expectedFloat32Size)
        floatData = reinterpret_cast<const float*>(buffer.data.data());
    else if (buffer.data.size() == expectedUint8Size)
    {
        convertedData.resize(pixelCount);
        for (size_t i = 0; i < pixelCount; ++i)
            convertedData[i] = buffer.data[i] / 255.0f;
        floatData = convertedData.data();
    }
    else
    {
        LOG_ERROR(
            "Unexpected texture buffer size {} for '{}' ({}x{}x{}) — expected {} (float32) or {} (uint8)",
            buffer.data.size(),
            texImage.asset_identifier,
            texImage.width,
            texImage.height,
            texImage.channels,
            expectedFloat32Size,
            expectedUint8Size
        );
        return kInvalidTextureId;
    }

    using Ch = tinyusdz::tydra::UVTexture::Channel;
    bool isSingleChannel =
        (uvTexture.connectedOutputChannel == Ch::R || uvTexture.connectedOutputChannel == Ch::G || uvTexture.connectedOutputChannel == Ch::B ||
         uvTexture.connectedOutputChannel == Ch::A);

    uint32_t engineTextureId;

    // For scalar parameters (metallic, roughness, etc.) that reference a single channel
    // of a multi-channel texture (e.g., ORM packing), extract that channel into a
    // 1-channel texture so the shader can always sample .r correctly.
    if (isSingleChannel && texImage.channels > 1)
    {
        int srcCh = 0;
        if (uvTexture.connectedOutputChannel == Ch::G)
            srcCh = 1;
        else if (uvTexture.connectedOutputChannel == Ch::B)
            srcCh = 2;
        else if (uvTexture.connectedOutputChannel == Ch::A)
            srcCh = 3;

        float scale = uvTexture.scale[srcCh];
        float bias = uvTexture.bias[srcCh];

        size_t numPixels = static_cast<size_t>(texImage.width) * texImage.height;
        std::vector<float> extracted(numPixels);
        for (size_t i = 0; i < numPixels; ++i)
            extracted[i] = floatData[i * texImage.channels + srcCh] * scale + bias;

        engineTextureId = scene->loadTexture(extracted.data(), texImage.width, texImage.height, 1, texImage.asset_identifier);
    }
    else
        engineTextureId = scene->loadTexture(floatData, texImage.width, texImage.height, texImage.channels, texImage.asset_identifier);

    mUsdTextureIdChannelToEngineId[cacheKey] = engineTextureId;
    return engineTextureId;
}
