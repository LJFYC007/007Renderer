Texture2D<float4> source;
Texture2D<float4> reference;
RWTexture2D<float4> difference;

cbuffer PerFrameCB
{
    uint gWidth;
    uint gHeight;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint2 id = dispatchThreadID.xy;
    if (id.x >= gWidth || id.y >= gHeight)
        return;
    difference[id] = float4(abs(reference[id].xyz - source[id].xyz), 1.f);
}