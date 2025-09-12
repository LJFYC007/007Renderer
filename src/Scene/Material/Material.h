#pragma once
#include <string>

#include "Utils/Math/Math.h"

struct Material
{
    // std::string name;

    // GLTF 2.0 standard
    float4 baseColorFactor = {1.0f, 0.766f, 0.336f, 1.0f};
    float3 emissiveFactor = {0.f, 0.f, 0.f};
    float metallicFactor = 1.f;
    float roughnessFactor = 0.f;

    Material() = default;
    // explicit Material(const std::string& materialName) : name(materialName) {}
};
