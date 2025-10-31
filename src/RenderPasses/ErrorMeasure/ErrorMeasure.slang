Texture2D<float4> source;
Texture2D<float4> reference;
RWTexture2D<float4> output;

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
    uint gSelectedOutput;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint2 id = dispatchThreadID.xy;
    if (id.x >= gWidth || id.y >= gHeight)
        return;

    float4 result;
    if (gSelectedOutput == 0) // Source
        result = source[id];
    else if (gSelectedOutput == 1) // Reference
        result = reference[id];
    else // Difference (default)
        result = float4(abs(reference[id].xyz - source[id].xyz), 1.f);

    output[id] = result;
}