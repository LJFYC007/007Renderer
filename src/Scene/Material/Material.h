#pragma once
#include <string>

#include "Utils/Math/Math.h"
#include "Scene/Material/TextureManager.h"

struct Material
{
    // GLTF 2.0 standard
    float3 baseColorFactor = {1.f, 1.f, 1.f};
    float _padding0; // Padding to align float3 to 16 bytes

    float3 emissiveFactor = {1.f, 1.f, 1.f};
    float _padding1; // Padding to align float3 to 16 bytes

    float metallicFactor = 1.f;
    float roughnessFactor = 1.f;

    uint baseColorTextureId = kInvalidTextureId;
    uint metallicTextureId = kInvalidTextureId;
    uint roughnessTextureId = kInvalidTextureId;
    uint emissiveTextureId = kInvalidTextureId;
    uint normalTextureId = kInvalidTextureId;

    Material() = default;
};
