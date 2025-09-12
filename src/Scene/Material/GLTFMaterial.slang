#include "Scene/Material/Material.slang"
#include "Utils/Math/MathConstants.slangh"
import Utils.Sampling.Sample;
import Utils.Sampling.SampleGeneratorInterface;
import GGXMicrofacet;

// Calculate PDF for uniform hemisphere sampling
float pdfHemisphere(float3 normal, float3 direction)
{
    // For uniform hemisphere sampling, PDF = 1/(2*pi)
    return dot(direction, normal) > 0.0f ? (1.0f / TWO_PI) : 0.0f;
}

struct GLTFSpecularBRDF
{
    float3 albedo; // Specular albedo
    float alpha;   // GGX parameter (roughness squared)

    // Constructor
    __init(float3 _albedo, float _alpha)
    {
        albedo = _albedo;
        alpha = _alpha;
    }

    // Remember we are in the local space where normal is (0, 0, 1)
    float eval<S : ISampleGenerator>(const float3 wi, const float3 wo, inout S sg)
    {
        float3 h = normalize(wi + wo);
        h *= float(sign(h.z)); // make sure h.z is positive
        if (dot(h, wi) <= 0.f || dot(h, wo) <= 0.f)
            return 0.f;

        float alpha2 = alpha * alpha;
        float D = evalNdfGGX(h.z, alpha2);
        float G = evalG1GGX(wi.z, alpha2) * evalG1GGX(abs(wo.z), alpha2);
        return D * G * 0.25f / wi.z / abs(wo.z);
    }

    bool sample<S : ISampleGenerator>(const float3 wi, out float3 wo, out float pdf, out float3 weight, inout S sg)
    {
        // Sample microfacet normal using GGX distribution
        float2 u = sampleNext2D(sg);

        // GGX visible normal distribution sampling
        float alpha2 = alpha * alpha;
        float cosTheta = sqrt((1.0f - u.x) / (1.0f + (alpha2 - 1.0f) * u.x));
        float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        float phi = TWO_PI * u.y;

        // Construct half vector in local space (normal is (0,0,1))
        float3 h = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

        // Compute reflection direction
        wo = reflect(-wi, h);

        // Check if the reflection is above the surface
        if (wo.z <= 0.0f)
        {
            pdf = 0.0f;
            weight = float3(0.0f);
            return false;
        }

        // Calculate PDF for the sampled direction
        float D = evalNdfGGX(h.z, alpha2);
        float G1_wi = evalG1GGX(wi.z, alpha2);
        float G1_wo = evalG1GGX(wo.z, alpha2);

        // PDF in terms of solid angle (convert from half-vector PDF)
        pdf = D * h.z / (4.0f * dot(wo, h));

        // Calculate BRDF value
        float brdf = D * G1_wi * G1_wo * 0.25f / (wi.z * wo.z);

        // Sample weight = BRDF * cos(theta_o) / PDF
        weight = albedo * brdf * wo.z / pdf;
        return true;
    }
}

struct GLTFMaterial
{
    float3 baseColor;
    float3 emissive;
    float metallic = 1.f;
    float roughness = 0.f;

    // Constructor to initialize the material with a color
    __init(float3 _baseColor, float3 _emissive, float _metallic, float _roughness)
    {
        baseColor = _baseColor;
        emissive = _emissive;
        metallic = _metallic;
        roughness = _roughness;
    }

    // Implement the scatter method for Lambertian reflection
    bool scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg, out BSDFSample sample)
    {
        float3 wiLocal = sd.toLocal(sd.V);
        float3 woLocal;
        GLTFSpecularBRDF specularBRDF = GLTFSpecularBRDF(baseColor, roughness * roughness);
        if (!specularBRDF.sample(wiLocal, woLocal, sample.pdf, sample.weight, sg))
            return false;
        sample.wo = sd.toWorld(woLocal);
        return true;
    }
};