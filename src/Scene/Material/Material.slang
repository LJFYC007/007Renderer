#pragma once

import Utils.Math.Ray;
import Utils.Sampling.SampleGeneratorInterface;
import Scene.ShadingData;

static const uint kInvalidTextureId = 0xFFFFFFFF;
static const uint kMaxTextures = 1024; // Bindless texture array capacity

struct BSDFSample
{
    float3 wo;     // Sampled direction in world space (normalized).
    float pdf;     // pdf with respect to solid angle for the sampled direction (wo).
    float3 weight; // Sample weight f(wi, wo) * dot(wo, n) / pdf(wo).
};

struct BSDFProperties
{
    float3 emission;
};

// Bindless texture array - uses descriptor table in its own register space
struct MaterialTextures
{
    Texture2D textures[kMaxTextures];
};

// Sampler in separate parameter block to avoid mixing with descriptor table
struct MaterialSampler
{
    SamplerState sampler;
};

ParameterBlock<MaterialTextures> gMaterialTextures;
ParameterBlock<MaterialSampler> gMaterialSampler;

// Unified material interface for BSDF evaluation and sampling
interface IMaterial
{
    /** Sample the BSDF and return the scattered direction and weight
        \param[in] sd The shading data
        \param[inout] sg The sample generator
        \return The BSDF sample
    */
    BSDFSample scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg);

    /** Get the emissive contribution
        \param[in] uv The UV coordinates
        \return The emissive contribution
    */
    float3 getEmissive(float2 uv);
}