Texture2D<float4> input;
RWTexture2D<float4> accumulateTexture;
RWTexture2D<float4> output;

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    uint frameCount;
    bool reset;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint2 id = dispatchThreadID.xy;
    if (id.x >= gWidth || id.y >= gHeight)
        return;
    if (reset == 1)
        accumulateTexture[id] = float4(0.f);

    accumulateTexture[id] += input[id];
    output[id] = accumulateTexture[id] / frameCount;

    // TODO: This is only a temporary solution for gamma correction, we should eventually add a proper tone mapping pass.
    output[id].xyz = pow(output[id].xyz, float3(1.f / 2.2f));
}