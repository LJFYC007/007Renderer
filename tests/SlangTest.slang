struct Constants
{
    float scalarValue;
    int integerValue;
    bool flagValue;
    float _padding; // Align the following float3 to the next 16-byte boundary
    float3 vectorValue;
};

StructuredBuffer<float> gInputBuffer;
Texture2D<float4> gInputTexture;
ConstantBuffer<Constants> gConstants;
RWStructuredBuffer<float4> gOutputBuffer;

RWTexture2D<float4> gRWTextureOutput;

struct BlockParams
{
    StructuredBuffer<float> values;
    Texture2D<float4> tex;
};
ParameterBlock<BlockParams> gBlockParams;
RWStructuredBuffer<float4> gBlockOutput;

static const uint kBindlessSlotCount = 8;
struct BindlessTextures
{
    Texture2D<float4> textures[kBindlessSlotCount];
};
ParameterBlock<BindlessTextures> gBindless;
RWStructuredBuffer<float4> gBindlessOutput;

[shader("compute")]
[numthreads(8, 8, 1)]
void everythingMain(uint3 dtid: SV_DispatchThreadID)
{
    if (dtid.y == 0 && dtid.x < 4)
    {
        uint i = dtid.x;
        float bufferSample = gInputBuffer[i];
        float4 textureSample = gInputTexture.Load(int3(i, 0, 0));
        float accumulator =
            gConstants.scalarValue + float(gConstants.integerValue) + (gConstants.flagValue ? 1.0f : 0.0f) + gConstants.vectorValue.z;
        gOutputBuffer[i] = float4(bufferSample, textureSample.y, accumulator, gConstants.vectorValue.y);
    }

    gRWTextureOutput[dtid.xy] = float4(float(dtid.x) * 0.125f, float(dtid.y) * 0.125f, 0.25f, 1.0f);

    if (dtid.y == 1 && dtid.x < 4)
    {
        uint i = dtid.x;
        float v = gBlockParams.values[i];
        float4 t = gBlockParams.tex.Load(int3(i, 0, 0));
        gBlockOutput[i] = float4(v, t.r, t.g, t.b);
    }

    if (dtid.y == 2 && dtid.x < 8)
    {
        uint i = dtid.x;
        gBindlessOutput[i] = gBindless.textures[i].Load(int3(0, 0, 0));
    }
}
