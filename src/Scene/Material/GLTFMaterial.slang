#include "Scene/Material/Material.slang"
#include "Utils/Math/MathConstants.slangh"
import Utils.Sampling.Sample;
import Utils.Sampling.SampleGeneratorInterface;

// Calculate PDF for uniform hemisphere sampling
float pdfHemisphere(float3 normal, float3 direction)
{
    // For uniform hemisphere sampling, PDF = 1/(2*pi)
    return dot(direction, normal) > 0.0f ? (1.0f / TWO_PI) : 0.0f;
}

struct GLTFMaterial
{
    float4 baseColorFactor;
    float metallicFactor = 1.f;
    float roughnessFactor = 0.f;

    // Constructor to initialize the material with a color
    __init(float4 baseColor, float metallic, float roughness)
    {
        baseColorFactor = baseColor;
        metallicFactor = metallic;
        roughnessFactor = roughness;
    }

    // Implement the scatter method for Lambertian reflection
    BSDFSample scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg)
    {
        BSDFSample sample;
        sample.wo = sd.normal + sampleHemisphere(sd.normal, sampleNext2D(sg));
        return sample;
    }
};