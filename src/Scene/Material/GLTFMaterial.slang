#include "Scene/Material/Material.slang"
#include "Scene/Material/TextureSampler.slang"
#include "Utils/Math/MathConstants.slangh"
import GGXMicrofacet;
import Scene.Material.GLTFBSDFs;
import Utils.Sampling.SampleGeneratorInterface;

struct UVTransform
{
    float2 scale;
    float2 offset;
};

// Fully sampled BSDF instance — all textures resolved, lobe weights computed.
// Operates in shading-frame local space (wi/wo in tangent frame).
struct GLTFBSDF
{
    float3 baseColor;
    float metallic;
    float transmission;
    float alpha;
    float eta;
    float baseBudget;
    float pSpecular;
    float pTransmission;
    float pDiffuse;
    float3 wi;

    // Evaluate f(wi, wo) * |cos(theta_o)|.
    float3 eval<S : ISampleGenerator>(float3 wo, inout S sg)
    {
        float3 result = float3(0.f);

        if (wo.z > kMinCosTheta)
        {
            GLTFSpecularBRDF specularBRDF = GLTFSpecularBRDF(baseColor, metallic, alpha, eta);
            result += specularBRDF.eval(wi, wo, sg) * wo.z;

            GLTFDiffuseBRDF diffuseBRDF = GLTFDiffuseBRDF(baseColor, metallic, eta);
            result += (1.f - transmission) * diffuseBRDF.eval(wi, wo, sg) * wo.z;

            if (transmission > 0.f)
            {
                GLTFDielectricBTDF btdf = GLTFDielectricBTDF(alpha, eta);
                result += baseBudget * transmission * btdf.eval(wi, wo) * wo.z;
            }
        }
        else if (wo.z < -kMinCosTheta)
        {
            if (transmission > 0.f)
            {
                GLTFDielectricBTDF btdf = GLTFDielectricBTDF(alpha, eta);
                result += baseBudget * transmission * baseColor * btdf.eval(wi, wo) * (-wo.z);
            }
        }

        return result;
    }

    // Evaluate the mixture solid-angle PDF.
    float evalPdf(float3 wo)
    {
        float pdf = 0.f;
        float alpha2 = alpha * alpha;

        if (wo.z > kMinCosTheta)
        {
            if (pSpecular > 0.f)
            {
                float3 h = normalize(wi + wo);
                if (h.z > 0.f && dot(wi, h) > kMinCosTheta && dot(wo, h) > kMinCosTheta)
                {
                    float D = evalNdfGGX(h.z, alpha2);
                    pdf += pSpecular * D * h.z / (4.f * abs(dot(wo, h)));
                }
            }

            if (pDiffuse > 0.f)
                pdf += pDiffuse * wo.z * INV_PI;

            if (pTransmission > 0.f)
            {
                GLTFDielectricBTDF btdf = GLTFDielectricBTDF(alpha, eta);
                pdf += pTransmission * btdf.evalPdf(wi, wo);
            }
        }
        else if (wo.z < -kMinCosTheta)
        {
            if (pTransmission > 0.f)
            {
                GLTFDielectricBTDF btdf = GLTFDielectricBTDF(alpha, eta);
                pdf += pTransmission * btdf.evalPdf(wi, wo);
            }
        }

        return pdf;
    }

    // Sample a direction. Returns local-space wo in the BSDFSample.
    BSDFSample sample<S : ISampleGenerator>(inout S sg)
    {
        BSDFSample result;
        result.pdf = 0.f;
        result.weight = float3(0.f);
        result.wo = float3(0.f);
        result.eventType = BSDFEventType.Reflection;

        float3 woLocal;
        float uSelect = sampleNext1D(sg);
        float lobePdf;
        float3 lobeWeight;

        if (uSelect < pSpecular)
        {
            GLTFSpecularBRDF specularBRDF = GLTFSpecularBRDF(baseColor, metallic, alpha, eta);
            if (pSpecular > 0.f && specularBRDF.sample(wi, woLocal, lobePdf, lobeWeight, sg))
            {
                result.wo = woLocal;
                result.eventType = BSDFEventType.Reflection;
                result.pdf = lobePdf;
                result.weight = lobeWeight;
            }
        }
        else if (uSelect < pSpecular + pTransmission)
        {
            GLTFDielectricBTDF btdf = GLTFDielectricBTDF(alpha, eta);
            bool isTransmission;
            if (pTransmission > 0.f && btdf.sampleOrTIR(wi, isTransmission, woLocal, lobePdf, lobeWeight, sg))
            {
                result.wo = woLocal;
                result.eventType = isTransmission ? BSDFEventType.Transmission : BSDFEventType.Reflection;
                result.pdf = lobePdf;
                result.weight = lobeWeight;
            }
        }
        else
        {
            GLTFDiffuseBRDF diffuseBRDF = GLTFDiffuseBRDF(baseColor, metallic, eta);
            if (pDiffuse > 0.f && diffuseBRDF.sample(wi, woLocal, lobePdf, lobeWeight, sg))
            {
                result.wo = woLocal;
                result.eventType = BSDFEventType.Reflection;
                result.pdf = lobePdf;
                result.weight = lobeWeight;
            }
        }

        return result;
    }
};

