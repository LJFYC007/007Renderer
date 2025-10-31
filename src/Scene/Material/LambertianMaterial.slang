#include "Scene/Material/Material.slang"
#include "Utils/Math/MathConstants.slangh"
import Utils.Sampling.Sample;

// Calculate PDF for uniform hemisphere sampling
float pdfHemisphere(float3 normal, float3 direction)
{
    // For uniform hemisphere sampling, PDF = 1/(2*pi)
    return dot(direction, normal) > 0.0f ? (1.0f / TWO_PI) : 0.0f;
}

struct LambertianMaterial : IMaterial
{
    float3 albedo; // Diffuse color of the material

    __init(float3 color) { albedo = color; }

    float3 getEmissive(float2 uv, Texture2D textures[2], SamplerState sampler)
    {
        return float3(0.0f); // No emission for Lambertian
    }

    BSDFSample scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg, Texture2D textures[2], SamplerState sampler)
    {
        BSDFSample sample;
        sample.wo = normalize(sd.normal + sampleHemisphere(sd.normal, sampleNext2D(sg)));
        sample.pdf = max(dot(sample.wo, sd.normal), 0.0f) * INV_PI;
        sample.weight = albedo; // Lambertian BRDF * cos(theta) / pdf
        return sample;
    }
};