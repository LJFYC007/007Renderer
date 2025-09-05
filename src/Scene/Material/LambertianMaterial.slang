#include "Scene/Material/Material.slang"
#include "Utils/Math/MathConstants.slangh"
import Utils.Sampling.Sample;

// Calculate PDF for uniform hemisphere sampling
float pdfHemisphere(float3 normal, float3 direction)
{
    // For uniform hemisphere sampling, PDF = 1/(2*pi)
    return dot(direction, normal) > 0.0f ? (1.0f / TWO_PI) : 0.0f;
}

struct LambertianMaterial : Material
{
    float3 albedo; // Diffuse color of the material

    // Constructor to initialize the material with a color
    __init(float3 color) { albedo = color; }

    // Implement the scatter method for Lambertian reflection
    BSDFSample scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg)
    {
        BSDFSample sample;
        sample.wo = sd.normal + sampleHemisphere(sd.normal, sampleNext2D(sg));
        return sample;
    }
};