struct GLTFMaterial : IMaterial
{
    // Note: float3 in GPU buffers is padded to 16 bytes (same as float4)
    float3 baseColor;
    float _padding0;

    float3 emissive;
    float _padding1;

    float metallic = 1.f;
    float roughness = 0.f;
    float transmissionFactor = 0.f;
    float ior = 1.5f;

    uint baseColorTextureId;
    uint metallicTextureId;
    uint roughnessTextureId;
    uint emissiveTextureId;
    uint normalTextureId;
    uint transmissionTextureId;

    uint _padding2[2];

    UVTransform baseColorUV;
    UVTransform metallicUV;
    UVTransform roughnessUV;
    UVTransform emissiveUV;
    UVTransform normalUV;
    UVTransform transmissionUV;

    float2 applyUV(float2 uv, UVTransform t) { return uv * t.scale + t.offset; }

    float3 getEmissive(float2 uv) { return sampleEmissive(emissiveTextureId, applyUV(uv, emissiveUV), emissive); }

    void prepareShadingFrame(inout ShadingData sd)
    {
        float3 tsN = sampleNormal(normalTextureId, applyUV(sd.uv, normalUV));

        // Decode tangent-space normal in the material-facing frame.
        // For back-face hits, flip the base B/N first so normal-map detail
        // is interpreted in the same frame we use for shading.
        float3 baseT = sd.T;
        float3 baseB = sd.B;
        float3 baseN = sd.N;
        if (!sd.frontFacing)
        {
            baseB = -baseB;
            baseN = -baseN;
        }

        float3 N = normalize(tsN.x * baseT + tsN.y * baseB + tsN.z * baseN);

        // Re-orthogonalize against sd.tangentW (stable geometric reference) to avoid frame drift
        float3 T = normalize(sd.tangentW.xyz - dot(sd.tangentW.xyz, N) * N);
        float3 B = sd.tangentW.w * cross(N, T);

        sd.N = N;
        sd.T = T;
        sd.B = B;
    }

    GLTFBSDF prepareBSDF(const ShadingData sd)
    {
        GLTFBSDF bsdf;
        bsdf.wi = sd.toLocal(sd.V);

        bsdf.baseColor = sampleBaseColor(baseColorTextureId, applyUV(sd.uv, baseColorUV), baseColor);
        bsdf.metallic = sampleMetallic(metallicTextureId, applyUV(sd.uv, metallicUV), metallic);
        float sampledRoughness = sampleRoughness(roughnessTextureId, applyUV(sd.uv, roughnessUV), roughness);
        bsdf.transmission = sampleTransmission(transmissionTextureId, applyUV(sd.uv, transmissionUV), transmissionFactor);
        bsdf.alpha = max(kMinGGXAlpha, sampledRoughness * sampledRoughness);

        float dielectricWeight = 1.0f - bsdf.metallic;
        bsdf.eta = sd.frontFacing ? (1.0f / ior) : ior;

        // baseBudget = (1-metallic)*(1-macroF): energy reaching the base layer.
        // The BTDF core has no internal Fresnel, so it must be weighted by this
        // to avoid double-counting TIR energy captured by specular micro-Fresnel.
        float macroF = evalFresnelDielectric(bsdf.wi.z, bsdf.eta);
        bsdf.baseBudget = dielectricWeight * (1.0f - macroF);
        bsdf.pSpecular = bsdf.metallic + dielectricWeight * macroF;
        bsdf.pTransmission = bsdf.baseBudget * bsdf.transmission;
        bsdf.pDiffuse = bsdf.baseBudget * (1.0f - bsdf.transmission);

        return bsdf;
    }

    BSDFSample scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg)
    {
        BSDFSample result = prepareBSDF(sd).sample(sg);
        result.wo = sd.toWorld(result.wo);
        return result;
    }
};
