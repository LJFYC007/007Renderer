#include "Scene/Material/Material.slang"

// Texture sampling utilities that access global material resources via descriptor table
// All texture IDs are validated against kMaxTextures for bounds safety

// Sample base color texture with material's base color multiplier
float3 sampleBaseColor(uint textureId, float2 uv, float3 baseColor)
{
    if (textureId != kInvalidTextureId && textureId < kMaxTextures)
        return gMaterialTextures.textures[textureId].SampleLevel(gMaterialSampler.sampler, uv, 0).rgb * baseColor;
    return baseColor;
}

// Sample metallic texture (red channel for grayscale)
float sampleMetallic(uint textureId, float2 uv, float metallic)
{
    if (textureId != kInvalidTextureId && textureId < kMaxTextures)
        return gMaterialTextures.textures[textureId].SampleLevel(gMaterialSampler.sampler, uv, 0).r * metallic;
    return metallic;
}

// Sample roughness texture (red channel for grayscale)
float sampleRoughness(uint textureId, float2 uv, float roughness)
{
    if (textureId != kInvalidTextureId && textureId < kMaxTextures)
        return gMaterialTextures.textures[textureId].SampleLevel(gMaterialSampler.sampler, uv, 0).r * roughness;
    return roughness;
}

// Sample emissive texture with material's emissive multiplier
float3 sampleEmissive(uint textureId, float2 uv, float3 emissive)
{
    if (textureId != kInvalidTextureId && textureId < kMaxTextures)
        return gMaterialTextures.textures[textureId].SampleLevel(gMaterialSampler.sampler, uv, 0).rgb * emissive;
    return emissive;
}

// Sample normal map texture (returns tangent space normal).
// Only RG channels are used; Z is reconstructed via Z = sqrt(1 - X^2 - Y^2).
// This correctly handles BC5/ATI2 DDS normal maps where the B channel is not
// stored (decompressed as 0), as well as standard RGB normal maps.
float3 sampleNormal(uint textureId, float2 uv)
{
    if (textureId != kInvalidTextureId && textureId < kMaxTextures)
    {
        float2 rg = gMaterialTextures.textures[textureId].SampleLevel(gMaterialSampler.sampler, uv, 0).rg;
        float2 nxy = rg * 2.0f - 1.0f;
        float nz = sqrt(max(0.0f, 1.0f - dot(nxy, nxy)));
        return normalize(float3(nxy, nz));
    }
    return float3(0, 0, 1); // Default tangent space normal
}
