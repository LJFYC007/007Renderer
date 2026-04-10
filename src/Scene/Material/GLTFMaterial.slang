#include "Scene/Material/Material.slang"
#include "Scene/Material/TextureSampler.slang"
import GGXMicrofacet;
import Scene.Material.GLTFBSDFs;
import Utils.Sampling.SampleGeneratorInterface;

struct UVTransform
{
    float2 scale;
    float2 offset;
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

    BSDFSample scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg)
    {
        BSDFSample sample;
        sample.pdf = 0.0f;
        sample.weight = float3(0.0f);
        sample.wo = float3(0.0f);
        sample.eventType = BSDFEventType.Reflection;

        float3 wiLocal = sd.toLocal(sd.V);
        float3 woLocal;

        float3 sampledBaseColor = sampleBaseColor(baseColorTextureId, applyUV(sd.uv, baseColorUV), baseColor);
        float sampledMetallic = sampleMetallic(metallicTextureId, applyUV(sd.uv, metallicUV), metallic);
        float sampledRoughness = sampleRoughness(roughnessTextureId, applyUV(sd.uv, roughnessUV), roughness);
        float sampledTransmission = sampleTransmission(transmissionTextureId, applyUV(sd.uv, transmissionUV), transmissionFactor);
        float alpha = max(kMinGGXAlpha, sampledRoughness * sampledRoughness);

        // Dielectric F0 derived from IOR: ((ior-1)/(ior+1))^2
        float iorRatio = (ior - 1.0f) / (ior + 1.0f);
        float dielectricF0 = iorRatio * iorRatio;

        float uSelect = sampleNext1D(sg);
        float pdf;
        float3 weight;

        float dielectricWeight = 1.0f - sampledMetallic;
        float eta = sd.frontFacing ? (1.0f / ior) : ior;

        // Dielectric macro-Fresnel steers non-metal energy between the interface
        // and the base layer (diffuse and/or transmission), while metallic energy
        // stays purely specular. Applies to opaque dielectrics too — otherwise the
        // specular highlight on non-metals would never be sampled.
        float cosI = wiLocal.z;
        float sin2T = eta * eta * (1.0f - cosI * cosI);
        float macroF;
        if (sin2T >= 1.0f)
        {
            macroF = 1.0f;
        }
        else
        {
            float cosSchlick = (eta > 1.0f) ? sqrt(1.0f - sin2T) : cosI;
            macroF = schlickPow5(clamp(1.0f - abs(cosSchlick), 0.0f, 1.0f));
            macroF = dielectricF0 + (1.0f - dielectricF0) * macroF;
        }

        float baseBudget = dielectricWeight * (1.0f - macroF);
        float pSpecular = sampledMetallic + dielectricWeight * macroF;
        float pTransmission = baseBudget * sampledTransmission;
        float pDiffuse = baseBudget * (1.0f - sampledTransmission);

        if (uSelect < pSpecular)
        {
            // Specular lobe: the BRDF already blends metallic/dielectric via lerped F0,
            // so its weight is the full specular contribution — no extra mixing needed.
            GLTFSpecularBRDF specularBRDF = GLTFSpecularBRDF(sampledBaseColor, sampledMetallic, alpha, dielectricF0);
            if (pSpecular > 0.0f && specularBRDF.sample(wiLocal, woLocal, pdf, weight, sg))
            {
                sample.pdf = pdf * pSpecular;
                sample.weight = weight / pSpecular;
                sample.wo = sd.toWorld(woLocal);
            }
        }
        else if (uSelect < pSpecular + pTransmission)
        {
            // Transmission lobe: the BTDF returns the core microfacet weight (with Fresnel).
            // We must apply the missing mixing factors: (1-metallic) * transmission * baseColor.
            // For TIR, light is reflected without passing through the medium, so no baseColor tint.
            GLTFDielectricBTDF btdf = GLTFDielectricBTDF(alpha, eta, dielectricF0);
            bool isTransmission;
            if (pTransmission > 0.0f && btdf.sampleOrTIR(wiLocal, isTransmission, woLocal, pdf, weight, sg))
            {
                sample.pdf = pdf * pTransmission;
                sample.wo = sd.toWorld(woLocal);
                if (isTransmission)
                {
                    sample.eventType = BSDFEventType.Transmission;
                    sample.weight = dielectricWeight * sampledTransmission * sampledBaseColor * weight / pTransmission;
                }
                else
                {
                    sample.eventType = BSDFEventType.Reflection;
                    sample.weight = dielectricWeight * sampledTransmission * weight / pTransmission;
                }
            }
        }
        else
        {
            // Diffuse lobe: the BRDF returns (1-F)*cDiff (with cDiff = (1-metallic)*baseColor).
            // We must apply the missing (1-transmission) factor to match the full BSDF.
            GLTFDiffuseBRDF diffuseBRDF = GLTFDiffuseBRDF(sampledBaseColor, sampledMetallic, dielectricF0);
            if (pDiffuse > 0.0f && diffuseBRDF.sample(wiLocal, woLocal, pdf, weight, sg))
            {
                sample.pdf = pdf * pDiffuse;
                sample.weight = (1.0f - sampledTransmission) * weight / pDiffuse;
                sample.wo = sd.toWorld(woLocal);
            }
        }

        return sample;
    }
};
