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
    float eta;   // n_incident / n_transmitted for the dielectric branch

    __init(float3 _albedo, float _metallic, float _alpha, float _eta = (1.0f / 1.5f))
    {
        albedo = _albedo;
        metallic = _metallic;
        alpha = _alpha;
        eta = _eta;
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
        float dielectricF = evalFresnelDielectric(dot(h, wi), eta);
        float3 metallicF = evalFresnelSchlick(dot(h, wi), albedo);
        float3 F = lerp(float3(dielectricF), metallicF, metallic);
        return F * D * G / (4.f * wi.z * abs(wo.z));
    }

    bool sampleGivenHalfVector(const float3 wi, const float3 h, out float3 wo, out float pdf, out float3 weight)
    {
        if (wi.z < kMinCosTheta)
            return false;

        wo = reflect(-wi, h);

        if (wo.z < kMinCosTheta || dot(h, wi) < kMinCosTheta || dot(h, wo) < kMinCosTheta)
            return false;

        float alpha2 = alpha * alpha;
        float D = evalNdfGGX(h.z, alpha2);
        float G1wi = evalG1GGX(wi.z, alpha2);
        float G1wo = evalG1GGX(wo.z, alpha2);
        float dielectricF = evalFresnelDielectric(dot(h, wi), eta);
        float3 metallicF = evalFresnelSchlick(dot(h, wi), albedo);
        float3 F = lerp(float3(dielectricF), metallicF, metallic);

        pdf = D * h.z / (4.0f * dot(wo, h));
        weight = F * D * G1wi * G1wo * 0.25f / (wi.z * wo.z) * wo.z / pdf;
        return true;
    }

    bool sample<S : ISampleGenerator>(const float3 wi, out float3 wo, out float pdf, out float3 weight, inout S sg)
    {
        if (wi.z < kMinCosTheta)
            return false;

        float3 h = sampleGGXHalfVector(sampleNext2D(sg), alpha * alpha);
        return sampleGivenHalfVector(wi, h, wo, pdf, weight);
    }
};

struct GLTFDiffuseBRDF
{
    float3 albedo;
    float metallic;
    float eta;

    __init(float3 _albedo, float _metallic, float _eta = (1.0f / 1.5f))
    {
        albedo = _albedo;
        metallic = _metallic;
        eta = _eta;
    }

    // Evaluate the diffuse BRDF. The Fresnel term uses the exact dielectric
    // interface reflectance because metallic surfaces have zero diffuse.
    float3 eval<S : ISampleGenerator>(const float3 wi, const float3 wo, inout S sg)
    {
        float3 h = normalize(wi + wo);
        if (wi.z < kMinCosTheta || wo.z < kMinCosTheta)
            return float3(0.f);

        float3 cDiff = lerp(albedo, float3(0.f), metallic);
        float3 F = float3(evalFresnelDielectric(dot(h, wi), eta));
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
        float3 F = float3(evalFresnelDielectric(dot(h, wi), eta));
        weight = (1.f - F) * (1 / PI) * cDiff * wo.z / pdf;
        return true;
    }
};

// Dielectric BTDF using GGX microfacet model (Walter et al. 2007).
// Refracts through the surface using Snell's law and reuses the same sampled
// microfacet for total internal reflection so the proposal stays consistent.
struct GLTFDielectricBTDF
{
    float alpha; // GGX roughness^2
    float eta;   // n_incident / n_transmitted

    __init(float _alpha, float _eta)
    {
        alpha = _alpha;
        eta = _eta;
    }

    bool sampleOrTIR<
        S : ISampleGenerator>(const float3 wi, out bool isTransmission, out float3 wo, out float pdf, out float3 weight, inout S sg)
    {
        if (wi.z < kMinCosTheta)
            return false;

        float alpha2 = alpha * alpha;
        float eta2 = eta * eta;
        float3 h = sampleGGXHalfVector(sampleNext2D(sg), alpha2);

        float wiDotH = dot(wi, h);
        if (wiDotH < kMinCosTheta)
            return false;

        float D = evalNdfGGX(h.z, alpha2);
        float G1wi = evalG1GGX(wi.z, alpha2);

        // Snell's law: check for total internal reflection
        float sin2ThetaT = eta2 * (1.0f - wiDotH * wiDotH);
        if (sin2ThetaT >= 1.0f)
        {
            isTransmission = false;
            wo = reflect(-wi, h);
            if (wo.z < kMinCosTheta)
                return false;

            float G1wo = evalG1GGX(wo.z, alpha2);
            pdf = D * h.z / (4.0f * wiDotH);
            weight = float3(G1wi * G1wo * wiDotH / (wi.z * h.z));
            return true;
        }

        float cosThetaT = sqrt(1.0f - sin2ThetaT);

        // Refracted direction through the microfacet
        wo = (eta * wiDotH - cosThetaT) * h - eta * wi;

        if (-wo.z < kMinCosTheta)
            return false;

        isTransmission = true;

        float G1wo = evalG1GGX(abs(wo.z), alpha2);

        // Walter et al. Jacobian: |dH/dwo| = |wo·H| / (eta·wi·H + wo·H)^2
        float woDotH = dot(wo, h);
        float denom = eta * wiDotH + woDotH;
        float jacobian = abs(woDotH) / (denom * denom);

        pdf = D * h.z * jacobian;

        // Non-symmetric radiance transport correction: (n_t/n_i)^2 = 1/eta^2.
        // In a camera-traced path tracer, radiance concentrates in denser media.
        // The dielectric base-layer Fresnel mix is applied by GLTFMaterial, so the
        // BTDF core weight here must not multiply an extra (1 - F) term.
        weight = (1.0f / eta2) * G1wi * G1wo * wiDotH / (wi.z * h.z);
        return true;
    }
};
