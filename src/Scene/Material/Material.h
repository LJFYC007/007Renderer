#pragma once
#include <string>

#include "Utils/Math/Math.h"
#include "Scene/Material/TextureManager.h"

// Per-texture UV transform: uv' = uv * scale + offset
struct UVTransform
{
    float2 scale = {1.f, 1.f};
    float2 offset = {0.f, 0.f};
};

struct Material
{
    // GLTF 2.0 standard
    float3 baseColorFactor = {1.f, 1.f, 1.f};
    float _padding0; // Padding to align float3 to 16 bytes

    float3 emissiveFactor = {0.f, 0.f, 0.f};
    float _padding1; // Padding to align float3 to 16 bytes

    float metallicFactor = 0.f;
    float roughnessFactor = 1.f;
    float transmissionFactor = 0.f;
    float ior = 1.5f;

    uint baseColorTextureId = kInvalidTextureId;
    uint metallicTextureId = kInvalidTextureId;
    uint roughnessTextureId = kInvalidTextureId;
    uint emissiveTextureId = kInvalidTextureId;
    uint normalTextureId = kInvalidTextureId;
    uint transmissionTextureId = kInvalidTextureId;

    uint _padding2[2]; // pad to 16-byte boundary before UV transforms

    UVTransform baseColorUV;
    UVTransform metallicUV;
    UVTransform roughnessUV;
    UVTransform emissiveUV;
    UVTransform normalUV;
    UVTransform transmissionUV;

    Material() = default;
};
