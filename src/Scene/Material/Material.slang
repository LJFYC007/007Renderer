#pragma once

import Utils.Sampling.SampleGeneratorInterface;
import Scene.ShadingData;
import Scene.Material.BSDFTypes;

static const uint kInvalidTextureId = 0xFFFFFFFF;
static const uint kMaxTextures = 1024;

// Bindless texture array - uses descriptor table in its own register space
struct MaterialTextures
{
    Texture2D textures[kMaxTextures];
};

// Sampler in separate parameter block to avoid mixing with descriptor table
struct MaterialSampler
{
    SamplerState sampler;
};

ParameterBlock<MaterialTextures> gMaterialTextures;
ParameterBlock<MaterialSampler> gMaterialSampler;

interface IMaterial
{
    /** Apply normal map and back-face flip. Mutates sd.T/B/N in-place.
        sd.faceN and sd.tangentW are immutable geometric references — do not modify them.
    */
    void prepareShadingFrame(inout ShadingData sd);

    /** Sample the BSDF. Must be called after prepareShadingFrame.
        Returns wo in world space with eventType set.
        \param[in] furnaceMode 0 = off, 1 = WeakWhiteFurnace (G1-only masking).
    */
    BSDFSample scatter<S : ISampleGenerator>(const ShadingData sd, inout S sg, uint furnaceMode = 0);

    float3 getEmissive(float2 uv);
}
