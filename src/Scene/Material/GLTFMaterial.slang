#include "Scene/Material/Material.slang"
#include "Utils/Math/MathConstants.slangh"
import Utils.Sampling.Sample;
import Utils.Sampling.SampleGeneratorInterface;
import GGXMicrofacet;

struct GLTFMetallicBRDF
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
    float3 eval<S : ISampleGenerator>(const float3 wi, const float3 wo, inout S sg)
    {
        float3 h = normalize(wi + wo);
        h *= float(sign(h.z)); // make sure h.z is positive
        if (wi.z < kMinCosTheta || wo.z < kMinCosTheta)
            return float3(0.f);
        if (dot(h, wi) < kMinCosTheta || dot(h, wo) < kMinCosTheta)
            return float3(0.f);

        float alpha2 = alpha * alpha;
        float D = evalNdfGGX(h.z, alpha2);
        float G = evalG1GGX(wi.z, alpha2) * evalG1GGX(abs(wo.z), alpha2);
        float3 F = evalFresnelSchlick(dot(h, wi), albedo);
        return F * D * G / (4.f * wi.z * abs(wo.z));
    }

    bool sample<S : ISampleGenerator>(const float3 wi, out float3 wo, out float pdf, out float3 weight, inout S sg)
    {
        if (wi.z < kMinCosTheta)
            return false;

        // Sample microfacet normal using GGX distribution
        float2 u = sampleNext2D(sg);

        // GGX normal distribution sampling
        float alpha2 = alpha * alpha;
        float cosTheta = sqrt((1.0f - u.x) / (1.0f + (alpha2 - 1.0f) * u.x));
        float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        float phi = TWO_PI * u.y;
        // Construct half vector in local space (normal is (0,0,1))
        float3 h = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
        // Compute reflection direction
        wo = reflect(-wi, h);
        // Check if the reflection is above the surface
        if (wo.z < kMinCosTheta)
            return false;

        // Calculate PDF for the sampled direction
        float D = evalNdfGGX(h.z, alpha2);
        float G1_wi = evalG1GGX(wi.z, alpha2);
        float G1_wo = evalG1GGX(wo.z, alpha2);
        float3 F = evalFresnelSchlick(dot(h, wi), albedo);

        // PDF in terms of solid angle (convert from half-vector PDF)
        pdf = D * h.z / (4.0f * dot(wo, h));
        // Calculate BRDF value
        float3 brdf = F * D * G1_wi * G1_wo * 0.25f / (wi.z * wo.z);
        weight = brdf * wo.z / pdf;
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
        GLTFMetallicBRDF metallicBRDF = GLTFMetallicBRDF(baseColor, roughness * roughness);
        if (!metallicBRDF.sample(wiLocal, woLocal, sample.pdf, sample.weight, sg))
            return false;
        sample.wo = sd.toWorld(woLocal);
        return true;
    }
};