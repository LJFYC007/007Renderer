#include "Scene/Material/Material.slang"
#include "Scene/Material/TextureSampler.slang"
import Scene.Material.GLTFBRDFs;
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

    uint baseColorTextureId;
    uint metallicTextureId;
    uint roughnessTextureId;
    uint emissiveTextureId;
    uint normalTextureId;

    uint _padding2[3];

    UVTransform baseColorUV;
    UVTransform metallicUV;
    UVTransform roughnessUV;
    UVTransform emissiveUV;
    UVTransform normalUV;

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
        float alpha = max(kMinGGXAlpha, sampledRoughness * sampledRoughness);

        float uSelect = sampleNext1D(sg);
        float pdf;
        float3 weight;

        if (uSelect < sampledMetallic)
        {
            GLTFSpecularBRDF specularBRDF = GLTFSpecularBRDF(sampledBaseColor, sampledMetallic, alpha);
            if (specularBRDF.sample(wiLocal, woLocal, pdf, weight, sg))
            {
                sample.pdf = pdf * sampledMetallic;
                sample.weight = weight / sampledMetallic;
                sample.wo = sd.toWorld(woLocal);
            }
        }
        else
        {
            GLTFDiffuseBRDF diffuseBRDF = GLTFDiffuseBRDF(sampledBaseColor, sampledMetallic, alpha);
            if (diffuseBRDF.sample(wiLocal, woLocal, pdf, weight, sg))
            {
                sample.pdf = pdf * (1.f - sampledMetallic);
                sample.weight = weight / (1.f - sampledMetallic);
                sample.wo = sd.toWorld(woLocal);
            }
        }

        return sample;
    }
};
