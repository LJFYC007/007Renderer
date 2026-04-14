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

    // Weak furnace test (Heitz 2014 Sec 5.2) uses G1(wi) only;
    // production uses height-correlated G2.
    float evalMasking(float cosThetaI, float cosThetaO, float alpha2)
    {
#ifdef WEAK_WHITE_FURNACE
        return evalG1GGX(cosThetaI, alpha2);
#else
        return evalG2GGX(cosThetaI, cosThetaO, alpha2);
#endif
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
        float G = evalMasking(wi.z, wo.z, alpha2);
        float dielectricF = evalFresnelDielectric(dot(h, wi), eta);
        float3 metallicF = evalFresnelSchlick(dot(h, wi), albedo);
        float3 F = lerp(float3(dielectricF), metallicF, metallic);
        return F * D * G / (4.f * wi.z * wo.z);
    }

    bool sampleGivenHalfVector(const float3 wi, const float3 h, out float3 wo, out float pdf, out float3 weight)
    {
        if (wi.z < kMinCosTheta)
            return false;

        wo = reflect(-wi, h);

        if (dot(h, wi) < kMinCosTheta)
            return false;

#ifndef WEAK_WHITE_FURNACE
        // In weak furnace mode, allow wo below the macrosurface (wo.z < 0).
        // The weak white furnace identity (Heitz 2014 Sec 5.2) integrates over
        // ALL microfacet normals — including those whose reflection exits through
        // the lower hemisphere. The sample weight G1(wi)*dot(wo,h)/(wi.z*h.z) is
        // independent of the sign of wo.z (since dot(wo,h) == dot(wi,h) for
        // reflection), so the estimator is well-defined.
        if (wo.z < kMinCosTheta || dot(h, wo) < kMinCosTheta)
            return false;
#endif

        float alpha2 = alpha * alpha;
        float D = evalNdfGGX(h.z, alpha2);
        float G = evalMasking(wi.z, wo.z, alpha2);
        float dielectricF = evalFresnelDielectric(dot(h, wi), eta);
        float3 metallicF = evalFresnelSchlick(dot(h, wi), albedo);
        float3 F = lerp(float3(dielectricF), metallicF, metallic);

        pdf = D * h.z / (4.0f * dot(wo, h));
        // weight = f(wi,wo) * cos(wo) / pdf
        //        = F * D * G / (4 * wi.z * wo.z) * wo.z / (D * h.z / (4 * woDotH))
        //        = F * G * woDotH / (wi.z * h.z)
        weight = F * G * dot(wo, h) / (wi.z * h.z);
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

    // Evaluate f(wi, wo) for an arbitrary (wi, wo) pair (cosine NOT included).
    // Handles both refraction (wo.z < 0) and total internal reflection (wo.z > 0).
    float3 eval(const float3 wi, const float3 wo)
    {
        if (wi.z < kMinCosTheta)
            return float3(0.f);

        float alpha2 = alpha * alpha;
        float eta2 = eta * eta;

        if (wo.z > kMinCosTheta)
        {
            // Upper hemisphere: possible TIR reflection
            float3 h = normalize(wi + wo);
            if (h.z < 0.f)
                h = -h;
            float wiDotH = dot(wi, h);
            if (wiDotH < kMinCosTheta || dot(wo, h) < kMinCosTheta)
                return float3(0.f);

            // Only contributes if Snell's law gives TIR
            float sin2ThetaT = eta2 * (1.f - wiDotH * wiDotH);
            if (sin2ThetaT < 1.f)
                return float3(0.f);

            float D = evalNdfGGX(h.z, alpha2);
            float G1wi = evalG1GGX(wi.z, alpha2);
            float G1wo = evalG1GGX(wo.z, alpha2);
            // f = (weight * pdf) / |cos_o|, where weight*pdf = D * G1wi * G1wo / (4 * wi.z)
            return float3(D * G1wi * G1wo / (4.f * wi.z * wo.z));
        }

        if (-wo.z < kMinCosTheta)
            return float3(0.f);

        // Lower hemisphere: refraction
        float3 h = normalize(eta * wi + wo);
        if (h.z < 0.f)
            h = -h;

        float wiDotH = dot(wi, h);
        float woDotH = dot(wo, h);
        if (wiDotH < kMinCosTheta || -woDotH < kMinCosTheta)
            return float3(0.f);

        float sin2ThetaT = eta2 * (1.f - wiDotH * wiDotH);
        if (sin2ThetaT >= 1.f)
            return float3(0.f);

        float D = evalNdfGGX(h.z, alpha2);
        float G1wi = evalG1GGX(wi.z, alpha2);
        float G1wo = evalG1GGX(abs(wo.z), alpha2);
        float denom = eta * wiDotH + woDotH;
        if (abs(denom) < kMinCosTheta)
            return float3(0.f);

        return float3((1.f / eta2) * D * G1wi * G1wo * wiDotH * abs(woDotH) / (wi.z * abs(wo.z) * denom * denom));
    }

    // Evaluate the sampling PDF for an arbitrary (wi, wo) pair.
    float evalPdf(const float3 wi, const float3 wo)
    {
        if (wi.z < kMinCosTheta)
            return 0.f;

        float alpha2 = alpha * alpha;
        float eta2 = eta * eta;

        if (wo.z > kMinCosTheta)
        {
            // Upper hemisphere: TIR reflection PDF
            float3 h = normalize(wi + wo);
            if (h.z < 0.f)
                h = -h;
            float wiDotH = dot(wi, h);
            if (wiDotH < kMinCosTheta || dot(wo, h) < kMinCosTheta)
                return 0.f;

            float sin2ThetaT = eta2 * (1.f - wiDotH * wiDotH);
            if (sin2ThetaT < 1.f)
                return 0.f; // Not TIR

            float D = evalNdfGGX(h.z, alpha2);
            return D * h.z / (4.f * wiDotH);
        }

        if (-wo.z < kMinCosTheta)
            return 0.f;

        // Lower hemisphere: refraction PDF
        float3 h = normalize(eta * wi + wo);
        if (h.z < 0.f)
            h = -h;

        float wiDotH = dot(wi, h);
        float woDotH = dot(wo, h);
        if (wiDotH < kMinCosTheta || -woDotH < kMinCosTheta)
            return 0.f;

        float D = evalNdfGGX(h.z, alpha2);
        float denom = eta * wiDotH + woDotH;
        if (abs(denom) < kMinCosTheta)
            return 0.f;
        float jacobian = abs(woDotH) / (denom * denom);
        return D * h.z * jacobian;
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

        float woDotH = dot(wo, h);
        if (-woDotH < kMinCosTheta)
            return false;
        float denom = eta * wiDotH + woDotH;
        if (abs(denom) < kMinCosTheta)
            return false;
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
