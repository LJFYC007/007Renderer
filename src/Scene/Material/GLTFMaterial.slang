#include "Scene/Material/Material.slang"
#include "Utils/Math/MathConstants.slangh"
import Utils.Sampling.Sample;
import Utils.Sampling.SampleGeneratorInterface;
import GGXMicrofacet;

static const float kMinGGXAlpha = 0.0064f;
static const float kMinCosTheta = 1e-6f;

struct GLTFSpecularBRDF
{
    float3 albedo;  // Specular albedo
    float metallic; // Metallic factor
    float alpha;    // GGX parameter (roughness squared)

    // Constructor
    __init(float3 _albedo, float _metallic, float _alpha)
    {
        albedo = _albedo;
        metallic = _metallic;
        alpha = _alpha;
    }

    float3 eval<S : ISampleGenerator>(const float3 wi, const float3 wo, inout S sg)
    {
        float3 h = normalize(wi + wo);
        if (wi.z < kMinCosTheta || wo.z < kMinCosTheta)
            return float3(0.f);
        if (dot(h, wi) < kMinCosTheta || dot(h, wo) < kMinCosTheta)
            return float3(0.f);

        float alpha2 = alpha * alpha;
        float D = evalNdfGGX(h.z, alpha2);
        float G = evalG1GGX(wi.z, alpha2) * evalG1GGX(abs(wo.z), alpha2);
        float3 F = evalFresnelSchlick(dot(h, wi), lerp(0.04, albedo, metallic));
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
        if (wo.z < kMinCosTheta || dot(h, wi) < kMinCosTheta || dot(h, wo) < kMinCosTheta)
            return false;

        // Calculate PDF for the sampled direction
        float D = evalNdfGGX(h.z, alpha2);
        float G1_wi = evalG1GGX(wi.z, alpha2);
        float G1_wo = evalG1GGX(wo.z, alpha2);
        float3 F = evalFresnelSchlick(dot(h, wi), lerp(0.04, albedo, metallic));

        // PDF in terms of solid angle (convert from half-vector PDF)
        pdf = D * h.z / (4.0f * dot(wo, h));
        // Calculate BRDF value
        float3 brdf = F * D * G1_wi * G1_wo * 0.25f / (wi.z * wo.z);
        weight = brdf * wo.z / pdf;
        return true;
    }
}

struct GLTFDiffuseBRDF
{
    float3 albedo;  // Specular albedo
    float metallic; // Metallic factor
    float alpha;    // GGX parameter (roughness squared)

    // Constructor
    __init(float3 _albedo, float _metallic, float _alpha)
    {
        albedo = _albedo;
        metallic = _metallic;
        alpha = _alpha;
    }

    float3 eval<S : ISampleGenerator>(const float3 wi, const float3 wo, inout S sg)
    {
        float3 h = normalize(wi + wo);
        if (wi.z < kMinCosTheta || wo.z < kMinCosTheta)
            return float3(0.f);

        float3 cDiff = lerp(albedo, float3(0.f), metallic);
        float3 F = evalFresnelSchlick(dot(h, wi), lerp(0.04, albedo, metallic));
        return (1.f - F) * (1 / PI) * cDiff;
    }

    bool sample<S : ISampleGenerator>(const float3 wi, out float3 wo, out float pdf, out float3 weight, inout S sg)
    {
        if (wi.z < kMinCosTheta)
            return false;

        float2 u = sampleNext2D(sg);
        float phi = TWO_PI * u.x;
        float r = sqrt(u.y);
        wo = float3(cos(phi) * r, sin(phi) * r, sqrt(1.f - r * r));

        pdf = wo.z * INV_PI;
        float3 cDiff = lerp(albedo, float3(0.f), metallic);
        float3 h = normalize(wi + wo);
        float3 F = evalFresnelSchlick(dot(h, wi), lerp(0.04, albedo, metallic));
        weight = (1.f - F) * (1 / PI) * cDiff * wo.z / pdf;
        return true;
    }
};

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
        float alpha = max(kMinGGXAlpha, roughness * roughness);

        float uSelect = sampleNext1D(sg);
        if (uSelect < metallic)
        {
            GLTFSpecularBRDF specularBRDF = GLTFSpecularBRDF(baseColor, metallic, alpha);
            if (!specularBRDF.sample(wiLocal, woLocal, sample.pdf, sample.weight, sg))
                return false;
            sample.pdf *= metallic;
            sample.weight /= metallic;
        }
        else
        {
            GLTFDiffuseBRDF diffuseBRDF = GLTFDiffuseBRDF(baseColor, metallic, alpha);
            if (!diffuseBRDF.sample(wiLocal, woLocal, sample.pdf, sample.weight, sg))
                return false;
            sample.pdf *= (1.f - metallic);
            sample.weight /= (1.f - metallic);
        }

        sample.wo = sd.toWorld(woLocal);
        return true;
    }
};