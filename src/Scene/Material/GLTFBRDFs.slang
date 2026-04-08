#include "Utils/Math/MathConstants.slangh"
import Utils.Sampling.Sample;
import Utils.Sampling.SampleGeneratorInterface;
import GGXMicrofacet;

// Evaluation and sampling in local (tangent) space where N = (0, 0, 1).

static const float kMinGGXAlpha = 0.0064f;
static const float kMinCosTheta = 1e-6f;

struct GLTFSpecularBRDF
{
    float3 albedo;
    float metallic;
    float alpha; // GGX roughness^2

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

        float2 u = sampleNext2D(sg);
        float alpha2 = alpha * alpha;
        float cosTheta = sqrt((1.0f - u.x) / (1.0f + (alpha2 - 1.0f) * u.x));
        float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
        float phi = TWO_PI * u.y;
        float3 h = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
        wo = reflect(-wi, h);

        if (wo.z < kMinCosTheta || dot(h, wi) < kMinCosTheta || dot(h, wo) < kMinCosTheta)
            return false;

        float D = evalNdfGGX(h.z, alpha2);
        float G1wi = evalG1GGX(wi.z, alpha2);
        float G1wo = evalG1GGX(wo.z, alpha2);
        float3 F = evalFresnelSchlick(dot(h, wi), lerp(0.04, albedo, metallic));

        pdf = D * h.z / (4.0f * dot(wo, h));
        weight = F * D * G1wi * G1wo * 0.25f / (wi.z * wo.z) * wo.z / pdf;
        return true;
    }
};

struct GLTFDiffuseBRDF
{
    float3 albedo;
    float metallic;
    float alpha;

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
        float3 h = normalize(wi + wo);
        float3 cDiff = lerp(albedo, float3(0.f), metallic);
        float3 F = evalFresnelSchlick(dot(h, wi), lerp(0.04, albedo, metallic));
        weight = (1.f - F) * (1 / PI) * cDiff * wo.z / pdf;
        return true;
    }
};
